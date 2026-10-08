#pragma once
constexpr int TICK_RATE = 50;
constexpr int TICK_INTERVAL_MS = 1000 / TICK_RATE;


constexpr int BUF_SIZE = 2048;
constexpr int MAX_MESSAGE_SIZE = 512;
constexpr int SERVER_PORT = 12312;

constexpr int MAX_PLAYER_COUNT = 10000;
constexpr int MAX_ROOM_COUNT = 500;
constexpr int MAX_ROOM_PLAYER_COUNT = 20;
constexpr int MAX_ROOM_NAME_SIZE = 48; // 16자 * UTF-8 1문자 크기(3)
constexpr int MAX_ROOM_PASSWORD_SIZE = 48;
constexpr int MAX_ARRAY_SIZE = 127;
constexpr int ID_SIZE = 16;

constexpr int MAX_PLAYER_NAME_SIZE = 48;
constexpr int MAX_PLAYER_ID_SIZE = 48;
constexpr int MAX_PLAYER_PASSWORD_SIZE = 48;

// 이동 동기화 간격. 클라이언트도 같은 값을 사용한다.
constexpr int MOVE_SEND_INTERVAL_MS = 100; // 클라이언트가 이동 상태를 주기적으로 보내는 간격
constexpr int MOVE_EXTRA_SEND_INTERVAL_MS = 50; // 클라이언트가 이동 방향이 꺾여 주기보다 먼저 보낼 때의 최소 간격. 입력 변화는 제한 없이 즉시 보낸다.
constexpr int MOVE_RELAY_INTERVAL_MS = 40; // 서버가 한 플레이어의 위치를 중계하는 최소 간격. 입력 변화는 다음 틱에 바로 중계한다.
constexpr int MAX_ROOM_ACTIONS_PER_TICK = 80; // 서버가 한 방에서 한 틱에 중계하는 동작 수
constexpr int LOADING_TIMEOUT_MS = 60000; // 게임 시작 후 전원의 로딩 완료를 기다리는 최대 시간. 지나면 남은 플레이어를 기다리지 않고 시작 대기에 들어간다.
constexpr int GAME_READY_WAIT_MS = 30000; // 시작 대기에 들어간 뒤 실제 게임 시작까지 기다리는 시간
constexpr double PLATFORM_SPAWN_PROBABILITY = 0.6; // 반드시 놓는 자리가 아닌 발판을 놓을 확률

// 상자 배치(임시값). 게임 시작 때 유적 상자를 먼저 고르고, 자연 상자는 미첼 최적 후보 방식(spread_selection.h)으로 고른다.
constexpr int NATURAL_BOX_COUNT = 40; // 나무·바위에 놓는 자연 상자 수(유적 상자 제외)
constexpr float BOX_MIN_DISTANCE = 5000.0f; // 자연 상자끼리 최소 수평 거리(cm). 지키는 후보를 계속 못 찾으면 줄여 가며 개수를 채운다.
constexpr int MAX_BOX_COUNT = 64; // 한 판의 상자 수 상한(상자 배치 패킷 크기). 유적 상자 최대 13개와 자연 상자 수를 합친 값보다 커야 한다.
static_assert(NATURAL_BOX_COUNT + 13 <= MAX_BOX_COUNT, "MAX_BOX_COUNT must hold all boxes.");

// 탑 배치. 게임 시작 때 탑 후보 자리(tower_candidates.txt) 중 TOWER_COUNT곳을 미첼 최적 후보 방식으로 고른다.
// 탑(SM_tower_04)은 후보 자리의 높이(바닥에서 가장 높은 땅)에 놓고, 그 아래에 기초(SM_tower_foundation_03)를 붙여 땅까지 채운다.
constexpr int TOWER_COUNT = 20;
constexpr float TOWER_MIN_DISTANCE = 10000.0f; // 탑끼리 최소 수평 거리(cm)
constexpr float TOWER_FOUNDATION_HEIGHT = 1565.0f; // 기초 높이(cm). 기초 바닥은 탑 바닥보다 이만큼 아래다.
constexpr float TOWER_FOUNDATION_WIDTH_SCALE = 1.41f; // 기초 가로·세로 배율. 탑 아래 돌 받침 폭에 맞춘다.

