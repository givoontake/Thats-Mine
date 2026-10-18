#include <algorithm>
#include <cmath>
#include "CharacterPose.h"
#include "HitboxData.h"

using namespace DirectX;

// 기준 방향을 바꾸지 않고 유지하는 여유 각도. 경계 근처에서 기준 방향이 떨리지 않게 한다.
constexpr float CARDINAL_HYSTERESIS_DEGREES = 10.0f;
// 이보다 느리면 기준 방향을 바꾸지 않고 하체도 돌리지 않는다.
constexpr float MIN_LOCOMOTION_SPEED = 10.0f;
// 조준 상하 각도를 허리와 가슴에 나눠 주는 비율
constexpr float SPINE_AIM_RATIO = 0.35f;
constexpr float CHEST_AIM_RATIO = 0.65f;

// XMQuaternionMultiply(a, b)는 a를 먼저, b를 나중에 적용한 회전이다. 부모 회전 P 아래의 자식 회전 C는 XMQuaternionMultiply(C, P)다.

static float GetCardinalAngle(LocomotionCardinal cardinal)
{
	switch (cardinal) {
	case LocomotionCardinal::BACKWARD: return 180.0f;
	case LocomotionCardinal::LEFT: return -90.0f;
	case LocomotionCardinal::RIGHT: return 90.0f;
	default: return 0.0f;
	}
}

// from에서 to까지 가장 짧게 도는 각도(-180~180도)
static float GetDeltaAngle(float from, float to)
{
	float delta = std::fmod(to - from, 360.0f);
	if (delta > 180.0f) delta -= 360.0f;
	if (delta < -180.0f) delta += 360.0f;
	return delta;
}

// 앞뒤는 대각선까지 포함하고(±67.5도), 옆은 정면 옆 근처(±22.5도)만 맡는다.
static bool IsInCardinal(LocomotionCardinal cardinal, float direction, float margin)
{
	const float half_range = cardinal == LocomotionCardinal::FORWARD || cardinal == LocomotionCardinal::BACKWARD ? 67.5f : 22.5f;
	return std::abs(GetDeltaAngle(GetCardinalAngle(cardinal), direction)) <= half_range + margin;
}

static float GetMoveSpeed(const MOVE_STATE& move_state)
{
	return std::hypot(move_state.velocity[0], move_state.velocity[1]);
}

// 캐릭터가 바라보는 방향 기준 이동 방향(도). 오른쪽이 +다.
static float GetMoveDirection(const MOVE_STATE& move_state)
{
	const float yaw = XMConvertToRadians(move_state.yaw);
	const float forward = move_state.velocity[0] * std::cos(yaw) + move_state.velocity[1] * std::sin(yaw);
	const float right = -move_state.velocity[0] * std::sin(yaw) + move_state.velocity[1] * std::cos(yaw);
	return XMConvertToDegrees(std::atan2(right, forward));
}

// 기준 방향 줄의 블렌드 스페이스 샘플 중 속도를 사이에 둔 두 샘플과, 뒤 샘플 쪽 비율을 구한다.
struct LocomotionBlend {
	int first;
	int second;
	float alpha;
};

static LocomotionBlend GetLocomotionBlend(const HitboxData& data, LocomotionCardinal cardinal, float speed)
{
	const std::vector<LocomotionSample>& samples = data.GetLocomotionSamples();
	std::vector<int> row;
	for (int index = 0; index < static_cast<int>(samples.size()); ++index) {
		const bool is_row = std::abs(GetDeltaAngle(GetCardinalAngle(cardinal), samples[index].direction)) < 1.0f;
		const bool is_duplicate = std::any_of(row.begin(), row.end(), [&](int other) { return samples[other].speed == samples[index].speed; });
		if (is_row && !is_duplicate) row.push_back(index);
	}
	std::sort(row.begin(), row.end(), [&](int a, int b) { return samples[a].speed < samples[b].speed; });
	if (row.empty()) return { 0, 0, 0.0f };
	if (speed <= samples[row.front()].speed) return { row.front(), row.front(), 0.0f };
	for (size_t index = 1; index < row.size(); ++index) {
		const LocomotionSample& low = samples[row[index - 1]];
		const LocomotionSample& high = samples[row[index]];
		if (speed <= high.speed) return { row[index - 1], row[index], (speed - low.speed) / (high.speed - low.speed) };
	}
	return { row.back(), row.back(), 0.0f };
}

