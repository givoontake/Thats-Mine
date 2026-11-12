#pragma once
#include "equipment_types.h"

// 장비 종류별 고정값. 서버 장비와 클라이언트 무기 설정이 같은 상수를 쓴다.
// 탄창이 빈 총으로 바꾸면 교체 대기가 끝날 때 자동 재장전한다.

constexpr PickaxeSettings PICKAXE_SETTINGS{ { 200, 0.0f, false }, 3, 60000 };
constexpr GunSettings SNIPER_SETTINGS{ { 500, 50.0f, true }, 50000.0f, 3, 2000, 1000, { 5.0f, 5.0f, 0.0f, 0.5f, 0.0f, 0, 0 } };
// 권총 퍼짐은 1·2발째가 기본이고 3발째부터 0.1도씩 커져 7발째에 최대다.
constexpr GunSettings PISTOL_SETTINGS{ { 500, 20.0f, false }, 10000.0f, 7, 1000, 200, { 0.5f, 1.0f, 0.0f, 0.0f, 0.1f, 1, 6 } };
constexpr EquipmentSettings MAGNIFIER_SETTINGS{ 200, 0.0f, false };
