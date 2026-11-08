#pragma once
constexpr int TICK_RATE = 50;
constexpr int TICK_INTERVAL_MS = 1000 / TICK_RATE;


constexpr int BUF_SIZE = 2048;
constexpr int SERVER_PORT = 12312;

constexpr int MAX_PLAYER_COUNT = 10000;
constexpr int MAX_ROOM_COUNT = 500;
constexpr int MAX_ROOM_PLAYER_COUNT = 20;
constexpr int ROOM_LIST_REFRESH_INTERVAL_MS = 1000; // 로비가 방 목록 응답을 새로 만드는 간격. 방 목록 요청에는 마지막으로 만든 목록을 보낸다.
constexpr int DISCONNECT_CLEANUP_INTERVAL_MS = 1000; // 타이머가 연결 종료 정리를 입출력 스레드에 맡기는 간격
constexpr int ACCEPT_RETRY_INTERVAL_MS = 1000; // 자원 부족 등으로 다시 걸지 못한 접속 수락 요청을 다시 거는 간격
constexpr int DB_RECONNECT_INTERVAL_MS = 1000; // DB 연결이 끊겼을 때 다시 연결을 시도하는 간격. 성공할 때까지 계속 시도한다.
constexpr int DB_REQUEST_TIMEOUT_MS = 10000; // 로그인 요청이 DB 처리를 기다릴 수 있는 최대 시간. 넘으면 시간 초과로 응답하고 버린다.
constexpr int DB_CONNECT_TIMEOUT_SECONDS = 2; // DB 연결 시도 한 번이 기다리는 최대 시간. DB 서버가 응답 없이 사라졌을 때 연결 시도에 오래 묶이지 않게 한다.
constexpr int DB_IO_TIMEOUT_SECONDS = 5; // 연결된 뒤 쿼리를 주고받을 때 기다리는 최대 시간. 조회 중 DB 서버가 사라져도 끝없이 기다리지 않는다.
constexpr int MIN_START_PLAYER_COUNT = 1; // 게임을 시작할 수 있는 최소 인원. 당분간 혼자서도 시작할 수 있게 1로 둔다(원래 2).
constexpr int MAX_ROOM_NAME_SIZE = 48; // 16자 * UTF-8 1문자 크기(3)
constexpr int MAX_ROOM_PASSWORD_SIZE = 48;

constexpr int MAX_PLAYER_NAME_SIZE = 48;
constexpr int MAX_PLAYER_ID_SIZE = 48;
constexpr int MAX_PLAYER_PASSWORD_SIZE = 48;

// 이동 동기화 간격. 클라이언트도 같은 값을 사용한다.
constexpr int MOVE_SEND_INTERVAL_MS = 100; // 클라이언트가 이동 상태를 주기적으로 보내는 간격
constexpr int MOVE_EXTRA_SEND_INTERVAL_MS = 50; // 클라이언트가 이동 방향이 꺾여 주기보다 먼저 보낼 때의 최소 간격. 공중 단계가 바뀌면 제한 없이 즉시 보낸다.
constexpr int LOADING_TIMEOUT_MS = 60000; // 게임 시작 후 전원의 로딩 완료를 기다리는 최대 시간. 지나면 남은 플레이어를 기다리지 않고 시작 대기에 들어간다.
constexpr int GAME_READY_WAIT_MS = 30000; // 시작 대기에 들어간 뒤 실제 게임 시작까지 기다리는 시간
constexpr int GAME_TIME_LIMIT_MS = 7200000; // 게임이 시작된 뒤 보물을 찾지 못해도 게임을 끝내는 시간. 끝나면 보물 위치를 알리고 GAME_END_WAIT_MS 뒤 방으로 돌아간다.