// 샘플 하나의 한 바퀴 시간(초). 재생 속도 배율만큼 짧아진다.
static float GetSampleCycle(const HitboxData& data, int sample)
{
	const LocomotionSample& entry = data.GetLocomotionSamples()[sample];
	return data.GetTrack(entry.track).length / std::max(entry.rate_scale, 0.01f);
}

// 두 회전을 섞는다. 같은 회전의 반대 부호 표현이면 뒤집어 짧은 쪽으로 섞고, 길이를 1로 맞춘다.
static XMVECTOR BlendRotation(FXMVECTOR from, FXMVECTOR to, float alpha)
{
	const XMVECTOR target = XMVectorGetX(XMQuaternionDot(from, to)) < 0.0f ? XMVectorNegate(to) : to;
	return XMQuaternionNormalize(XMVectorLerp(from, target, alpha));
}

static std::vector<BoneTransform> BlendPoses(const std::vector<BoneTransform>& from, const std::vector<BoneTransform>& to, float alpha)
{
	std::vector<BoneTransform> result(from.size());
	for (size_t bone = 0; bone < from.size(); ++bone) {
		XMStoreFloat4(&result[bone].rotation, BlendRotation(XMLoadFloat4(&from[bone].rotation), XMLoadFloat4(&to[bone].rotation), alpha));
		XMStoreFloat3(&result[bone].position, XMVectorLerp(XMLoadFloat3(&from[bone].position), XMLoadFloat3(&to[bone].position), alpha));
	}
	return result;
}

// 트랙의 time 시각 자세를 앞뒤 키를 섞어 구한다. 반복 재생이면 길이로 나눈 나머지를 쓰고, 아니면 끝 키에서 멈춘다.
static std::vector<BoneTransform> SampleTrack(const AnimationTrack& track, int bone_count, float time, bool is_looping)
{
	const float local_time = is_looping && track.length > 0.0f ? std::fmod(std::max(time, 0.0f), track.length) : std::clamp(time, 0.0f, track.length);
	const float frame = std::min(local_time * ANIMATION_SAMPLE_RATE, static_cast<float>(track.key_count - 1));
	const int index = static_cast<int>(frame);
	const int next = std::min(index + 1, track.key_count - 1);
	const std::vector<BoneTransform> first(track.keys.begin() + index * bone_count, track.keys.begin() + (index + 1) * bone_count);
	const std::vector<BoneTransform> second(track.keys.begin() + next * bone_count, track.keys.begin() + (next + 1) * bone_count);
	return BlendPoses(first, second, frame - index);
}

// 뼈의 부모 기준 자세를 루트부터 차례로 곱해 액터 기준 자세를 구한다. 뼈는 부모가 먼저 오는 순서다.
static std::vector<BoneTransform> ComposePose(const HitboxData& data, const std::vector<BoneTransform>& locals)
{
	std::vector<BoneTransform> poses(locals.size());
	for (int bone = 0; bone < static_cast<int>(locals.size()); ++bone) {
		const int parent = data.GetBoneParent(bone);
		if (parent < 0) {
			poses[bone] = locals[bone];
			continue;
		}
		const XMVECTOR parent_rotation = XMLoadFloat4(&poses[parent].rotation);
		XMStoreFloat4(&poses[bone].rotation, XMQuaternionMultiply(XMLoadFloat4(&locals[bone].rotation), parent_rotation));
		XMStoreFloat3(&poses[bone].position, XMVectorAdd(XMLoadFloat3(&poses[parent].position), XMVector3Rotate(XMLoadFloat3(&locals[bone].position), parent_rotation)));
	}
	return poses;
}

// 단위 벡터 from을 to로 가장 짧게 돌리는 회전
static XMVECTOR FindBetween(FXMVECTOR from, FXMVECTOR to)
{
	const float dot = XMVectorGetX(XMVector3Dot(from, to));
	if (dot < -0.9999f) {
		const XMVECTOR axis = XMVector3Normalize(XMVector3Cross(from, std::abs(XMVectorGetX(from)) < 0.9f ? g_XMIdentityR0 : g_XMIdentityR1));
		return XMQuaternionRotationNormal(axis, XM_PI);
	}
	return XMQuaternionNormalize(XMVectorSetW(XMVector3Cross(from, to), 1.0f + dot));
}

void CharacterPose::Reset(const HitboxData& data)
{
	data_ = &data;
	cardinal_ = LocomotionCardinal::FORWARD;
	locomotion_phase_ = 0.0f;
	jump_time_ = 0.0f;
	weapon_ = 0;
	has_weapon_ = false;
	montage_ = nullptr;
	montage_start_ms_ = 0;
}

