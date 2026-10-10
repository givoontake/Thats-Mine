#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <unordered_map>
#include "HitboxData.h"
#include "CharacterPose.h"

using namespace DirectX;

// 파일 형식 버전. 내보내기 도구와 같아야 한다.
constexpr std::uint32_t HITBOX_DATA_VERSION = 4;
constexpr int BONE_NAME_SIZE = 32;
constexpr int TRACK_SOURCE_SIZE = 256;
constexpr int POSE_NAME_SIZE = 32;

// 1차 판정 캡슐을 구할 때 살펴보는 시선 상하 각도(도). 상체 기울기와 고개는 각도에 따라 한쪽으로만 움직이므로 양 끝과 가운데만 본다.
constexpr float CAPSULE_SAMPLE_PITCHES[] = { -45.0f, 0.0f, 60.0f };
// 이동·점프 애니메이션을 진행하며 보는 횟수와 간격(애니메이션 키 간격). 가장 긴 한 바퀴(약 1.07초)를 덮는다.
constexpr int CAPSULE_TIME_STEPS = 32;
constexpr float CAPSULE_TIME_STEP_SECONDS = 1.0f / ANIMATION_SAMPLE_RATE;
// 동작 몽타주를 나눠 보는 횟수
constexpr int CAPSULE_MONTAGE_STEPS = 20;
// 이동 방향을 기준 방향에서 벌려 보는 최대 간격(도)
constexpr float CAPSULE_DIRECTION_STEP = 15.0f;
// 캡슐 크기를 맞출 때 쓰는 후보 구를 나누는 방위 칸(도)과 높이 칸(cm)
constexpr float CAPSULE_SECTOR_DEGREES = 5.0f;
constexpr float CAPSULE_HEIGHT_BIN = 2.0f;
// 훑지 못한 사이 자세를 위한 여유(cm)
constexpr float CAPSULE_SAFETY_MARGIN = 2.0f;

// 축 위치(cx, cy)와 반지름 radius로 구들을 모두 덮을 때 필요한 축 선분 높이 범위와 부피를 구한다. 덮을 수 없으면 부피는 무한대다.
// 구 하나는 축과의 수평 거리 d에서 축 선분을 위아래로 sqrt((R - r)^2 - d^2)만큼 넘어도 들어가므로, 선분 위 끝은 그만큼 내려도 되고 아래 끝은 올려도 된다.
static BroadphaseCapsule FitCapsuleHeight(const std::vector<XMFLOAT4>& spheres, float cx, float cy, float radius, float& out_volume)
{
	BroadphaseCapsule capsule{ cx, cy, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), radius };
	out_volume = std::numeric_limits<float>::max();
	for (const XMFLOAT4& sphere : spheres) {
		const float distance = std::hypot(sphere.x - cx, sphere.y - cy);
		if (distance + sphere.w > radius + 1e-4f) return capsule;
		const float reach = std::sqrt(std::max(0.0f, (radius - sphere.w) * (radius - sphere.w) - distance * distance));
		capsule.top_z = std::max(capsule.top_z, sphere.z - reach);
		capsule.bottom_z = std::min(capsule.bottom_z, sphere.z + reach);
	}
	if (capsule.top_z < capsule.bottom_z) capsule.top_z = capsule.bottom_z = (capsule.top_z + capsule.bottom_z) * 0.5f;
	out_volume = XM_PI * radius * radius * (capsule.top_z - capsule.bottom_z) + 4.0f / 3.0f * XM_PI * radius * radius * radius;
	return capsule;
}

// 축 위치를 정하면 반지름은 가장 바깥 구가 닿는 값부터 커질수록 선분이 짧아지므로, 부피가 가장 작은 반지름을 황금 분할로 찾는다.
static BroadphaseCapsule FitCapsuleAt(const std::vector<XMFLOAT4>& spheres, float cx, float cy, float& out_volume)
{
	float min_radius = 0.0f;
	for (const XMFLOAT4& sphere : spheres) min_radius = std::max(min_radius, std::hypot(sphere.x - cx, sphere.y - cy) + sphere.w);
	const float golden = 0.618034f;
	float low = min_radius;
	float high = min_radius + 100.0f;
	for (int step = 0; step < 40; ++step) {
		const float first = high - (high - low) * golden;
		const float second = low + (high - low) * golden;
		float first_volume = 0.0f;
		float second_volume = 0.0f;
		FitCapsuleHeight(spheres, cx, cy, first, first_volume);
		FitCapsuleHeight(spheres, cx, cy, second, second_volume);
		if (first_volume < second_volume) high = second;
		else low = first;
	}
	return FitCapsuleHeight(spheres, cx, cy, (low + high) * 0.5f, out_volume);
}