// 상자 배치. 게임 시작 때 유적별 규칙대로 유적 상자를 먼저 고르고, 나머지 자연 상자는 미첼 최적 후보 방식(spread_selection.h)으로 고른다.
constexpr float BOX_MIN_DISTANCE = 5000.0f; // 자연 상자끼리 최소 수평 거리(cm). 지키는 후보를 계속 못 찾으면 줄여 가며 개수를 채운다.

// 최종 보물. 임시로 크기 1의 보물 상자 모델(가로 약 170cm, 높이 130cm) 크기로 보고, 서버가 맵 안 무작위 위치에 놓는다. 위치는 서버만 안다.
constexpr float MAP_MIN[2] = { -100750.0f, -100700.0f }; // 맵 범위(cm). 클라이언트 지도와 같다.
constexpr float MAP_MAX[2] = { 100650.0f, 100700.0f };
constexpr float TREASURE_RADIUS = 85.0f; // 보물을 감싸는 수평 반경(cm). 이 사각형 안의 땅이 모두 있어야 하고, 이 반경의 구가 배치물과 겹치면 다시 뽑는다.
constexpr float TREASURE_HALF_HEIGHT = 65.0f; // 보물 높이의 절반(cm)
constexpr int TREASURE_PLACE_ATTEMPTS = 10000; // 조건에 맞는 위치를 찾을 때까지 다시 뽑는 최대 횟수
// 보물 발견. 게임이 시작된 뒤 곡괭이를 휘두를 때 발밑(캡슐 아래 끝)이 보물 바닥 가운데에서 이 거리(cm) 안이면 보물을 발견한다.
constexpr float TREASURE_DIG_DISTANCE = 300.0f;
constexpr int GAME_END_WAIT_MS = 10000; // 보물을 발견한 뒤 게임을 끝내고 시상식으로 넘어갈 때까지 기다리는 시간
constexpr int AWARD_SCENE_MS = 5000; // 시상식에서 부문 하나를 보여 주는 시간
constexpr int AWARD_END_MARGIN_MS = 1000; // 시상식이 끝난 뒤 방으로 돌려보낼 때까지 더 기다리는 시간. 마지막 화면 전환이 끝나게 둔다.
constexpr float TREASURE_EDGE_MARGIN = 1000.0f; // 맵 경계에서 이만큼(cm) 안쪽에만 보물을 놓는다. 경계 너머 배경 산이 플레이 영역 가장자리를 덮는 곳이 있다.
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
// 탄약 상자. 무기마다 AMMO_BOX_COUNT × 글로벌 배수만큼 만들고, 한 상자에는 한 무기의 탄만 든다. 열면 그 무기의 소지 탄이 늘어난다.
constexpr int AMMO_BOX_COUNT = 50;
constexpr int AMMO_BOX_ROUNDS[] = { 0, 1, 3 }; // 상자 하나로 느는 탄 수. 무기 값 순서: 곡괭이, 저격총, 권총. 0이면 그 무기의 탄약 상자를 만들지 않는다.
// 힌트 상자 열기 검증(임시값). 게임이 시작된 뒤에만 열 수 있다.
constexpr float HINT_BOX_OPEN_DISTANCE = 225.0f; // F키로 열 때 캡슐 중심에서 상자 중심까지 최대 거리(cm)
constexpr float HINT_BOX_INTERACT_SCALE = 1.5f; // 상호작용 범위는 상자 메시 경계 상자를 이 배율로 키운 상자다. F키로 열 때 시선이 이 범위에 닿아야 한다. 클라이언트도 같은 값을 쓴다.
// 선물은 탄에 맞으면 열리고, 나무 상자는 탄을 막기만 한다. 모양은 서버 상자 모양 파일의 직육면체다.
constexpr int CHEST_OPEN_SHAPE_DELAY_MS = 500; // 나무 상자가 열린 뒤 열린 모양으로 바뀔 때까지 시간. 그 전에는 닫힌 모양이다. 클라이언트 여는 동작 시간과 같다.
// 영역 지우기. 게임 시작부터 AREA_REMOVAL_INTERVAL_MS마다 배제 판 칸 중 보물 칸과 이미 지운 칸을 뺀 칸 하나를 골라 플레이 중인 전원의 배제 판에서 지운다.
// 지울 칸이 없으면 그 차례는 건너뛴다.
constexpr int AREA_REMOVAL_INTERVAL_MS = 180000;

