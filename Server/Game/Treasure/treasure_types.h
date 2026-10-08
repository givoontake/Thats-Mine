#pragma once
#include <DirectXMath.h>

// 최종 보물 배치. 판마다 보물을 놓을 수 있는 땅 중에서 무작위로 정하고, 서버만 알고 클라이언트에 보내지 않는다.
struct TreasurePlacement {
	DirectX::XMFLOAT3 location; // 보물 바닥 가운데(cm)
	float yaw; // 방향(도)
};
