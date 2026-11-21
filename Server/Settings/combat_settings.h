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
// 명중 카메라 검증. 받은 카메라 위치가 서버가 계산한 자리에서 이동 검증 허용치(MOVE_TOLERANCE)와 "속도 × CAMERA_FRAME_ALLOWANCE_SECONDS"까지 벗어나도 된다.
// 카메라는 이전 프레임에 자리를 정하므로 그만큼 움직인 몫을 봐준다. 3인칭 카메라에서 맞은 곳은 카메라 방향에서 CAMERA_VIEW_CONE_DEGREES 안이어야 한다.
constexpr float CAMERA_FRAME_ALLOWANCE_SECONDS = 0.05f;
constexpr float CAMERA_VIEW_CONE_DEGREES = 45.0f;

// 탄 퍼짐(shot_spread.h). 서버가 퍼짐 크기를 정한다. 서버가 받은 발사 시각으로 센 연사 열기를 SHOT_SPREAD_TIME_ALLOWANCE_SECONDS만큼 더 회복한 값으로 정한다.
constexpr float SHOT_SPREAD_TIME_ALLOWANCE_SECONDS = 0.2f;
constexpr float HIT_OCCLUSION_MARGIN = 5.0f; // 지형 막힘 검사 선분의 양 끝에서 빼는 길이(cm). 맞은 사람이 붙어 선 벽이나 발사 지점 바로 옆 면을 막힘으로 보지 않는다.

// 탄약 상자. 무기마다 AMMO_BOX_COUNT × 글로벌 배수만큼 만들고, 한 상자에는 한 무기의 탄만 든다. 열면 그 무기의 소지 탄이 늘어난다.
constexpr int AMMO_BOX_COUNT = 50;
constexpr int AMMO_BOX_ROUNDS[] = { 0, 1, 3 }; // 상자 하나로 느는 탄 수. 무기 값 순서: 곡괭이, 저격총, 권총. 0이면 그 무기의 탄약 상자를 만들지 않는다.
