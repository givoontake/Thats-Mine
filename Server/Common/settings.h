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

// 명중 검증. 지연 보정이 없어 클라이언트 화면과 서버의 위치 차이를 여유로 허용한다.
constexpr float HIT_BASE_MARGIN = 20.0f; // 서버 히트박스를 키우는 기본 여유(cm)
constexpr float HIT_LATENCY_ALLOWANCE_SECONDS = 0.15f; // 속도에 곱해 여유에 더하는 지연 허용 시간(초)
constexpr float HIT_ORIGIN_MAX_OFFSET = 200.0f; // 발사 시작점이 쏜 사람 위치에서 벗어날 수 있는 기본 거리(cm)