// 캐릭터 최대 이동 속도(cm/s). 클라이언트 이동과 서버 이동 검증이 같은 값을 쓴다.
// 뒤로 이동하면 뒤 속도, 조준 중에는 방향과 관계없이 조준 속도다.
constexpr float MOVE_SPEED_FORWARD = 600.0f;
constexpr float MOVE_SPEED_BACKWARD = 300.0f;
constexpr float MOVE_SPEED_AIMING = 150.0f;
// 땅에서의 가감속. 언리얼 캐릭터 이동 설정과 같은 값이다. 상한보다 느리면 최대 가속으로 일정하게 빨라지고,
// 상한보다 빠르면 "지면 마찰 × 제동 마찰 배수 × 속도 + 제동 감속"으로 느려진다.
constexpr float MOVE_MAX_ACCELERATION = 2048.0f;
constexpr float MOVE_BRAKING_DECELERATION = 2048.0f;
constexpr float MOVE_GROUND_FRICTION = 8.0f;
constexpr float MOVE_BRAKING_FRICTION_FACTOR = 2.0f;
constexpr float MOVE_MAX_STEP_HEIGHT = 60.0f; // 땅에서 걸어서 오르내릴 수 있는 높이(cm). 언리얼 캐릭터 이동 설정과 같은 값이다.

// 점프와 낙하. 클라이언트 캐릭터 이동과 서버 이동 계산이 같은 값을 쓴다.
// 중력은 프로젝트 기본 중력, 낙하 속도 상한은 게임 레벨 기본 물리 볼륨의 값과 같다.
constexpr float JUMP_Z_VELOCITY = 500.0f; // 점프 순간 위쪽 속도(cm/s)
constexpr float GRAVITY = 980.0f; // 중력 가속도(cm/s²)
constexpr float TERMINAL_FALL_SPEED = 4000.0f; // 낙하 속도 상한(cm/s)

// 캐릭터 이동 캡슐. 언리얼 캐릭터 캡슐과 같은 크기이고, 이동 상태의 위치가 이 캡슐의 중심이다.
constexpr float CHARACTER_CAPSULE_RADIUS = 50.0f; // 반지름(cm)
constexpr float CHARACTER_CAPSULE_HALF_HEIGHT = 90.0f; // 중심에서 위·아래 끝까지 거리(cm). 양 끝 반구를 포함한다.

