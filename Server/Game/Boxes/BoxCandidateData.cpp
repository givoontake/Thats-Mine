#include "BoxCandidateData.h"
#include <fstream>
#include <optional>
#include <sstream>

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
