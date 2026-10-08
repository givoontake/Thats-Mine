#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "game_packets.h"

// 상자 하나를 놓을 자리. 좌표는 언리얼 월드 좌표(cm), 회전은 언리얼 회전 순서(pitch, yaw, roll, 도)다.
struct BoxPlacement {
	BoxKind kind;
	DirectX::XMFLOAT3 location;
	DirectX::XMFLOAT3 rotation;
};

// 자연 배치 기준(나무·바위) 하나. 선물 자리와 보물 방향을 함께 가지며, 한 판에 이 중 한 자리만 쓴다.
struct BoxAnchor {
	DirectX::XMFLOAT2 location;
	std::vector<BoxPlacement> placements;
};

// 유적 하나에 직접 지정한 상자 자리들.
struct BoxGroup {
	std::string name;
	std::vector<BoxPlacement> placements;
};
