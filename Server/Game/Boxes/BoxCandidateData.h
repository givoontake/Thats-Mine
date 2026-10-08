#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "box_types.h"

// 상자 배치 후보(자연 배치 기준과 유적별 지정 자리)를 서버 시작 때 읽어 둔다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class BoxCandidateData
{
	std::vector<BoxAnchor> anchors_;
	std::vector<BoxGroup> groups_;
	bool is_loaded_ = false;

	bool LoadAnchors(const std::string& file_path);
	bool LoadGroups(const std::string& file_path);

public:
	// anchor_path: 자연 배치 후보 파일, group_path: 유적 상자 파일
	bool Load(const std::string& anchor_path, const std::string& group_path);
	bool IsLoaded() const { return is_loaded_; }
	const std::vector<BoxAnchor>& GetAnchors() const { return anchors_; }
	const std::vector<BoxGroup>& GetGroups() const { return groups_; }
	// 한 판에 놓을 상자 자리를 고른다. 유적 상자를 규칙대로 먼저 고르고, 자연 상자 NATURAL_BOX_COUNT개를 퍼뜨려 고른다.
	std::vector<BoxPlacement> SelectPlacements(std::uint32_t seed) const;
};