// 최종 보물. 임시로 크기 1의 보물 상자 모델(가로 약 170cm, 높이 130cm) 크기로 보고, 서버가 맵 안 무작위 위치에 놓는다. 위치는 서버만 안다.
constexpr float MAP_MIN[2] = { -100750.0f, -100700.0f }; // 맵 범위(cm). 클라이언트 지도와 같다.
constexpr float MAP_MAX[2] = { 100650.0f, 100700.0f };
constexpr float TREASURE_RADIUS = 85.0f; // 보물을 감싸는 수평 반경(cm). 이 사각형 안의 땅이 모두 있어야 하고, 이 반경의 구가 배치물과 겹치면 다시 뽑는다.
constexpr float TREASURE_HALF_HEIGHT = 65.0f; // 보물 높이의 절반(cm)
constexpr int TREASURE_PLACE_ATTEMPTS = 10000; // 조건에 맞는 위치를 찾을 때까지 다시 뽑는 최대 횟수
constexpr float TREASURE_EDGE_MARGIN = 1000.0f; // 맵 경계에서 이만큼(cm) 안쪽에만 보물을 놓는다. 경계 너머 배경 산이 플레이 영역 가장자리를 덮는 곳이 있다.

// 캐릭터 이동 캡슐. 언리얼 캐릭터 캡슐과 같은 크기이고, 이동 상태의 위치가 이 캡슐의 중심이다.
constexpr float CHARACTER_CAPSULE_RADIUS = 50.0f; // 반지름(cm)
constexpr float CHARACTER_CAPSULE_HALF_HEIGHT = 90.0f; // 중심에서 위·아래 끝까지 거리(cm). 양 끝 반구를 포함한다.

// 이동 검증. 어기면 마지막으로 인정한 위치로 되돌린다.
// 허용치(cm): 캡슐 아래 끝(발)이 랜드스케이프보다 이만큼 넘게 아래면 땅속, 캡슐이 이동을 막는 배치물에 이만큼 넘게 파고들면 겹침,
// 게임 시작 전 캡슐 중심이 돔이 막는 거리(반지름 − 캡슐 반지름)보다 이만큼 넘게 멀면 돔 밖으로 본다.
constexpr float MOVE_TOLERANCE = 20.0f;

// 명중 검증. 지연 보정이 없어 클라이언트 화면과 서버의 위치 차이를 여유로 허용한다.
constexpr float HIT_BASE_MARGIN = 20.0f; // 서버 히트박스를 키우는 기본 여유(cm)
constexpr float HIT_LATENCY_ALLOWANCE_SECONDS = 0.15f; // 속도에 곱해 여유에 더하는 지연 허용 시간(초)
constexpr float HIT_ORIGIN_MAX_OFFSET = 200.0f; // 발사 시작점이 쏜 사람 위치에서 벗어날 수 있는 기본 거리(cm)
constexpr float HIT_OCCLUSION_MARGIN = 5.0f; // 지형 막힘 검사 선분의 양 끝에서 빼는 길이(cm). 맞은 사람이 붙어 선 벽이나 발사 지점 바로 옆 면을 막힘으로 보지 않는다.

// 기절 게이지. 명중당하면 무기별 증가량에 맞은 부위 배율을 곱해 올리고, 초당 감소량만큼 줄어든다. 최대치에 닿으면 기절한다.
constexpr float STUN_GAUGE_MAX = 100.0f;
constexpr float STUN_GAUGE_DECAY_PER_SECOND = 5.0f;
constexpr float STUN_GAUGE_WEAPON_GAINS[] = { 0.0f, 50.0f, 20.0f }; // 무기 값 순서: 곡괭이, 저격총, 권총
constexpr float STUN_GAUGE_PART_MULTIPLIERS[] = { 1.1f, 1.0f, 0.8f, 0.8f, 0.8f, 0.8f }; // 부위 값 순서: 머리, 몸통, 팔, 손, 다리, 발
constexpr int STUN_DURATION_MS = 5000; // 기절 시간. 기절 동안 서버는 그 플레이어의 이동·동작·명중을 받지 않고, 끝나면 게이지는 0이다.