// 공용 이동 계산(SharedMovement). 언리얼 캐릭터 이동의 엔진 고정값과 이 프로젝트 캐릭터 설정값을 그대로 쓴다.
// 걸을 때 바닥과의 거리(cm)가 이 범위 안이면 그대로 두고, 벗어나면 가운데로 맞춘다.
constexpr float MOVE_MIN_FLOOR_DISTANCE = 1.9f;
constexpr float MOVE_MAX_FLOOR_DISTANCE = 2.4f;
// 한 번 부를 때 이동을 이 시간(초) 이하 단계로 나누고, 단계는 이 횟수까지만 진행한다.
constexpr float MOVE_MAX_SIMULATION_TIME_STEP = 0.05f;
constexpr int MOVE_MAX_SIMULATION_ITERATIONS = 8;
// 이보다 짧은 시간(초)은 진행하지 않는다.
constexpr float MOVE_MIN_TICK_TIME = 1e-6f;
// 제동 중 이 속도(cm/s) 이하가 되면 멈춘다.
constexpr float MOVE_BRAKE_TO_STOP_SPEED = 10.0f;
// 쓸기가 닿은 점이 캡슐 가장자리에서 이 거리(cm) 안이면 바닥·착지로 보지 않는다.
constexpr float MOVE_SWEEP_EDGE_REJECT_DISTANCE = 0.15f;
// 처음부터 겹쳐 있으면 빠져나올 거리에 이만큼(cm) 더 밀어내고, 빠져나온 자리가 비었는지는 캡슐을 이만큼 키워 본다.
constexpr float MOVE_PENETRATION_PULLBACK_DISTANCE = 0.125f;
constexpr float MOVE_PENETRATION_OVERLAP_INFLATION = 0.1f;
// 턱 옆면으로 보는 면 법선의 위쪽 성분 상한
constexpr float MOVE_MAX_STEP_SIDE_Z = 0.08f;
// 걸을 수 있는 가장 가파른 면의 법선 위쪽 성분. 캐릭터 이동 설정의 걸을 수 있는 경사(약 44.8도)로 언리얼이 정한 값이다.
constexpr float MOVE_WALKABLE_FLOOR_Z = 0.71f;
// 가장자리에서 떨어지지 않게 할 때 옆으로 비켜 갈 자리 아래 바닥을 턱 높이보다 이만큼(cm) 더 내려가 찾는다. 캐릭터 이동 설정과 같은 값이다.
constexpr float MOVE_LEDGE_CHECK_THRESHOLD = 4.0f;
// 속도가 최고 속도의 제곱에 이 비율을 곱한 값을 넘어야 상한을 넘었다고 본다. 반올림으로 살짝 넘는 것은 넘지 않은 것으로 본다.
constexpr float MOVE_OVER_SPEED_RATIO = 1.01f;
// 미끄러지다 같은 벽에 또 닿으면 반올림 오차로 보고 벽에서 이만큼(cm) 떼어 낸다.
constexpr float MOVE_SAME_WALL_NUDGE_DISTANCE = 0.01f;
// 이동이 막히면 닿은 자리에서 이동 길이의 MOVE_HIT_PULLBACK_RATIO만큼 되돌리되, 그 거리는 아래 최소·최대(cm) 사이로 둔다. 이동의 MOVE_HIT_PULLBACK_EXTRA_RATIO만큼 더 되돌린다.
constexpr float MOVE_HIT_PULLBACK_RATIO = 0.1f;
constexpr float MOVE_HIT_PULLBACK_MIN_DISTANCE = 0.1f;
constexpr float MOVE_HIT_PULLBACK_MAX_DISTANCE = 1.0f;
constexpr float MOVE_HIT_PULLBACK_EXTRA_RATIO = 0.001f;
// 이보다 짧은 이동(cm)은 하지 않는다.
constexpr float MOVE_MIN_MOVE_DISTANCE = 4e-4f;
// 두 번째 면에 닿아 다시 정한 미끄러짐 이동이 축마다 이 거리(cm) 안이면 가지 않는다.
constexpr float MOVE_MIN_SLIDE_DISTANCE = 1e-3f;
// 바닥 찾기는 캡슐 원기둥 부분을 이 비율로 줄여 아래로 쓴다. 가장자리에 걸려 다시 쓸 때는 두 번째 비율로 줄인다.
constexpr float MOVE_FLOOR_SWEEP_SHRINK_SCALE = 0.9f;
constexpr float MOVE_FLOOR_SWEEP_RETRY_SHRINK_SCALE = 0.1f;
// 점프 정점을 지나는 단계는 정점에서 나눈다. 한 번 진행할 때 이 횟수까지만 나누고, 정점까지 이 시간(초)보다 짧게 남았으면 나누지 않는다.
constexpr int MOVE_MAX_JUMP_APEX_ATTEMPTS = 2;
constexpr float MOVE_JUMP_APEX_MIN_TIME = 1e-4f;
// 서버 캡슐 쓸기. 캡슐과 도형 사이가 이 거리(cm) 안으로 좁혀지면 닿은 것으로 본다. Chaos 쓸기의 수렴 기준과 같은 값이다.
constexpr float MOVE_SWEEP_CONTACT_DISTANCE = 1e-3f;
// 볼록 도형에 닿았을 때 닿은 점에서 이 거리(cm) 안의 면 가운데 이동과 가장 맞서는 면을 닿은 면으로 본다. Chaos 쓸기와 같은 값이다.
constexpr float MOVE_SWEEP_FACE_SEARCH_DISTANCE = 1.0f;

