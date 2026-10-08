#include "TowerCandidateData.h"
#include <fstream>
#include <sstream>

// 줄: x y z (cm). z는 탑 바닥 높이다.
bool TowerCandidateData::Load(const std::string& file_path)
{
	candidates_.clear();
	is_loaded_ = false;
	std::ifstream file(file_path);
	if (!file) return false;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		DirectX::XMFLOAT3 candidate{};
		stream >> candidate.x >> candidate.y >> candidate.z;
		if (!stream) return false;
		candidates_.push_back(candidate);
	}
	is_loaded_ = !candidates_.empty();
	return is_loaded_;
}
