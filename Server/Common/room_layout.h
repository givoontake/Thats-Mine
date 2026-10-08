#pragma once
#include <vector>
#include "platform_layout.h"

// 방이 게임 시작 때 정하는 배치물 상태. 지형 판정은 이번 게임에 놓인 발판·탑만 본다.
struct RoomLayout {
	PlatformLayout platforms{};
	std::vector<bool> towers; // 탑 후보마다 놓을지 여부. 순서는 탑 후보 파일과 같고, 비어 있으면 탑을 보지 않는다.
};