// 판정용 자세를 여러 상태(무기, 조준, 시선 상하, 이동 방향·속도·주기, 공중, 동작 몽타주)로 훑어, 뼈별 정점 구와 히트박스를 모두 덮는 가장 작은 세로 캡슐을 구한다.
// 정점은 묶인 뼈들이 각자 옮긴 위치의 가중 평균에 놓이므로, 뼈별 정점 구를 덮는 볼록한 캡슐은 메시도 덮는다.
static BroadphaseCapsule ComputeBroadphaseCapsule(const HitboxData& data)
{
	// 훑은 구 전체(액터 기준 중심 xyz와 반지름)와, 크기를 맞출 때 쓰는 후보(방위·높이 칸마다 축에서 가장 멀리 나간 구)
	std::vector<XMFLOAT4> spheres;
	std::unordered_map<long long, XMFLOAT4> candidates;
	const auto add_sphere = [&](FXMVECTOR center, float radius)
	{
		XMFLOAT4 sphere;
		XMStoreFloat4(&sphere, XMVectorSetW(center, radius));
		spheres.push_back(sphere);
		const long long sector = static_cast<long long>(std::floor(XMConvertToDegrees(std::atan2(sphere.y, sphere.x)) / CAPSULE_SECTOR_DEGREES));
		const long long height = static_cast<long long>(std::floor(sphere.z / CAPSULE_HEIGHT_BIN));
		XMFLOAT4& candidate = candidates.try_emplace(sector * 100000 + height, sphere).first->second;
		if (std::hypot(sphere.x, sphere.y) + sphere.w > std::hypot(candidate.x, candidate.y) + candidate.w) candidate = sphere;
	};
	const auto collect = [&](const CharacterPose& pose, const MOVE_STATE& state, long long time_ms)
	{
		const std::vector<BoneTransform> bones = pose.BuildBonePoses(state, time_ms);
		for (int bone = 0; bone < static_cast<int>(bones.size()); ++bone) {
			const BoneSphere& sphere = data.GetBoneSphere(bone);
			if (sphere.radius <= 0.0f) continue;
			add_sphere(XMVectorAdd(XMLoadFloat3(&bones[bone].position), XMVector3Rotate(XMLoadFloat3(&sphere.center), XMLoadFloat4(&bones[bone].rotation))), sphere.radius);
		}
		// 히트박스가 메시 밖으로 조금 나온 곳(머리 구 앞쪽 등)도 덮는다. 캡슐은 양 끝 구로, 상자는 꼭짓점으로 넣는다.
		for (const HitboxBody& body : data.GetBodies()) {
			const XMVECTOR bone_rotation = XMLoadFloat4(&bones[body.bone].rotation);
			const XMVECTOR bone_position = XMLoadFloat3(&bones[body.bone].position);
			for (const HitboxShape& shape : body.shapes) {
				const XMVECTOR center = XMVectorAdd(bone_position, XMVector3Rotate(XMLoadFloat3(&shape.transform.position), bone_rotation));
				const XMVECTOR rotation = XMQuaternionMultiply(XMLoadFloat4(&shape.transform.rotation), bone_rotation);
				if (shape.type == HitboxShapeType::SPHERE) {
					add_sphere(center, shape.radius);
				}
				else if (shape.type == HitboxShapeType::CAPSULE) {
					const XMVECTOR axis = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, shape.length * 0.5f, 0.0f), rotation);
					add_sphere(XMVectorAdd(center, axis), shape.radius);
					add_sphere(XMVectorSubtract(center, axis), shape.radius);
				}
				else {
					for (const float x : { -1.0f, 1.0f }) for (const float y : { -1.0f, 1.0f }) for (const float z : { -1.0f, 1.0f }) {
						add_sphere(XMVectorAdd(center, XMVector3Rotate(XMVectorSet(x * shape.half_size.x, y * shape.half_size.y, z * shape.half_size.z, 0.0f), rotation)), 0.0f);
					}
				}
			}
		}
	};

	const long long start_ms = 1000;
	for (std::uint8_t weapon = 0; weapon < data.GetWeaponCount(); ++weapon) {
		for (const bool is_aiming : { false, true }) {
			if (is_aiming && data.GetWeaponInfo(weapon)->pose_type != WeaponPoseType::RIFLE) continue;
			for (const float pitch : CAPSULE_SAMPLE_PITCHES) {
				MOVE_STATE base{};
				base.weapon = weapon;
				base.is_aiming = is_aiming;
				base.view_pitch = pitch;
				const auto run = [&](const MOVE_STATE& state, const MOVE_STATE* cardinal_state)
				{
					CharacterPose pose;
					pose.Reset(data);
					long long time_ms = start_ms;
					// 기준 방향을 먼저 정해 두고, 그 기준을 유지하는 범위 안의 실제 이동 방향으로 진행한다.
					if (cardinal_state) pose.Update(*cardinal_state, 0.0f, time_ms);
					for (int step = 0; step < CAPSULE_TIME_STEPS; ++step) {
						time_ms += static_cast<long long>(CAPSULE_TIME_STEP_SECONDS * 1000.0f);
						pose.Update(state, CAPSULE_TIME_STEP_SECONDS, time_ms);
						collect(pose, state, time_ms);
					}
				};
				// 서 있기와 공중
				run(base, nullptr);
				MOVE_STATE falling = base;
				falling.is_falling = true;
				falling.velocity[0] = 300.0f;
				run(falling, nullptr);
				// 기준 방향마다 그 기준을 유지하는 방향 범위(앞뒤 ±77.5도, 옆 ±32.5도)와, 그 줄의 샘플 속도·사이 속도를 훑는다.
				for (const float cardinal : { 0.0f, 180.0f, -90.0f, 90.0f }) {
					const float half_range = (cardinal == 0.0f || cardinal == 180.0f ? 67.5f : 22.5f) + 10.0f;
					std::vector<float> row_speeds;
					for (const LocomotionSample& sample : data.GetLocomotionSamples()) {
						if (std::abs(std::remainder(sample.direction - cardinal, 360.0f)) < 1.0f && sample.speed > 0.0f) row_speeds.push_back(sample.speed);
					}
					std::sort(row_speeds.begin(), row_speeds.end());
					row_speeds.erase(std::unique(row_speeds.begin(), row_speeds.end()), row_speeds.end());
					std::vector<float> speeds;
					float previous = 0.0f;
					for (const float speed : row_speeds) {
						speeds.push_back((previous + speed) * 0.5f);
						speeds.push_back(speed);
						previous = speed;
					}
					const int direction_count = static_cast<int>(std::ceil(half_range * 2.0f / CAPSULE_DIRECTION_STEP));
					for (int direction_index = 0; direction_index <= direction_count; ++direction_index) {
						const float direction = XMConvertToRadians(cardinal - half_range + half_range * 2.0f * direction_index / direction_count);
						for (const float speed : speeds) {
							MOVE_STATE cardinal_state = base;
							cardinal_state.velocity[0] = speed * std::cos(XMConvertToRadians(cardinal));
							cardinal_state.velocity[1] = speed * std::sin(XMConvertToRadians(cardinal));
							MOVE_STATE state = base;
							state.velocity[0] = speed * std::cos(direction);
							state.velocity[1] = speed * std::sin(direction);
							run(state, &cardinal_state);
						}
					}
				}
				// 동작 몽타주: 서 있거나 앞으로 달리는 하체 위에 처음부터 끝까지 재생한다.
				for (const MontageInfo& montage : data.GetMontages()) {
					if (montage.weapon != weapon && montage.weapon != ANY_WEAPON) continue;
					const float duration = data.GetTrack(montage.track).length / std::max(montage.play_rate, 0.01f);
					for (const float speed : { 0.0f, 600.0f }) {
						MOVE_STATE state = base;
						state.velocity[0] = speed;
						CharacterPose pose;
						pose.Reset(data);
						pose.Update(state, 0.0f, start_ms);
						pose.StartAction(montage.action, weapon, start_ms);
						for (int step = 0; step <= CAPSULE_MONTAGE_STEPS; ++step) {
							collect(pose, state, start_ms + static_cast<long long>(duration * 1000.0f * step / CAPSULE_MONTAGE_STEPS));
						}
					}
				}
			}
		}
	}

	// 후보 구로 축 위치를 찾고, 마지막에 훑은 구 전체로 반지름과 높이 범위를 다시 맞춰 하나도 빠지지 않게 한다.
	std::vector<XMFLOAT4> fit_spheres;
	for (const auto& [key, sphere] : candidates) fit_spheres.push_back(sphere);
	float best_volume = 0.0f;
	BroadphaseCapsule best = FitCapsuleAt(fit_spheres, 0.0f, 0.0f, best_volume);
	for (float step = 8.0f; step > 0.2f; step *= 0.5f) {
		bool is_improved = true;
		while (is_improved) {
			is_improved = false;
			for (const auto& [dx, dy] : { std::pair{ 1.0f, 0.0f }, std::pair{ -1.0f, 0.0f }, std::pair{ 0.0f, 1.0f }, std::pair{ 0.0f, -1.0f } }) {
				float volume = 0.0f;
				const BroadphaseCapsule trial = FitCapsuleAt(fit_spheres, best.offset_x + dx * step, best.offset_y + dy * step, volume);
				if (volume < best_volume) {
					best = trial;
					best_volume = volume;
					is_improved = true;
				}
			}
		}
	}
	float radius = best.radius;
	for (const XMFLOAT4& sphere : spheres) radius = std::max(radius, std::hypot(sphere.x - best.offset_x, sphere.y - best.offset_y) + sphere.w);
	float volume = 0.0f;
	BroadphaseCapsule capsule = FitCapsuleHeight(spheres, best.offset_x, best.offset_y, radius, volume);
	capsule.radius += CAPSULE_SAFETY_MARGIN;
	return capsule;
}

