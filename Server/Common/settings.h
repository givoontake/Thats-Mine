#pragma once
constexpr int TICK_RATE = 50;
constexpr int TICK_INTERVAL_MS = 1000 / TICK_RATE;


constexpr int BUF_SIZE = 2048;
constexpr int MAX_MESSAGE_SIZE = 512;
constexpr int SERVER_PORT = 12312;

constexpr int MAX_PLAYER_COUNT = 10000;
constexpr int MAX_ROOM_COUNT = 500;
constexpr int MAX_ROOM_PLAYER_COUNT = 20;
constexpr int MIN_START_PLAYER_COUNT = 1; // 게임을 시작할 수 있는 최소 인원. 당분간 혼자서도 시작할 수 있게 1로 둔다(원래 2).
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
constexpr int GAME_TIME_LIMIT_MS = 7200000; // 게임이 시작된 뒤 보물을 찾지 못해도 게임을 끝내는 시간. 끝나면 보물 위치를 알리고 GAME_END_WAIT_MS 뒤 방으로 돌아간다.

// 상자 배치. 게임 시작 때 힌트 상자 수만큼 유적 상자를 먼저 고르고, 나머지 자연 상자는 미첼 최적 후보 방식(spread_selection.h)으로 고른다.
constexpr float BOX_MIN_DISTANCE = 5000.0f; // 자연 상자끼리 최소 수평 거리(cm). 지키는 후보를 계속 못 찾으면 줄여 가며 개수를 채운다.

// 탑 배치. 게임 시작 때 탑 후보 자리(tower_candidates.txt) 중 TOWER_COUNT곳을 미첼 최적 후보 방식으로 고르고, 고른 순서대로 종류를 번갈아 정한다.
// 탑(SM_tower_02, SM_tower_04)은 후보 자리의 높이(바닥에서 가장 높은 땅)에 놓고, 그 아래에 기초(SM_tower_foundation_03)를 붙여 땅까지 채운다.
constexpr int TOWER_KIND_COUNT = 2;
constexpr int TOWER_COUNT_PER_KIND = 10; // 종류마다 놓는 탑 수
constexpr int TOWER_COUNT = TOWER_KIND_COUNT * TOWER_COUNT_PER_KIND;
constexpr float TOWER_MIN_DISTANCE = 10000.0f; // 탑끼리 최소 수평 거리(cm)
constexpr float TOWER_FOUNDATION_HEIGHT = 1565.0f; // 기초 높이(cm). 기초 바닥은 탑 바닥보다 이만큼 아래다.
constexpr float TOWER_FOUNDATION_WIDTH_SCALE = 1.41f; // 기초 가로·세로 배율. 탑 아래 돌 받침 폭에 맞춘다.

// 최종 보물. 임시로 크기 1의 보물 상자 모델(가로 약 170cm, 높이 130cm) 크기로 보고, 서버가 맵 안 무작위 위치에 놓는다. 위치는 서버만 안다.
constexpr float MAP_MIN[2] = { -100750.0f, -100700.0f }; // 맵 범위(cm). 클라이언트 지도와 같다.
constexpr float MAP_MAX[2] = { 100650.0f, 100700.0f };
constexpr float TREASURE_RADIUS = 85.0f; // 보물을 감싸는 수평 반경(cm). 이 사각형 안의 땅이 모두 있어야 하고, 이 반경의 구가 배치물과 겹치면 다시 뽑는다.
constexpr float TREASURE_HALF_HEIGHT = 65.0f; // 보물 높이의 절반(cm)
constexpr int TREASURE_PLACE_ATTEMPTS = 10000; // 조건에 맞는 위치를 찾을 때까지 다시 뽑는 최대 횟수
// 보물 발견. 게임이 시작된 뒤 곡괭이를 휘두를 때 발밑(캡슐 아래 끝)이 보물 바닥 가운데에서 이 거리(cm) 안이면 보물을 발견한다.
constexpr float TREASURE_DIG_DISTANCE = 300.0f;
constexpr int GAME_END_WAIT_MS = 10000; // 보물을 발견한 뒤 게임을 끝내고 방으로 돌아갈 때까지 기다리는 시간
constexpr float TREASURE_EDGE_MARGIN = 1000.0f; // 맵 경계에서 이만큼(cm) 안쪽에만 보물을 놓는다. 경계 너머 배경 산이 플레이 영역 가장자리를 덮는 곳이 있다.

