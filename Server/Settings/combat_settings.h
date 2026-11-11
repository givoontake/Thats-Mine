#pragma once
// 사격과 무기. 명중 판정, 카메라 검증, 탄 퍼짐, 발사 간격, 장비 교체, 무기 수치, 탄약 상자.

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

// 탄약 상자. 무기마다 AMMO_BOX_COUNT × 글로벌 배수만큼 만들고, 한 상자에는 한 무기의 탄만 든다. 열면 그 무기의 소지 탄이 늘어난다.
constexpr int AMMO_BOX_COUNT = 50;
constexpr int AMMO_BOX_ROUNDS[] = { 0, 1, 3 }; // 상자 하나로 느는 탄 수. 무기 값 순서: 곡괭이, 저격총, 권총. 0이면 그 무기의 탄약 상자를 만들지 않는다.
