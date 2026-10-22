#pragma once
#include "game_packets.h"

// 메시·폴리지 종류 이름과 힌트 종류. 눈 덮인 바위·가문비도 같은 모양이라 같은 종류로 묶는다.
// 색 데이지·버섯은 같은 메시에 재질만 바꾼 폴리지 종류 이름으로 구분하고, 지형 풀에서 옮긴 데이지(SM_DaisyFlowers_Landscape_FT)는 흰 데이지다.
// 서버 힌트 판정과 클라이언트 핑 종류 표시가 같이 쓴다. 클라이언트는 메시 이름, 서버는 맵 데이터 항목 이름(충돌 없는 폴리지는 폴리지 종류 이름)으로 찾으므로
// 둘이 다르면 같은 종류로 두 줄을 둔다(대나무). 서버는 종류마다 하나라도 찾으면 된다.
struct HintMesh {
	const char* name;
	HintObjectKind kind;
};
constexpr HintMesh HINT_MESHES[] = {
	{ "SM_RockLarge_01_RVT", HintObjectKind::LARGE_ROCK },
	{ "SM_RockLarge_02_RVT", HintObjectKind::LARGE_ROCK },
	{ "SM_RockLarge_Snow_01", HintObjectKind::LARGE_ROCK },
	{ "SM_RockLarge_Snow_02", HintObjectKind::LARGE_ROCK },
	{ "SM_RockMedium_01_RVT", HintObjectKind::SMALL_ROCK },
	{ "SM_RockMedium_02_RVT", HintObjectKind::SMALL_ROCK },
	{ "SM_RockMedium_Snow_01", HintObjectKind::SMALL_ROCK },
	{ "SM_RockMedium_Snow_02", HintObjectKind::SMALL_ROCK },
	{ "SM_SS_OakTree01", HintObjectKind::OAK },
	{ "SM_SS_OakTree01_RVT", HintObjectKind::OAK },
	{ "SM_SS_OakTree02_RVT", HintObjectKind::OAK },
	{ "SM_SS_OakTreeTall_RVT", HintObjectKind::OAK },
	{ "SM_SS_SpruceTree_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_SpruceTree_Tall_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_Spruce_Snow_Tree_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_Spruce_Snow_Tree_Tall_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_BirchTree_RVT", HintObjectKind::BIRCH },
	{ "SM_SS_PoplarTree_RVT", HintObjectKind::POPLAR },
	{ "SM_tree_02", HintObjectKind::MAPLE },
	{ "SM_bamboo_01", HintObjectKind::BAMBOO },
	{ "FT_Bamboo", HintObjectKind::BAMBOO },
	{ "SM_DaisyFlowers", HintObjectKind::DAISY_WHITE },
	{ "SM_DaisyFlowers_Red_FT", HintObjectKind::DAISY_RED },
	{ "SM_DaisyFlowers_Magenta_FT", HintObjectKind::DAISY_MAGENTA },
	{ "SM_DaisyFlowers_Blue_FT", HintObjectKind::DAISY_BLUE },
	{ "SM_DaisyFlowers_Yellow_FT", HintObjectKind::DAISY_YELLOW },
	{ "SM_DaisyFlowers_White_FT", HintObjectKind::DAISY_WHITE },
	{ "SM_DaisyFlowers_Landscape_FT", HintObjectKind::DAISY_WHITE },
	{ "SM_Mushrooms_Red_FT", HintObjectKind::MUSHROOM_RED },
	{ "SM_Mushrooms_Purple_FT", HintObjectKind::MUSHROOM_PURPLE },
	{ "SM_Mushrooms_Blue_FT", HintObjectKind::MUSHROOM_BLUE },
};
