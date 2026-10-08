#include "BoxCandidateData.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include "settings.h"

// 유적마다 한 판에 놓는 상자 수 범위. 범위 안의 개수를 같은 확률로 정한다(0~1이면 놓을 확률 50%).
struct BoxGroupRule {
	const char* name;
	int min_count;
	int max_count;
};
static constexpr BoxGroupRule BOX_GROUP_RULES[] = { { "Top", 4, 4 }, { "Asia", 3, 4 }, { "Egypt", 3, 4 }, { "Pyramid", 0, 1 } };
// 자연 상자 후보가 이 횟수만큼 연달아 최소 거리를 못 지키면 최소 거리를 줄인다.
static constexpr int BOX_RETRY_COUNT = 20;
static constexpr float BOX_MIN_DISTANCE_SHRINK = 0.9f;

// 상자 종류 글자(Treasure/Gift)를 바꾼다. 모르는 글자면 값이 없다.
static std::optional<BoxKind> ParseBoxKind(const std::string& text)
{
	if (text == "Treasure") return BoxKind::TREASURE;
	if (text == "Gift") return BoxKind::GIFT;
	return std::nullopt;
}

// 줄: 기준번호 기준x 기준y 종류 x y z pitch yaw roll. 같은 기준번호의 줄은 이어서 나온다.
bool BoxCandidateData::LoadAnchors(const std::string& file_path)
{
	std::ifstream file(file_path);
	if (!file) return false;

	std::string line;
	int last_anchor = -1;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		int anchor = 0;
		DirectX::XMFLOAT2 anchor_location{};
		std::string kind_text;
		BoxPlacement placement{};
		stream >> anchor >> anchor_location.x >> anchor_location.y >> kind_text
			>> placement.location.x >> placement.location.y >> placement.location.z
			>> placement.rotation.x >> placement.rotation.y >> placement.rotation.z;
		const std::optional<BoxKind> kind = ParseBoxKind(kind_text);
		if (!stream || !kind) return false;
		placement.kind = *kind;

		if (anchor != last_anchor)
		{
			anchors_.push_back({ anchor_location, {} });
			last_anchor = anchor;
		}
		anchors_.back().placements.push_back(placement);
	}
	return !anchors_.empty();
}

// 줄: 유적 종류 이름 x y z pitch yaw roll 뒤의 칸(크기·메시·원래 액터)은 읽지 않는다. 같은 유적의 줄은 이어서 나온다.
bool BoxCandidateData::LoadGroups(const std::string& file_path)
{
	std::ifstream file(file_path);
	if (!file) return false;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		std::string group_name, kind_text, box_name;
		BoxPlacement placement{};
		stream >> group_name >> kind_text >> box_name
			>> placement.location.x >> placement.location.y >> placement.location.z
			>> placement.rotation.x >> placement.rotation.y >> placement.rotation.z;
		const std::optional<BoxKind> kind = ParseBoxKind(kind_text);
		if (!stream || !kind) return false;
		placement.kind = *kind;

		if (groups_.empty() || groups_.back().name != group_name) groups_.push_back({ group_name, {} });
		groups_.back().placements.push_back(placement);
	}
	return !groups_.empty();
}

bool BoxCandidateData::Load(const std::string& anchor_path, const std::string& group_path)
{
	anchors_.clear();
	groups_.clear();
	is_loaded_ = LoadAnchors(anchor_path) && LoadGroups(group_path);
	return is_loaded_;
}

std::vector<BoxPlacement> BoxCandidateData::SelectPlacements(std::uint32_t seed) const
{
	std::mt19937 random(seed);
	std::vector<BoxPlacement> placements;

	// 유적: 규칙 범위에서 개수를 정하고, 유적의 자리 중 그만큼을 겹치지 않게 고른다.
	for (const BoxGroup& group : groups_)
	{
		const auto rule = std::find_if(std::begin(BOX_GROUP_RULES), std::end(BOX_GROUP_RULES), [&](const BoxGroupRule& candidate) { return group.name == candidate.name; });
		if (rule == std::end(BOX_GROUP_RULES)) continue;
		const int count = std::min(std::uniform_int_distribution<int>(rule->min_count, rule->max_count)(random), static_cast<int>(group.placements.size()));
		std::vector<BoxPlacement> shuffled = group.placements;
		std::shuffle(shuffled.begin(), shuffled.end(), random);
		placements.insert(placements.end(), shuffled.begin(), shuffled.begin() + count);
	}

	// 자연: 미첼 최적 후보 방식. 쓰지 않은 기준을 BOX_CANDIDATE_SAMPLE_COUNT개 뽑아, 이미 놓인 상자(유적 포함)까지 가장 가까운 거리가 가장 먼 기준을 고르고,
	// 그 기준의 자리(선물 자리·보물 방향) 중 한 곳을 무작위로 놓는다. 고른 기준도 최소 거리 안이면 놓지 않고 다시 뽑는다.
	const auto nearest_distance_sq = [&](const DirectX::XMFLOAT2& location)
	{
		float nearest = std::numeric_limits<float>::max();
		for (const BoxPlacement& placement : placements) {
			const float dx = placement.location.x - location.x;
			const float dy = placement.location.y - location.y;
			nearest = std::min(nearest, dx * dx + dy * dy);
		}
		return nearest;
	};
	std::vector<std::uint32_t> unused(anchors_.size());
	std::iota(unused.begin(), unused.end(), 0u);
	float min_distance = BOX_MIN_DISTANCE;
	int natural_count = 0;
	int failure_count = 0;
	while (natural_count < NATURAL_BOX_COUNT && !unused.empty())
	{
		std::size_t best = 0;
		float best_distance_sq = -1.0f;
		for (int sample = 0; sample < BOX_CANDIDATE_SAMPLE_COUNT; ++sample) {
			const std::size_t index = std::uniform_int_distribution<std::size_t>(0, unused.size() - 1)(random);
			const float distance_sq = nearest_distance_sq(anchors_[unused[index]].location);
			if (distance_sq > best_distance_sq) {
				best = index;
				best_distance_sq = distance_sq;
			}
		}
		if (best_distance_sq < min_distance * min_distance) {
			if (++failure_count >= BOX_RETRY_COUNT) {
				min_distance *= BOX_MIN_DISTANCE_SHRINK;
				failure_count = 0;
			}
			continue;
		}
		failure_count = 0;
		const BoxAnchor& anchor = anchors_[unused[best]];
		placements.push_back(anchor.placements[std::uniform_int_distribution<std::size_t>(0, anchor.placements.size() - 1)(random)]);
		unused[best] = unused.back();
		unused.pop_back();
		++natural_count;
	}
	return placements;
}