// 힌트 "보물에서 가까운 물체 종류" 순위에 담는 종류 수
constexpr int HINT_NEAREST_KIND_COUNT = 3;
// 개수 힌트. 판마다 물체 종류마다 등급마다 하나씩, 보물 바닥에서 그 등급 반경(cm) 안에 그 종류 물체가 몇 개 있는지 알려 준다.
// 등급은 반경으로 정해지므로 배제 영역 순위로 정하는 등급에서 뺀다.
constexpr float HINT_OBJECT_COUNT_RADII[] = { 1000.0f, 2000.0f, 3000.0f, 5000.0f }; // 등급별 반경(S, A, B, C 순서)
// 방향 힌트 기준 사각형을 영역 스플라인 조종점의 최소·최대 좌표에서 넓히는 거리(cm)
constexpr float HINT_DIRECTION_MARGIN = 1000.0f;
// 보임 힌트("~에서 보물이 묻힌 곳이 보인다")를 만들지. 당분간 끈다.
constexpr bool HINT_VIEW_ENABLED = false;
// 힌트의 "보물이 묻힌 자리가 보인다" 판정. 영역의 판정 위치(발밑 땅 + 3인칭 카메라 높이 140cm)에서 보물 자리 지면까지 그은 선이 하나라도 막히지 않으면 보인다.
constexpr float PLACE_VIEW_END_MARGIN = 30.0f; // 선을 보물 자리 지면 이만큼(cm) 앞에서 멈춘다. 목표 지면 자체에 걸리지 않게 하고, 등진 경사면에 묻힌 자리는 땅 속을 지나 막히게 한다.
// 힌트 등급. 방향 힌트는 배제 영역 순으로 위 절반은 S, 나머지는 A다.
// 방향·개수 힌트를 뺀 힌트는 배제 영역 순으로 정렬해 위에서 HINT_GRADE_S_PERCENT%는 S, 다음 HINT_GRADE_A_PERCENT%는 A, 다음 HINT_GRADE_B_PERCENT%는 B, 나머지는 C로 정한다.
// 배제 영역은 보물 가능 땅 중 그 힌트가 틀린 땅의 비율이고, 보물 가능 땅을 HINT_EXCLUSION_GRID_STEP 간격 격자 지점으로 세어 구한다.
constexpr int HINT_GRADE_S_PERCENT = 10;
constexpr int HINT_GRADE_A_PERCENT = 20;
constexpr int HINT_GRADE_B_PERCENT = 30;
constexpr float HINT_EXCLUSION_GRID_STEP = 5000.0f; // 배제 영역을 셀 격자 간격(cm)
// 힌트 상자. 힌트마다 등급 배수 × 글로벌 배수만큼 상자를 만들어 한 상자에 힌트 하나를 넣는다. 글로벌 배수는 참가 인원 HINT_BOX_PLAYERS_PER_MULTIPLIER명마다 1씩 오르고 탄약 상자에도 쓴다.
constexpr int HINT_BOX_COPIES[] = { 1, 1, 2, 2 }; // 등급별 배수(S, A, B, C 순서)
constexpr int HINT_BOX_PLAYERS_PER_MULTIPLIER = 5;
// 탄약 상자. 무기마다 AMMO_BOX_COUNT × 글로벌 배수만큼 만들고, 한 상자에는 한 무기의 탄만 든다. 열면 그 무기의 소지 탄이 늘어난다.
constexpr int AMMO_BOX_COUNT = 50;
constexpr int AMMO_BOX_ROUNDS[] = { 0, 1, 3 }; // 상자 하나로 느는 탄 수. 무기 값 순서: 곡괭이, 저격총, 권총. 0이면 그 무기의 탄약 상자를 만들지 않는다.
// 힌트 상자 열기 검증(임시값). 게임이 시작된 뒤에만 열 수 있다.
constexpr float HINT_BOX_OPEN_DISTANCE = 150.0f; // F키로 열 때 캡슐 중심에서 상자 중심까지 최대 거리(cm)
constexpr float HINT_BOX_CENTER_HEIGHTS[] = { 45.5f, 53.1f }; // 상자 바닥 가운데에서 상자 중심까지 상자 위쪽 방향 거리(cm). BoxKind 순서(0.7배 나무 상자, 선물)
constexpr float HINT_BOX_SHOT_TARGET_HEIGHT = 30.0f; // 쏴서 열 때 막힘을 검사할 상자 쪽 끝점을 상자 바닥 가운데에서 올리는 높이(cm)
constexpr float HINT_BOX_SHOT_END_MARGIN = 50.0f; // 쏴서 열 때 막힘 검사 선분의 상자 쪽 끝에서 빼는 길이(cm). 상자를 받친 가지나 바닥을 막힘으로 보지 않는다.
// 떨어진 힌트. 기절하면 가진 힌트 중 하나를 그 자리에 떨어뜨리고, 게임이 끝날 때까지 남는다. 떨어뜨린 사람도 F키로 주울 수 있다.
constexpr float HINT_DROP_PICKUP_DISTANCE = 150.0f; // F키로 주울 때 캡슐 중심에서 떨어진 위치까지 최대 수평 거리(cm)
constexpr float HINT_DROP_PICKUP_HEIGHT = 100.0f; // 주울 때 캡슐 중심이 떨어진 위치보다 높아도 되는 거리이자, 그 아래 랜드스케이프에 선 높이보다 낮아도 되는 거리(cm)
// 영역 지우기. 게임 시작부터 AREA_REMOVAL_INTERVAL_MS마다 배제 판 칸 중 보물 칸과 이미 지운 칸을 뺀 칸 하나를 골라 플레이 중인 전원의 배제 판에서 지운다.
// 지울 칸이 없으면 그 차례는 건너뛴다.
constexpr int AREA_REMOVAL_INTERVAL_MS = 180000;

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

