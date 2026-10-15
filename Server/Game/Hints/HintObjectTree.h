#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "hint_types.h"

class MapData;

// 힌트 물체(바위·나무·대나무·색 데이지·색 버섯)의 지도 위치로 만든 2차원 k-d 트리. 서버 시작 때 맵 데이터에서 한 번 만든다.
// 나누기는 수평 좌표로만 하고 거리는 높이까지 넣은 3차원 거리로 잰다. 기준선까지 수평 거리는 3차원 거리보다 길 수 없어 그대로 가지치기에 쓴다.
// 노드를 따로 두지 않는다. 점 배열의 구간 [시작, 끝)에서 가운데 점이 그 구간의 기준선이고, 깊이가 짝수면 x, 홀수면 y로 나눈다.
// 가운데 점은 기준선으로 그 자리에 두고, 왼쪽 자식은 [시작, 가운데), 오른쪽 자식은 [가운데 + 1, 끝)이다.
// 만든 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 찾는다.
class HintObjectTree
{
	std::vector<HintPoint> points_;
	bool is_loaded_ = false;

	void BuildRange(std::size_t begin, std::size_t end, int depth);
	// counts는 재귀 전체가 같이 채운다.
	void CountRange(std::size_t begin, std::size_t end, int depth, float x, float y, float z, float radius_sq, HintKindCounts& counts) const;

public:
	// 맵 데이터에서 힌트 종류 메시의 물체 중 맵 범위 안의 것을 모아 트리를 만든다. 대응표의 메시가 맵 데이터에 없으면 실패한다.
	bool Build(const MapData& map);
	bool IsLoaded() const { return is_loaded_; }
	// (x, y, z)에서 거리 radius(cm) 안에 있는 물체 수를 종류마다 센다.
	HintKindCounts CountKinds(float x, float y, float z, float radius) const;
	// 트리의 점을 클라이언트 돋보기가 읽는 힌트 물체 목록 파일(hint_object_file.h)로 쓴다.
	bool Export(const std::string& file_path) const;
};