void CharacterPose::Update(const MOVE_STATE& move_state, float delta_seconds, long long time_ms)
{
	if (!data_ || !data_->IsLoaded()) return;
	if (has_weapon_ && move_state.weapon != weapon_) StartAction(ACTION_EQUIP, move_state.weapon, time_ms);
	weapon_ = move_state.weapon;
	has_weapon_ = true;
	if (montage_) {
		const float elapsed = (time_ms - montage_start_ms_) / 1000.0f;
		if (elapsed * montage_->play_rate >= data_->GetTrack(montage_->track).length) montage_ = nullptr;
	}

	if (move_state.air_phase != AirPhase::GROUND) {
		jump_time_ += delta_seconds;
		return;
	}
	jump_time_ = 0.0f;
	const float speed = GetMoveSpeed(move_state);
	if (speed >= MIN_LOCOMOTION_SPEED) {
		const float direction = GetMoveDirection(move_state);
		if (!IsInCardinal(cardinal_, direction, CARDINAL_HYSTERESIS_DEGREES)) {
			for (const LocomotionCardinal cardinal : { LocomotionCardinal::FORWARD, LocomotionCardinal::BACKWARD, LocomotionCardinal::LEFT, LocomotionCardinal::RIGHT }) {
				if (IsInCardinal(cardinal, direction, 0.0f)) {
					cardinal_ = cardinal;
					break;
				}
			}
		}
	}
	// 섞는 두 샘플의 한 바퀴 시간을 비율대로 섞은 만큼 정규화 재생 위치를 진행한다.
	const LocomotionBlend blend = GetLocomotionBlend(*data_, cardinal_, speed);
	const float cycle = GetSampleCycle(*data_, blend.first) * (1.0f - blend.alpha) + GetSampleCycle(*data_, blend.second) * blend.alpha;
	if (cycle > 0.0f) {
		locomotion_phase_ += delta_seconds / cycle;
		locomotion_phase_ -= std::floor(locomotion_phase_);
	}
}

void CharacterPose::StartAction(std::uint8_t action, std::uint8_t weapon, long long time_ms)
{
	if (!data_ || !data_->IsLoaded()) return;
	const MontageInfo* montage = data_->FindMontage(action, weapon);
	if (!montage) return;
	montage_ = montage;
	montage_start_ms_ = time_ms;
}