// 이동 검증. 어기면 클라이언트를 서버 이동 상태로 맞춘다.
// 허용치(cm): 캡슐 아래 끝(발)이 랜드스케이프보다 이만큼 넘게 아래면 땅속이고, 받은 위치가 그 시각의 서버 이동 상태에서 이만큼 넘게 벗어나면 인정하지 않는다.
// 서버 이동 상태에서 받은 위치까지 쓸 때 캡슐을 이만큼 줄인다.
constexpr float MOVE_TOLERANCE = 20.0f;
// 공중 이동 끊김. 공중에서 이 시간(초) 넘게 이동 상태가 오지 않으면 그 클라이언트를 서버 이동 상태(공중 궤적)로 맞춘다.
constexpr float MOVE_AIR_DELAY_SECONDS = 0.5f;
// 이동 끊김. 이 시간(초) 넘게 이동 상태가 오지 않으면 서버가 그 플레이어를 서버 이동 상태 자리에 세우고 다른 플레이어에게 멈춤을 알린다. 그 전까지는 서버 이동 상태를 계속 중계한다.
// 공중 이동 끊김과 같은 값이다.
constexpr float MOVE_STOP_DELAY_SECONDS = MOVE_AIR_DELAY_SECONDS;
// 땅 확인. 캡슐 아래 이만큼(cm) 안에 랜드스케이프나 캐릭터를 막는 배치물 바닥이 있으면 땅 위다. 걸을 수 있는 가장 가파른 경사를 덮는 깊이다.
constexpr float GROUND_CHECK_DEPTH = 30.0f;
// 클라이언트 시각 검사. 이동 상태 사이 "클라이언트 시각 차이 - 서버가 받은 시각 차이"를 쌓아 클라이언트 시각이 이만큼(ms) 넘게 앞서면 시간을 빠르게 돌린 것으로 보고 되돌린다.
// 늦은 쪽은 CLOCK_MAX_BEHIND_MS까지만 쌓아, 오래 끊겼다가 한 번에 몰아 보내는 몫을 무한히 미리 쌓아 두지 못하게 한다.
constexpr long long CLOCK_MAX_AHEAD_MS = 250;
constexpr long long CLOCK_MAX_BEHIND_MS = 2000;

// 명중 판정. 서버가 탄 광선을 쏴서 처음 닿는 것을 맞힌다. 다른 플레이어는 사격 시각으로 되감은 위치·자세·기절 상태로 판정한다.
constexpr float HIT_MARGIN = 0.0f; // 서버 히트박스를 키우는 여유(cm). 시험으로 정한다.
constexpr int HIT_REWIND_MS = 1000; // 사격 시각을 되감는 최대 시간. 받은 시각보다 이보다 오래된 사격은 이 시간 전으로 맞춰 판정한다.
constexpr float HIT_LATENCY_ALLOWANCE_SECONDS = 0.15f; // 카메라 검증에서 다른 플레이어 캡슐을 속도에 비례해 키우는 지연 허용 시간(초)
// 탄 광선은 캡슐 안 오른쪽 어깨 높이의 고정점에서 시작한다. 캐릭터 위치(캡슐 중심)에서 몸 방향 기준 앞·오른쪽·위로 이만큼(cm) 옮긴 점이다.
// 캡슐은 벽에 파고들지 못하므로 시작점이 벽 너머에 있을 수 없다. 총구는 화염·궤적 표시에만 쓴다. 클라이언트와 서버가 같은 식으로 구한다.
constexpr float SHOT_ORIGIN_FORWARD = 10.0f;
constexpr float SHOT_ORIGIN_RIGHT = 12.0f;
constexpr float SHOT_ORIGIN_UP = -20.0f;

