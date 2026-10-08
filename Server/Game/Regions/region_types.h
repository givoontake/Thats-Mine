#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

// 맵 영역 하나. 에디터에서 그린 닫힌 스플라인의 조종점을 순서대로 이은 다각형이고, 좌표는 언리얼 월드 좌표(cm)다.
struct MapRegion {
	std::string name;
	bool is_treasure_excluded; // 최종 보물을 이 영역 안에 놓지 않는다.
	std::vector<DirectX::XMFLOAT2> points;
};