std::vector<BoneTransform> CharacterPose::BuildBonePoses(const MOVE_STATE& move_state, long long time_ms) const
{
	if (!data_ || !data_->IsLoaded()) return {};
	const HitboxData& data = *data_;
	const int bone_count = data.GetBoneCount();
	const float speed = GetMoveSpeed(move_state);

	// 1. 하체 기본 자세: 공중이면 점프 애니메이션, 땅이면 기준 방향 줄에서 속도로 두 샘플을 섞는다.
	std::vector<BoneTransform> locals;
	if (move_state.air_phase != AirPhase::GROUND) {
		locals = SampleTrack(data.GetTrack(data.GetJumpTrack()), bone_count, jump_time_, true);
	}
	else {
		const LocomotionBlend blend = GetLocomotionBlend(data, cardinal_, speed);
		const AnimationTrack& first = data.GetTrack(data.GetLocomotionSamples()[blend.first].track);
		const AnimationTrack& second = data.GetTrack(data.GetLocomotionSamples()[blend.second].track);
		locals = BlendPoses(SampleTrack(first, bone_count, locomotion_phase_ * first.length, true), SampleTrack(second, bone_count, locomotion_phase_ * second.length, true), blend.alpha);
	}
	std::vector<BoneTransform> poses = ComposePose(data, locals);

	// 뼈를 액터 기준으로 rotation만큼 더 돌린다. 뼈 위치는 그대로이고 아래 뼈는 따라 돈다. 루트는 액터 원점을 중심으로 위치도 돈다.
	const auto rotate_bone = [&](int bone, FXMVECTOR rotation)
	{
		const int parent = data.GetBoneParent(bone);
		const XMVECTOR actor_rotation = XMQuaternionMultiply(XMLoadFloat4(&poses[bone].rotation), rotation);
		if (parent < 0) {
			XMStoreFloat4(&locals[bone].rotation, actor_rotation);
			XMStoreFloat3(&locals[bone].position, XMVector3Rotate(XMLoadFloat3(&locals[bone].position), rotation));
		}
		else {
			XMStoreFloat4(&locals[bone].rotation, XMQuaternionMultiply(actor_rotation, XMQuaternionInverse(XMLoadFloat4(&poses[parent].rotation))));
		}
		poses = ComposePose(data, locals);
	};
	// branch_root 아래 뼈들을 source 자세 쪽으로 alpha만큼 섞는다. 회전은 액터 기준 회전을, 위치는 부모 기준 위치를 섞는다.
	// 애니메이션 블루프린트의 메시 공간 회전 레이어 블렌드와 같은 방식이라, 하체가 어떻게 돌든 상체는 source의 액터 기준 방향을 따른다.
	const auto blend_branch = [&](int branch_root, const std::vector<BoneTransform>& source_locals, float alpha)
	{
		const std::vector<BoneTransform> source_poses = ComposePose(data, source_locals);
		std::vector<XMVECTOR> actor_rotations(bone_count);
		for (int bone = 0; bone < bone_count; ++bone) {
			actor_rotations[bone] = XMLoadFloat4(&poses[bone].rotation);
			if (!data.IsInBranch(bone, branch_root)) continue;
			actor_rotations[bone] = BlendRotation(actor_rotations[bone], XMLoadFloat4(&source_poses[bone].rotation), alpha);
			const int parent = data.GetBoneParent(bone);
			XMStoreFloat4(&locals[bone].rotation, XMQuaternionMultiply(actor_rotations[bone], XMQuaternionInverse(actor_rotations[parent])));
			XMStoreFloat3(&locals[bone].position, XMVectorLerp(XMLoadFloat3(&locals[bone].position), XMLoadFloat3(&source_locals[bone].position), alpha));
		}
		poses = ComposePose(data, locals);
	};

	// 2. 하체 돌리기: 땅에서 움직이면 루트를 실제 이동 방향까지 돌리고, 허리와 가슴을 반씩 되돌려 상체가 앞을 보게 한다.
	if (move_state.air_phase == AirPhase::GROUND && speed >= MIN_LOCOMOTION_SPEED) {
		const float angle = XMConvertToRadians(GetDeltaAngle(GetCardinalAngle(cardinal_), GetMoveDirection(move_state)));
		rotate_bone(0, XMQuaternionRotationNormal(g_XMIdentityR2, angle));
		rotate_bone(data.GetSpineBone(), XMQuaternionRotationNormal(g_XMIdentityR2, -angle * 0.5f));
		rotate_bone(data.GetChestBone(), XMQuaternionRotationNormal(g_XMIdentityR2, -angle * 0.5f));
	}

	// 3. 상체 무기 자세. 무기가 없으면 공중에서만 대기 자세를 덮는다.
	const WeaponInfo* weapon_info = data.GetWeaponInfo(move_state.weapon);
	const WeaponPoseType pose_type = weapon_info ? weapon_info->pose_type : WeaponPoseType::NONE;
	int upper_track = -1;
	switch (pose_type) {
	case WeaponPoseType::PICKAXE: upper_track = data.GetPoseTrack(UpperBodyPose::PICKAXE_HOLD); break;
	case WeaponPoseType::PISTOL: upper_track = data.GetPoseTrack(UpperBodyPose::PISTOL_AIM); break;
	case WeaponPoseType::RIFLE: upper_track = data.GetPoseTrack(move_state.is_aiming ? UpperBodyPose::RIFLE_AIM : UpperBodyPose::RIFLE_HOLD); break;
	default: upper_track = move_state.air_phase != AirPhase::GROUND ? data.GetPoseTrack(UpperBodyPose::IDLE) : -1; break;
	}
	if (upper_track >= 0) blend_branch(data.GetSpineBone(), SampleTrack(data.GetTrack(upper_track), bone_count, 0.0f, false), 1.0f);

	// 4. 동작 몽타주: 들어올 때와 끝날 때 섞임 시간만큼 비율을 올리고 내린다.
	// 전신·꺼내기 동작은 조준 기울기 전에, 재장전·오른팔 동작은 조준 기울기 뒤에 덮는다(애니메이션 블루프린트 순서).
	const MontageInfo* montage = montage_;
	float montage_alpha = 0.0f;
	std::vector<BoneTransform> montage_locals;
	if (montage) {
		const AnimationTrack& track = data.GetTrack(montage->track);
		const float elapsed = (time_ms - montage_start_ms_) / 1000.0f;
		const float duration = track.length / std::max(montage->play_rate, 0.01f);
		if (elapsed >= 0.0f && elapsed < duration) {
			const float blend_in = montage->blend_in > 0.0f ? std::min(1.0f, elapsed / montage->blend_in) : 1.0f;
			const float blend_out = montage->blend_out > 0.0f ? std::min(1.0f, (duration - elapsed) / montage->blend_out) : 1.0f;
			montage_alpha = std::min(blend_in, blend_out);
			montage_locals = SampleTrack(track, bone_count, elapsed * montage->play_rate, false);
		}
	}
	const bool is_late_montage = montage && (montage->region == MontageRegion::RIGHT_ARM || (montage->action != ACTION_EQUIP && montage->region == MontageRegion::UPPER_BODY));
	if (montage_alpha > 0.0f && !is_late_montage) {
		if (montage->region == MontageRegion::FULL_BODY) {
			locals = BlendPoses(locals, montage_locals, montage_alpha);
			poses = ComposePose(data, locals);
		}
		else {
			blend_branch(data.GetSpineBone(), montage_locals, montage_alpha);
		}
	}

	// 5. 조준 상하: 권총·소총은 시선 상하 각도를 허리와 가슴에 나눠 기울인다. 위를 볼 때 앞(X)이 위(Z)로 가도록 Y축 반대로 돈다.
	if (pose_type == WeaponPoseType::PISTOL || pose_type == WeaponPoseType::RIFLE) {
		const float pitch = XMConvertToRadians(std::clamp(move_state.view_pitch, -weapon_info->aim_pitch_down_limit, weapon_info->aim_pitch_up_limit));
		rotate_bone(data.GetSpineBone(), XMQuaternionRotationNormal(g_XMIdentityR1, -pitch * SPINE_AIM_RATIO));
		rotate_bone(data.GetChestBone(), XMQuaternionRotationNormal(g_XMIdentityR1, -pitch * CHEST_AIM_RATIO));
	}

	if (montage_alpha > 0.0f && is_late_montage) {
		blend_branch(montage->region == MontageRegion::RIGHT_ARM ? data.GetUpperarmRBone() : data.GetSpineBone(), montage_locals, montage_alpha);
	}

	// 6. 고개: 머리의 위쪽 축(Z)이 시선 상하 방향을 향하도록 가장 짧게 돌린다.
	const float head_pitch = XMConvertToRadians(std::clamp(move_state.view_pitch, -data.GetHeadPitchDownLimit(), data.GetHeadPitchUpLimit()));
	const int head = data.GetHeadBone();
	const XMVECTOR head_axis = XMVector3Rotate(g_XMIdentityR2, XMLoadFloat4(&poses[head].rotation));
	rotate_bone(head, FindBetween(head_axis, XMVectorSet(std::cos(head_pitch), 0.0f, std::sin(head_pitch), 0.0f)));
	return poses;
}

