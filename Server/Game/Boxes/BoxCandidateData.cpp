#include "BoxCandidateData.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <random>
#include <sstream>
#include "random_seed.h"
#include "settings.h"
#include "spread_selection.h"

// 유적마다 한 판에 놓는 상자 수 범위. 범위 안의 개수를 같은 확률로 정한다(0~1이면 놓을 확률 50%).
struct BoxGroupRule {
	const char* name;
	int min_count;
	int max_count;
};
static constexpr BoxGroupRule BOX_GROUP_RULES[] = { { "Top", 4, 4 }, { "Asia", 3, 4 }, { "Egypt", 3, 4 }, { "Pyramid", 0, 1 } };

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
		anchors_.back().placement_indices.push_back(static_cast<std::uint32_t>(placements_.size()));
		placements_.push_back(placement);
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
		groups_.back().placement_indices.push_back(static_cast<std::uint32_t>(placements_.size()));
		placements_.push_back(placement);
	}
	return !groups_.empty();
}

bool BoxCandidateData::Load(const std::string& anchor_path, const std::string& group_path)
{
	placements_.clear();
	anchors_.clear();
	groups_.clear();
	is_loaded_ = LoadAnchors(anchor_path) && LoadGroups(group_path);
	return is_loaded_;
}

std::vector<std::uint32_t> BoxCandidateData::SelectPlacements(std::size_t box_count) const
{
	std::mt19937 random(CreateRandomSeed());
	std::vector<std::uint32_t> placements;

	// 유적: 규칙 범위에서 개수를 정하고, 유적의 자리 중 그만큼을 겹치지 않게 고른다.
	for (const BoxGroup& group : groups_)
	{
		const auto rule = std::find_if(std::begin(BOX_GROUP_RULES), std::end(BOX_GROUP_RULES), [&](const BoxGroupRule& candidate) { return group.name == candidate.name; });
		if (rule == std::end(BOX_GROUP_RULES)) continue;
		const int count = std::min(std::uniform_int_distribution<int>(rule->min_count, rule->max_count)(random), static_cast<int>(group.placement_indices.size()));
		std::vector<std::uint32_t> shuffled = group.placement_indices;
		std::shuffle(shuffled.begin(), shuffled.end(), random);
		placements.insert(placements.end(), shuffled.begin(), shuffled.begin() + count);
	}

	// 자연: 남은 개수만큼 기준(나무·바위)을 미첼 최적 후보 방식으로 고르게 퍼뜨려 고르고, 기준마다 자리(선물 자리·보물 방향) 중 한 곳을 무작위로 놓는다.
	// 유적 상자와의 거리는 보지 않는다.
	std::vector<DirectX::XMFLOAT2> anchor_locations;
	anchor_locations.reserve(anchors_.size());
	for (const BoxAnchor& anchor : anchors_) anchor_locations.push_back(anchor.location);
	const int natural_count = box_count > placements.size() ? static_cast<int>(box_count - placements.size()) : 0;
	for (const std::size_t index : SelectSpreadIndices(anchor_locations, natural_count, BOX_MIN_DISTANCE))
	{
		const BoxAnchor& anchor = anchors_[index];
		placements.push_back(anchor.placement_indices[std::uniform_int_distribution<std::size_t>(0, anchor.placement_indices.size() - 1)(random)]);
	}
	return placements;
}
