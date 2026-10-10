#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "game_packets.h"

// 상자 하나를 놓을 자리. 좌표는 언리얼 월드 좌표(cm), 회전은 언리얼 회전 순서(pitch, yaw, roll, 도)다.
// location은 상자 바닥 가운데이고, center는 읽을 때 계산해 둔 상자 중심이다.
struct BoxPlacement {
	BoxKind kind;
	DirectX::XMFLOAT3 location;
	DirectX::XMFLOAT3 rotation;
	DirectX::XMFLOAT3 center;
};

// 자연 배치 기준(나무·바위) 하나. 선물 자리와 보물 방향을 함께 가지며, 한 판에 이 중 한 자리만 쓴다.
struct BoxAnchor {
	DirectX::XMFLOAT2 location;
	std::vector<std::uint32_t> placement_indices; // 상자 자리 목록에서 이 기준의 자리 번호
};

// 유적 하나에 직접 지정한 상자 자리들.
struct BoxGroup {
	std::string name;
	std::vector<std::uint32_t> placement_indices; // 상자 자리 목록에서 이 유적의 자리 번호
};