bool HitboxData::Load(const std::string& file_path)
{
	std::ifstream file(file_path, std::ios::binary);
	if (!file.is_open()) return false;
	const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	// 파일 끝을 넘어 읽으면 실패로 끝낸다.
	size_t offset = 0;
	bool is_valid = true;
	const auto read = [&](void* destination, size_t size)
	{
		if (!is_valid || offset + size > bytes.size()) {
			is_valid = false;
			return;
		}
		std::memcpy(destination, bytes.data() + offset, size);
		offset += size;
	};
	const auto read_value = [&]<typename T>(T& value) { read(&value, sizeof(T)); };
	const auto read_name = [&](size_t size)
	{
		std::string name(size, '\0');
		read(name.data(), size);
		name.resize(std::strlen(name.c_str()));
		return name;
	};
	const auto read_transform = [&]()
	{
		BoneTransform transform{};
		read(&transform.rotation, sizeof(transform.rotation));
		read(&transform.position, sizeof(transform.position));
		return transform;
	};

	char magic[4] = {};
	read(magic, sizeof(magic));
	std::uint32_t version = 0;
	read_value(version);
	if (!is_valid || std::memcmp(magic, "TMHB", sizeof(magic)) != 0 || version != HITBOX_DATA_VERSION) return false;

	std::uint32_t bone_count = 0;
	read_value(bone_count);
	bone_names_.clear();
	bone_parents_.clear();
	for (std::uint32_t bone = 0; bone < bone_count && is_valid; ++bone) {
		bone_names_.push_back(read_name(BONE_NAME_SIZE));
		int parent = -1;
		read_value(parent);
		bone_parents_.push_back(parent);
	}
	read_value(head_pitch_up_limit_);
	read_value(head_pitch_down_limit_);

	std::uint32_t weapon_count = 0;
	read_value(weapon_count);
	weapon_infos_.clear();
	for (std::uint32_t weapon = 0; weapon < weapon_count && is_valid; ++weapon) {
		WeaponInfo info{};
		std::uint8_t is_hitscan = 0;
		read_value(info.pose_type);
		read_value(info.aim_pitch_up_limit);
		read_value(info.aim_pitch_down_limit);
		read_value(is_hitscan);
		read_value(info.hitscan_distance);
		read_value(info.magazine_capacity);
		read_value(info.reload_duration);
		read_value(info.fire_interval);
		info.is_hitscan = is_hitscan != 0;
		weapon_infos_.push_back(info);
	}

	std::uint32_t body_count = 0;
	read_value(body_count);
	bodies_.clear();
	for (std::uint32_t index = 0; index < body_count && is_valid; ++index) {
		HitboxBody body{};
		HitboxPart part = HitboxPart::TORSO;
		std::uint32_t shape_count = 0;
		read_value(body.bone);
		read_value(part);
		read_value(shape_count);
		for (std::uint32_t shape_index = 0; shape_index < shape_count && is_valid; ++shape_index) {
			HitboxShape shape{};
			shape.part = part;
			read_value(shape.type);
			shape.transform = read_transform();
			read_value(shape.radius);
			read_value(shape.length);
			read_value(shape.half_size);
			body.shapes.push_back(shape);
		}
		bodies_.push_back(std::move(body));
	}

	std::uint32_t track_count = 0;
	read_value(track_count);
	tracks_.clear();
	for (std::uint32_t index = 0; index < track_count && is_valid; ++index) {
		AnimationTrack track{};
		read_name(TRACK_SOURCE_SIZE);
		float start_time = 0.0f;
		std::uint32_t key_count = 0;
		read_value(start_time);
		read_value(track.length);
		read_value(key_count);
		track.key_count = static_cast<int>(key_count);
		for (std::uint32_t key = 0; key < key_count * bone_count && is_valid; ++key) {
			track.keys.push_back(read_transform());
		}
		tracks_.push_back(std::move(track));
	}

	std::uint32_t sample_count = 0;
	read_value(sample_count);
	locomotion_samples_.clear();
	for (std::uint32_t index = 0; index < sample_count && is_valid; ++index) {
		LocomotionSample sample{};
		std::uint32_t track = 0;
		read_value(sample.speed);
		read_value(sample.direction);
		read_value(track);
		read_value(sample.rate_scale);
		sample.track = static_cast<int>(track);
		locomotion_samples_.push_back(sample);
	}

	std::uint32_t pose_count = 0;
	read_value(pose_count);
	pose_tracks_.clear();
	for (std::uint32_t index = 0; index < pose_count && is_valid; ++index) {
		read_name(POSE_NAME_SIZE);
		std::uint32_t track = 0;
		read_value(track);
		pose_tracks_.push_back(static_cast<int>(track));
	}
	std::uint32_t jump_track = 0;
	read_value(jump_track);
	jump_track_ = static_cast<int>(jump_track);

	std::uint32_t montage_count = 0;
	read_value(montage_count);
	montages_.clear();
	for (std::uint32_t index = 0; index < montage_count && is_valid; ++index) {
		MontageInfo montage{};
		std::uint32_t track = 0;
		read_value(montage.action);
		read_value(montage.weapon);
		read_value(track);
		read_value(montage.play_rate);
		read_value(montage.blend_in);
		read_value(montage.blend_out);
		read_value(montage.region);
		montage.track = static_cast<int>(track);
		montages_.push_back(montage);
	}

	std::uint32_t sphere_count = 0;
	read_value(sphere_count);
	bone_spheres_.clear();
	for (std::uint32_t index = 0; index < sphere_count && is_valid; ++index) {
		BoneSphere sphere{};
		read_value(sphere.center);
		read_value(sphere.radius);
		bone_spheres_.push_back(sphere);
	}

	hips_bone_ = FindBone("hips");
	spine_bone_ = FindBone("spine");
	chest_bone_ = FindBone("chest");
	head_bone_ = FindBone("head");
	upperarm_r_bone_ = FindBone("upperarm_r");
	const bool has_bones = hips_bone_ >= 0 && spine_bone_ >= 0 && chest_bone_ >= 0 && head_bone_ >= 0 && upperarm_r_bone_ >= 0;
	// 다른 항목이 가리키는 트랙·뼈 번호가 범위 안인지 확인한다.
	const auto is_track = [this](int track) { return track >= 0 && track < static_cast<int>(tracks_.size()) && tracks_[track].key_count > 0; };
	bool has_valid_indices = is_track(jump_track_);
	for (const int track : pose_tracks_) has_valid_indices = has_valid_indices && is_track(track);
	for (const LocomotionSample& sample : locomotion_samples_) has_valid_indices = has_valid_indices && is_track(sample.track);
	for (const MontageInfo& montage : montages_) has_valid_indices = has_valid_indices && is_track(montage.track);
	for (const HitboxBody& body : bodies_) has_valid_indices = has_valid_indices && body.bone >= 0 && body.bone < static_cast<int>(bone_count);
	for (size_t bone = 0; bone < bone_parents_.size(); ++bone) has_valid_indices = has_valid_indices && bone_parents_[bone] < static_cast<int>(bone);
	if (!is_valid || !has_bones || !has_valid_indices || pose_tracks_.size() != static_cast<size_t>(UpperBodyPose::COUNT) || bone_spheres_.size() != bone_count) {
		tracks_.clear();
		return false;
	}
	broadphase_capsule_ = ComputeBroadphaseCapsule(*this);
	return true;
}

int HitboxData::FindBone(const std::string& name) const
{
	for (size_t bone = 0; bone < bone_names_.size(); ++bone) {
		if (bone_names_[bone] == name) return static_cast<int>(bone);
	}
	return -1;
}

bool HitboxData::IsInBranch(int bone, int ancestor) const
{
	for (int current = bone; current >= 0; current = bone_parents_[current]) {
		if (current == ancestor) return true;
	}
	return false;
}

const MontageInfo* HitboxData::FindMontage(std::uint8_t action, std::uint8_t weapon) const
{
	for (const MontageInfo& montage : montages_) {
		if (montage.action == action && (montage.weapon == weapon || montage.weapon == ANY_WEAPON)) return &montage;
	}
	return nullptr;
}
