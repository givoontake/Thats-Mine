#pragma once
// 아이템과 블록.

// 아이템. 상자에서 얻는 S·A 등급 아이템이고, 등급별 값은 S, A 순서다.
// 아이템은 얻은 뒤 그 게임 동안 계속 적용되고 기절해도 잃지 않는다. 같은 종류를 또 얻으면 가장 높은 등급만 적용한다.
// 상자는 종류마다 등급별 개수 × 글로벌 배수만큼 만든다.
constexpr int ITEM_BOX_COUNTS[] = { 1, 2 };
constexpr int ITEM_PICKAXE_CHARGE_REDUCTIONS_MS[] = { 20000, 10000 }; // 곡괭이 1번이 차는 시간에서 빼는 시간
constexpr float ITEM_STUN_GAUGE_DECAYS_PER_SECOND[] = { 6.0f, 5.0f }; // 기절 게이지 초당 감소량
constexpr int ITEM_STUN_GAUGE_DECAY_DELAYS_MS[] = { 3000, 4000 }; // 기절 게이지 감소 대기 시간
constexpr int ITEM_STUN_PROTECTIONS_MS[] = { 60000, 45000 }; // 기절이 끝난 뒤 보호 시간

// 블록 상자. 블록(block_pieces.h)마다 등급과 상관없이 BLOCK_BOX_COUNT × 칸 수별 배수 × 글로벌 배수만큼 상자를 만든다.
// 얻은 블록은 하나마다 그 게임 동안 아무 때나 한 번 쓸 수 있고, 같은 블록을 여러 개 가질 수 있으며, 기절해도 잃지 않는다.
constexpr int BLOCK_BOX_COUNT = 1;
constexpr int BLOCK_BOX_SIZE_MULTIPLIERS[] = { 5, 4, 3, 2, 1 }; // 1칸부터 5칸 블록까지 칸 수별 배수