// 탄약 검증. 재장전 완료와 발사 간격, 곡괭이 충전은 클라이언트보다 늦게 받은 만큼 이 시간(ms)을 봐준다.
constexpr long long AMMO_LATENCY_ALLOWANCE_MS = 150;
// 곡괭이 사용 가능 횟수. 게임마다 최대치로 시작하고, 최대치보다 적으면 PICKAXE_CHARGE_MS마다 1번씩 찬다.
constexpr int PICKAXE_MAX_CHARGES = 3;
constexpr int PICKAXE_CHARGE_MS = 120000;

// 기절 게이지. 명중당하면 무기별 증가량에 맞은 부위 배율을 곱해 올리고, 마지막으로 오른 뒤 감소 대기 시간이 지나면 초당 감소량만큼 줄어든다. 최대치에 닿으면 기절한다.
constexpr float STUN_GAUGE_MAX = 100.0f;
constexpr float STUN_GAUGE_DECAY_PER_SECOND = 4.0f;
constexpr int STUN_GAUGE_DECAY_DELAY_MS = 5000;
constexpr float STUN_GAUGE_WEAPON_GAINS[] = { 0.0f, 50.0f, 20.0f }; // 무기 값 순서: 곡괭이, 저격총, 권총
constexpr float STUN_GAUGE_PART_MULTIPLIERS[] = { 1.1f, 1.0f, 0.8f, 0.8f, 0.8f, 0.8f }; // 부위 값 순서: 머리, 몸통, 팔, 손, 다리, 발
constexpr int STUN_DURATION_MS = 5000; // 기절 시간. 기절 동안 서버는 그 플레이어의 이동·동작·명중을 받지 않고, 끝나면 게이지는 0이다.
constexpr int STUN_PROTECTION_MS = 30000; // 기절이 끝난 뒤 보호 시간. 보호 동안 명중당해도 기절 게이지가 오르지 않는다.

