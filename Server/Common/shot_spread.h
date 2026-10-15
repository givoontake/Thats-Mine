#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

// 총 탄 퍼짐. 서버와 클라이언트가 같은 계산을 쓴다.
// 공중에서는 항상 최대 퍼짐이다. 조준이 있는 무기(저격총)는 조준하지 않으면 기본, 조준하고 서 있으면 aimed_still, 조준하고 움직이면 aimed_moving이다.
// 연사가 있는 무기(권총)는 연사 열기로 퍼짐이 커진다. 쏠 때마다 열기가 1 오르고(최대 heat_limit), 열기가 free_heat보다 높으면 넘은 만큼 per_shot씩 퍼짐이 커진다.
// 마지막 발사 뒤 발사 간격만큼 더 쏘지 않으면 그때부터 발사 간격마다 열기가 1씩 내려간다. 최대 속도로 연사하면 회복 없이 오른다.
// 한 발이 빗나가는 방향은 (판 시드, 플레이어 아이디, 무기, 그 무기의 발사 번호)를 섞은 해시로 정한다. 정수 연산만 써서 서버와 클라이언트가 같은 값을 얻는다.
// 해시에서 얻은 두 수로 원 둘레 방향과 빗나감 비율(0~1)을 정하고, 실제 빗나간 각도는 비율 × 퍼짐 크기라 가운데 쪽에 더 많이 맞는다.

// 무기 하나의 퍼짐 수치(도)
struct ShotSpreadSettings {
	float base;
	float max;
	bool has_aim;
	float aimed_still;
	float aimed_moving;
	float per_shot;
	int free_heat;
	int heat_limit;
};

// 무기 값 순서: 곡괭이, 저격총, 권총, 돋보기. 총이 아닌 무기는 퍼짐이 없다.
// 권총은 1·2발째가 기본이고 3발째부터 0.1도씩 커져 7발째에 최대다.
constexpr ShotSpreadSettings SHOT_SPREADS[] = {
	{ 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 0, 0 },
	{ 5.0f, 5.0f, true, 0.0f, 0.5f, 0.0f, 0, 0 },
	{ 0.5f, 1.0f, false, 0.0f, 0.0f, 0.1f, 1, 6 },
	{ 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 0, 0 },
};
// 조준한 채 움직이는 것으로 보는 수평 속도(cm/s)
constexpr float SHOT_SPREAD_MOVING_SPEED = 10.0f;
// 언리얼의 PI 매크로와 겹치지 않게 따로 둔다.
constexpr float SHOT_SPREAD_PI = 3.14159265358979f;

// 빗나간 탄 방향(단위 벡터)
struct ShotSpreadDirection {
	float x;
	float y;
	float z;
};

constexpr const ShotSpreadSettings& GetShotSpreadSettings(std::uint8_t weapon)
{
	return SHOT_SPREADS[weapon < std::size(SHOT_SPREADS) ? weapon : 0];
}

// 마지막 발사 뒤 elapsed_seconds가 지났을 때의 연사 열기
inline int RecoverShotHeat(int heat, float elapsed_seconds, float fire_interval)
{
	const int recovered = static_cast<int>(std::floor(std::max(0.0f, elapsed_seconds) / std::max(fire_interval, 0.01f))) - 1;
	return std::max(0, heat - std::max(0, recovered));
}

// 쏜 뒤의 연사 열기
inline int AddShotHeat(const ShotSpreadSettings& settings, int heat)
{
	return std::min(heat + 1, settings.heat_limit);
}

// 지금 쏘면 쓰는 퍼짐 크기(도)
inline float GetShotSpread(const ShotSpreadSettings& settings, int heat, bool is_aiming, float horizontal_speed, bool is_falling)
{
	if (is_falling) return settings.max;
	if (settings.has_aim && is_aiming) return horizontal_speed > SHOT_SPREAD_MOVING_SPEED ? settings.aimed_moving : settings.aimed_still;
	return std::min(settings.max, settings.base + settings.per_shot * std::max(0, heat - settings.free_heat));
}

constexpr std::uint64_t MixShotSpreadBits(std::uint64_t value)
{
	value += 0x9E3779B97F4A7C15ull;
	value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
	value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
	return value ^ (value >> 31);
}

// 한 발의 빗나감을 정하는 해시
constexpr std::uint64_t GetShotSpreadHash(std::uint64_t seed, std::int32_t player_id, std::uint8_t weapon, std::uint32_t shot_index)
{
	return MixShotSpreadBits(MixShotSpreadBits(MixShotSpreadBits(seed ^ static_cast<std::uint32_t>(player_id)) ^ weapon) ^ shot_index);
}

// 조준 방향(단위 벡터)을 해시로 정한 방향으로 비율 × spread_degrees만큼 기울인 방향.
// 조준 방향에 수직인 오른쪽·위쪽 축을 만들고, 원 둘레 방향 각으로 두 축을 섞은 쪽으로 기울인다.
inline ShotSpreadDirection ApplyShotSpread(float aim_x, float aim_y, float aim_z, float spread_degrees, std::uint64_t hash)
{
	const float around = static_cast<float>(hash >> 40) / 16777216.0f * 2.0f * SHOT_SPREAD_PI;
	const float ratio = static_cast<float>(hash & 0xFFFFFFull) / 16777216.0f;
	const float tilt = ratio * spread_degrees * SHOT_SPREAD_PI / 180.0f;
	// 조준이 거의 수직이면 위쪽 대신 x축을 기준으로 오른쪽 축을 만든다.
	const bool is_vertical = std::abs(aim_z) > 0.99f;
	const float reference[3] = { is_vertical ? 1.0f : 0.0f, 0.0f, is_vertical ? 0.0f : 1.0f };
	float right[3] = { aim_y * reference[2] - aim_z * reference[1], aim_z * reference[0] - aim_x * reference[2], aim_x * reference[1] - aim_y * reference[0] };
	const float right_length = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
	for (float& value : right) value /= right_length;
	const float up[3] = { right[1] * aim_z - right[2] * aim_y, right[2] * aim_x - right[0] * aim_z, right[0] * aim_y - right[1] * aim_x };
	const float side = std::sin(tilt);
	const float forward = std::cos(tilt);
	ShotSpreadDirection direction{
		aim_x * forward + (right[0] * std::cos(around) + up[0] * std::sin(around)) * side,
		aim_y * forward + (right[1] * std::cos(around) + up[1] * std::sin(around)) * side,
		aim_z * forward + (right[2] * std::cos(around) + up[2] * std::sin(around)) * side,
	};
	const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	direction.x /= length;
	direction.y /= length;
	direction.z /= length;
	return direction;
}
