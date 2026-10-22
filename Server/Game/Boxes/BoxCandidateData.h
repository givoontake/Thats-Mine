#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "box_types.h"

class BoxShapeData;

// 상자 배치 후보(자연 배치 기준과 유적별 지정 자리)를 서버 시작 때 읽어 둔다.
// 모든 상자 자리를 한 목록에 담아 자리마다 번호 하나를 정하고, 자연 기준과 유적은 자기 자리 번호만 갖는다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class BoxCandidateData
{
	std::vector<BoxPlacement> placements_; // 모든 상자 자리. 자리 번호는 이 목록의 번호다.
	std::vector<BoxAnchor> anchors_;
	std::vector<BoxGroup> groups_;
	bool is_loaded_ = false;

	bool LoadAnchors(const std::string& file_path);
	bool LoadGroups(const std::string& file_path);

public:
	// anchor_path: 자연 배치 후보 파일, group_path: 유적 상자 파일. 상자 중심을 구하려고 먼저 읽은 상자 모양 데이터가 필요하다.
	bool Load(const std::string& anchor_path, const std::string& group_path, const BoxShapeData& box_shapes);
	bool IsLoaded() const { return is_loaded_; }
	const std::vector<BoxPlacement>& GetPlacements() const { return placements_; }
	// 한 판에 놓을 상자 자리 번호를 box_count개 고른다. 유적 상자를 규칙대로 먼저 고르고, 나머지는 자연 상자로 퍼뜨려 고른다. 후보가 모자라면 있는 만큼 고른다.
	std::vector<std::uint32_t> SelectPlacements(std::size_t box_count) const;
};