// 아이템. 상자에서 얻는 S·A 등급 아이템이고, 등급별 값은 S, A 순서다.
// 넓은 탐지 돋보기 외의 아이템은 얻은 뒤 그 게임 동안 계속 적용되고 기절해도 잃지 않는다. 같은 종류를 또 얻으면 가장 높은 등급만 적용한다.
// 넓은 탐지 돋보기는 가진 개수만큼 한 번씩 쓴다. 상자는 종류마다 등급별 개수 × 글로벌 배수만큼 만든다.
constexpr int ITEM_BOX_COUNTS[] = { 1, 2 };
constexpr int ITEM_PICKAXE_CHARGE_REDUCTIONS_MS[] = { 60000, 30000 }; // 곡괭이 1번이 차는 시간에서 빼는 시간
constexpr float ITEM_STUN_GAUGE_DECAYS_PER_SECOND[] = { 6.0f, 5.0f }; // 기절 게이지 초당 감소량
constexpr int ITEM_STUN_GAUGE_DECAY_DELAYS_MS[] = { 3000, 4000 }; // 기절 게이지 감소 대기 시간
constexpr int ITEM_STUN_PROTECTIONS_MS[] = { 60000, 45000 }; // 기절이 끝난 뒤 보호 시간
constexpr float ITEM_MAGNIFIER_RADII[] = { 3000.0f, 4000.0f }; // 기본 돋보기 탐색 반경(cm). 좁을수록 찾았을 때 보물 자리를 더 좁힌다.
constexpr int ITEM_MAGNIFIER_COOLDOWNS_MS[] = { 80000, 100000 }; // 기본 돋보기 재사용 대기시간
// 돋보기 탐색. 클릭하면 MAGNIFIER_SEARCH_MS 동안 움직이지 않고 탐색한 뒤, 탐색을 시작한 발밑에서 보물 바닥까지 거리가 반경 안인지 알려 준다.
// 기본 돋보기는 모두 처음부터 가지고 횟수 제한 없이 쓰지만, 탐색을 마칠 때마다 재사용 대기시간이 지나야 다시 쓸 수 있다. 넓은 탐지 돋보기는 1회용이다.
constexpr float MAGNIFIER_RADIUS = 5000.0f; // 기본 돋보기 탐색 반경(cm)
constexpr int MAGNIFIER_COOLDOWN_MS = 120000; // 기본 돋보기 재사용 대기시간
constexpr float WIDE_MAGNIFIER_RADII[] = { 20000.0f, 10000.0f }; // 넓은 탐지 돋보기 탐색 반경(cm)
constexpr int MAGNIFIER_SEARCH_MS = 2000;
// 못 찾은 탐색으로 칸을 배제할 때 칸의 땅 높이 범위를 위아래로 넓히는 여유(cm). 땅 높이를 MAGNIFIER_HEIGHT_SAMPLE_STEP 간격으로만 재서 생기는 오차를 덮는다.
constexpr float MAGNIFIER_HEIGHT_MARGIN = 300.0f;
constexpr float MAGNIFIER_HEIGHT_SAMPLE_STEP = 500.0f;
constexpr float MAGNIFIER_MOVE_TOLERANCE = 100.0f; // 탐색이 끝날 때 시작 위치에서 벗어나도 되는 수평 거리(cm). 벗어나면 탐색을 취소하고 돋보기를 쓰지 않은 것으로 본다.

// 블록 상자. 블록(block_pieces.h)마다 등급과 상관없이 BLOCK_BOX_COUNT × 글로벌 배수만큼 상자를 만든다. 얻은 블록은 그 게임 동안 아무 때나 한 번 쓸 수 있고 기절해도 잃지 않는다.
constexpr int BLOCK_BOX_COUNT = 1;