// 카메라. 클라이언트 카메라 설정과 서버 명중 검증이 같은 값을 쓴다.
// 3인칭 카메라 팔은 캡슐 중심에서 시선 반대 방향으로 팔 길이만큼, 시선 기준 오른쪽·위로 어깨 오프셋만큼 간 곳이 끝이고, 벽에 막히면 막힌 곳까지 당겨진다.
// 카메라 충돌 구 반지름, 팔 끝이 땅에서 떨어져 있어야 하는 높이(올려다볼 때 팔 전체를 올린다), 팔 방향보다 아래로 숙인 카메라 각도(도)다.
constexpr float CAMERA_ARM_LENGTH = 300.0f;
constexpr float CAMERA_SOCKET_RIGHT = 50.0f;
constexpr float CAMERA_SOCKET_UP = 100.0f;
constexpr float CAMERA_PROBE_RADIUS = 12.0f;
constexpr float CAMERA_GROUND_CLEARANCE = 100.0f;
constexpr float CAMERA_PITCH_DOWN_DEGREES = 8.0f;
// 조준 카메라는 캡슐 중심에서 몸 방향 앞·위로 이만큼(cm) 간 곳에 있고, 걸음마다 위아래로 AIM_CAMERA_BOB_RANGE까지 흔들린다.
constexpr float AIM_CAMERA_FORWARD = 66.9f;
constexpr float AIM_CAMERA_UP = 18.7f;
constexpr float AIM_CAMERA_BOB_RANGE = 30.0f;
// 명중 카메라 검증. 받은 카메라 위치가 서버가 계산한 자리에서 이만큼(cm)과 "속도 × CAMERA_FRAME_ALLOWANCE_SECONDS"까지 벗어나도 된다.
// 카메라는 이전 프레임에 자리를 정하므로 그만큼 움직인 몫을 봐준다. 3인칭 카메라에서 맞은 곳은 카메라 방향에서 CAMERA_VIEW_CONE_DEGREES 안이어야 한다.
constexpr float CAMERA_POSITION_TOLERANCE = 15.0f;
constexpr float CAMERA_FRAME_ALLOWANCE_SECONDS = 0.05f;
constexpr float CAMERA_VIEW_CONE_DEGREES = 45.0f;

// 탄 퍼짐(shot_spread.h). 서버가 퍼짐 크기를 정한다. 서버가 받은 발사 시각으로 센 연사 열기를 SHOT_SPREAD_TIME_ALLOWANCE_SECONDS만큼 더 회복한 값으로 정하고,
// 공중이나 조준하지 않은 상태는 이동 상태에서 SHOT_SPREAD_STATE_ALLOWANCE_MS 넘게 이어졌을 때만 그 퍼짐을 쓴다. 상태가 막 바뀐 것은 지연으로 보고 봐준다.
constexpr float SHOT_SPREAD_TIME_ALLOWANCE_SECONDS = 0.2f;
constexpr long long SHOT_SPREAD_STATE_ALLOWANCE_MS = 300;
constexpr float HIT_OCCLUSION_MARGIN = 5.0f; // 지형 막힘 검사 선분의 양 끝에서 빼는 길이(cm). 맞은 사람이 붙어 선 벽이나 발사 지점 바로 옆 면을 막힘으로 보지 않는다.

