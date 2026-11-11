#pragma once
// 맵과 배치물. 보물, 상자 배치와 열기, 힌트.

// 상자 배치. 게임 시작 때 유적별 규칙대로 유적 상자를 먼저 고르고, 나머지 자연 상자는 미첼 최적 후보 방식(spread_selection.h)으로 고른다.
constexpr float BOX_MIN_DISTANCE = 5000.0f; // 자연 상자끼리 최소 수평 거리(cm). 지키는 후보를 계속 못 찾으면 줄여 가며 개수를 채운다.

// 최종 보물. 임시로 크기 1의 보물 상자 모델(가로 약 170cm, 높이 130cm) 크기로 보고, 서버가 맵 안 무작위 위치에 놓는다. 위치는 서버만 안다.
constexpr float MAP_MIN[2] = { -100750.0f, -100700.0f }; // 맵 범위(cm). 클라이언트 지도와 같다.
constexpr float MAP_MAX[2] = { 100650.0f, 100700.0f };
constexpr float TREASURE_RADIUS = 85.0f; // 보물을 감싸는 수평 반경(cm). 이 사각형 안의 땅이 모두 있어야 하고, 이 반경의 구가 배치물과 겹치면 다시 뽑는다.
constexpr float TREASURE_HALF_HEIGHT = 65.0f; // 보물 높이의 절반(cm)
constexpr int TREASURE_PLACE_ATTEMPTS = 10000; // 조건에 맞는 위치를 찾을 때까지 다시 뽑는 최대 횟수
constexpr float TREASURE_EDGE_MARGIN = 1000.0f; // 맵 경계에서 이만큼(cm) 안쪽에만 보물을 놓는다. 경계 너머 배경 산이 플레이 영역 가장자리를 덮는 곳이 있다.
// 보물 발견. 게임이 시작된 뒤 곡괭이를 휘두를 때 발밑(캡슐 아래 끝)이 보물 바닥 가운데에서 이 거리(cm) 안이면 보물을 발견한다.
constexpr float TREASURE_DIG_DISTANCE = 300.0f;
// 시험용 보물 위치 공개. 켜면 방장의 요청(클라이언트 F9)에 플레이 중인 전원에게 보물 위치를 알려 준다. 꺼져 있으면 요청을 버린다.
constexpr bool IS_TREASURE_REVEAL_ENABLED = false;

// 개수 힌트. 판마다 물체 종류마다 반경마다 하나씩, 보물 바닥에서 그 반경(cm) 안에 그 종류 물체가 몇 개 있는지 알려 준다.
constexpr float HINT_OBJECT_COUNT_RADII[] = { 3000.0f, 1000.0f };
constexpr float HINT_OBJECT_GRID_CELL_SIZE = 4000.0f; // 개수 힌트에 쓰는 힌트 물체 격자 한 칸의 크기(cm)
// 방향 힌트 기준 사각형을 영역 스플라인 조종점의 최소·최대 좌표에서 넓히는 거리(cm)
constexpr float HINT_DIRECTION_MARGIN = 1000.0f;
// 힌트 상자. 힌트마다 HINT_BOX_COPIES × 글로벌 배수만큼 상자를 만들어 한 상자에 힌트 하나를 넣는다. 글로벌 배수는 참가 인원 HINT_BOX_PLAYERS_PER_MULTIPLIER명마다 1씩 오르고 탄약 상자에도 쓴다.
constexpr int HINT_BOX_COPIES = 2;
constexpr int HINT_BOX_PLAYERS_PER_MULTIPLIER = 5;
// 힌트 상자 열기 검증(임시값). 게임이 시작된 뒤에만 열 수 있다.
constexpr float HINT_BOX_OPEN_DISTANCE = 225.0f; // F키로 열 때 캡슐 중심에서 상자 중심까지 최대 거리(cm)
constexpr float HINT_BOX_INTERACT_SCALE = 1.5f; // 상호작용 범위는 상자 메시 경계 상자를 이 배율로 키운 상자다. F키로 열 때 시선이 이 범위에 닿아야 한다. 클라이언트도 같은 값을 쓴다.
// 선물은 탄에 맞으면 열리고, 나무 상자는 탄을 막기만 한다. 모양은 서버 상자 모양 파일의 직육면체다.
constexpr int CHEST_OPEN_SHAPE_DELAY_MS = 500; // 나무 상자가 열린 뒤 열린 모양으로 바뀔 때까지 시간. 그 전에는 닫힌 모양이다. 클라이언트 여는 동작 시간과 같다.
