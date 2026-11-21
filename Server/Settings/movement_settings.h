#pragma once
// 이동 계산과 이동 동기화. 클라이언트 캐릭터 이동과 서버 이동 계산이 같은 값을 쓴다.

// 이동 동기화 간격. 클라이언트도 같은 값을 사용한다.
constexpr int MOVE_STEP_MS = 20; // 전송 프레임 길이. 클라이언트는 전송 프레임마다 내 캐릭터 이동을 계산하고 이동 패킷을 보낸다. 서버는 틱마다 전송 프레임 하나를 처리하므로 틱 간격과 같아야 한다.
// 스폰·롤백 뒤 첫 이동 패킷이 도착한 틱에서 그 번호를 처리할 틱까지 더 기다리는 틱 수
constexpr int FRAME_GAP_ALLOWANCE_TICKS = 1;

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
// 한 단계는 캡슐을 직선으로 한 번 쓸고 바닥을 한 번 찾으므로, 긴 시간을 한 단계로 가면 점프 궤적과 가장자리·경사 변화를 놓친다.
// 값은 언리얼 캐릭터 이동 기본값이다. 서버는 틱 간격이 이 시간보다 짧아 평소에는 나누지 않고, 틱이 밀렸거나
// 받은 상태 시각이 서버보다 앞서 한 번에 이 시간보다 길게 진행할 때 서버 위치가 거칠게 계산되지 않도록 나눈다.
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