// 발사 간격 검증. 무기마다 직전 인정 발사와의 동작 시각 간격에 보상(ms)을 더해 발사 간격 이상이면 인정한다.
// 인정하면 서버가 받은 간격과 동작 시각 간격의 차이를 보상에 더한다. 서버가 늦게 받았으면 양수, 일찍 받았으면 음수다.
// 발사 간격보다 늦게 쏜 만큼은 지난 보상이나 빚을 0 쪽으로 줄인다. 보상은 FIRE_REWARD_MAX_MS까지 쌓이고,
// 음수 보상은 FIRE_DEBT_FREE_MS를 넘은 몫만 간격에서 깎는다. 동작 시각이 서버가 받은 시각보다 늦으면 인정하지 않는다.
constexpr long long FIRE_REWARD_MAX_MS = 500;
constexpr long long FIRE_DEBT_FREE_MS = 500;
// 장비 교체 대기. 다른 무기로 바꾼 동작 시각부터 이 시간(ms) 동안 발사·휘두르기·재장전을 받지 않는다. 클라이언트도 같은 값을 쓴다.
// 탄창이 빈 무기로 바꾸면 대기가 끝날 때 자동 재장전한다. 무기 값 순서: 곡괭이, 저격총, 권총, 돋보기.
constexpr int EQUIP_DURATIONS_MS[] = { 200, 500, 500, 200 };
// 총 수치. 무기 값 순서: 곡괭이, 저격총, 권총, 돋보기. 총이 아닌 무기는 0이다. 클라이언트도 같은 값을 쓴다.
constexpr float WEAPON_HITSCAN_DISTANCES[] = { 0.0f, 50000.0f, 10000.0f, 0.0f }; // 사거리(cm)
constexpr int WEAPON_MAGAZINE_CAPACITIES[] = { 0, 3, 7, 0 }; // 탄창 하나의 탄 수
constexpr int WEAPON_RELOAD_DURATIONS_MS[] = { 0, 2000, 1000, 0 }; // 재장전 시간(ms)
constexpr int WEAPON_FIRE_INTERVALS_MS[] = { 0, 1000, 200, 0 }; // 발사 간격(ms)
// 곡괭이 사용 가능 횟수. 게임마다 최대치로 시작하고, 최대치보다 적으면 PICKAXE_CHARGE_MS마다 1번씩 찬다.
constexpr int PICKAXE_MAX_CHARGES = 3;
constexpr int PICKAXE_CHARGE_MS = 60000;

// 기절 게이지. 명중당하면 무기별 증가량에 맞은 부위 배율을 곱해 올리고, 마지막으로 오른 뒤 감소 대기 시간이 지나면 초당 감소량만큼 줄어든다. 최대치에 닿으면 기절한다.
constexpr float STUN_GAUGE_MAX = 100.0f;
constexpr float STUN_GAUGE_DECAY_PER_SECOND = 4.0f;
constexpr int STUN_GAUGE_DECAY_DELAY_MS = 5000;
constexpr float STUN_GAUGE_WEAPON_GAINS[] = { 0.0f, 50.0f, 20.0f }; // 무기 값 순서: 곡괭이, 저격총, 권총
constexpr float STUN_GAUGE_PART_MULTIPLIERS[] = { 1.1f, 1.0f, 0.8f, 0.8f, 0.8f, 0.8f }; // 부위 값 순서: 머리, 몸통, 팔, 손, 다리, 발
constexpr int STUN_DURATION_MS = 5000; // 기절 시간. 기절 동안 서버는 그 플레이어의 이동 입력(이동 키·조준·점프)·동작·명중을 받지 않고, 끝나면 게이지는 0이다.
// 기절이 시작된 뒤 이 시간(ms) 동안은 이동 입력이 있던 상태로 본다. 기절을 알리는 패킷이 클라이언트에 닿기 전까지 걷거나 뛴 몫을 받아 준다.
constexpr int STUN_INPUT_GRACE_MS = 500;
constexpr int STUN_PROTECTION_MS = 30000; // 기절이 끝난 뒤 보호 시간. 보호 동안 명중당해도 기절 게이지가 오르지 않는다.

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