std::vector<HitboxShape> CharacterPose::BuildWorldHitboxes(const MOVE_STATE& move_state, long long time_ms) const
{
	const std::vector<BoneTransform> poses = BuildBonePoses(move_state, time_ms);
	if (poses.empty()) return {};
	const HitboxData& data = *data_;

	// 액터 기준 자세에 캐릭터 좌우 방향과 위치를 적용해 도형을 월드에 놓는다.
	const XMVECTOR yaw = XMQuaternionRotationNormal(g_XMIdentityR2, XMConvertToRadians(move_state.yaw));
	const XMVECTOR location = XMVectorSet(move_state.location[0], move_state.location[1], move_state.location[2], 0.0f);
	std::vector<HitboxShape> world_shapes;
	for (const HitboxBody& body : data.GetBodies()) {
		const XMVECTOR bone_rotation = XMLoadFloat4(&poses[body.bone].rotation);
		const XMVECTOR bone_position = XMLoadFloat3(&poses[body.bone].position);
		for (const HitboxShape& shape : body.shapes) {
			HitboxShape world_shape = shape;
			const XMVECTOR actor_position = XMVectorAdd(bone_position, XMVector3Rotate(XMLoadFloat3(&shape.transform.position), bone_rotation));
			XMStoreFloat3(&world_shape.transform.position, XMVectorAdd(location, XMVector3Rotate(actor_position, yaw)));
			XMStoreFloat4(&world_shape.transform.rotation, XMQuaternionMultiply(XMQuaternionMultiply(XMLoadFloat4(&shape.transform.rotation), bone_rotation), yaw));
			world_shapes.push_back(world_shape);
		}
	}
	return world_shapes;
}
