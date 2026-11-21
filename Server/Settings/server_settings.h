#pragma once
#include <cstdint>
// 서버 운영. 틱, 네트워크, 접속·방 수, 스레드 수, 정리 주기, DB.

constexpr int TICK_RATE = 50;
constexpr int TICK_INTERVAL_MS = 1000 / TICK_RATE;
// 방이 한 번 깨어날 때 밀린 틱을 몰아서 처리하는 최대 수. 남은 틱은 다음에 깨어날 때 이어서 처리한다.
constexpr int MAX_CATCH_UP_TICKS = 5;
// ms 설정값을 틱 수로 바꾼다. 게임 안 시간은 모두 틱으로 센다. 틱 간격으로 나누어떨어지지 않으면 올린다.
constexpr std::uint32_t MsToTicks(long long ms) { return static_cast<std::uint32_t>((ms + TICK_INTERVAL_MS - 1) / TICK_INTERVAL_MS); }

constexpr int BUF_SIZE = 2048;
constexpr int SERVER_PORT = 12312;
// 동시에 걸어 두는 접속 수락 요청 수. 하나가 처리되는 동안에도 나머지가 접속을 받는다.
constexpr int ACCEPT_COUNT = 10;
// 세션 하나가 번갈아 쓰는 송신 버퍼 수
constexpr int SEND_BUFFER_POOL_SIZE = 5;

constexpr int MAX_PLAYER_COUNT = 10000;
constexpr int MAX_ROOM_COUNT = 500;
// 스레드 수. 입출력 스레드는 하드웨어 스레드 수에서 이 스레드 수들을 뺀 만큼(최소 1개) 만든다.
constexpr int GAME_THREAD_COUNT = 12;
constexpr int LOBBY_THREAD_COUNT = 2;
constexpr int MATCHING_THREAD_COUNT = 1;
constexpr int DB_THREAD_COUNT = 1;
constexpr int TIMER_THREAD_COUNT = 1;
// 틱을 시작하려면 일을 받을 수 있는 게임 스레드가 이만큼 있어야 한다.
constexpr int MIN_AVAILABLE_GAME_THREAD_COUNT = GAME_THREAD_COUNT / 2;

constexpr int DISCONNECT_CLEANUP_INTERVAL_MS = 1000; // 타이머가 연결 종료 정리를 입출력 스레드에 맡기는 간격
constexpr int ACCEPT_RETRY_INTERVAL_MS = 1000; // 자원 부족 등으로 다시 걸지 못한 접속 수락 요청을 다시 거는 간격
constexpr int DB_RECONNECT_INTERVAL_MS = 1000; // DB 연결이 끊겼을 때 다시 연결을 시도하는 간격. 성공할 때까지 계속 시도한다.
constexpr int DB_REQUEST_TIMEOUT_MS = 10000; // 로그인 요청이 DB 처리를 기다릴 수 있는 최대 시간. 넘으면 시간 초과로 응답하고 버린다.
constexpr int DB_CONNECT_TIMEOUT_SECONDS = 2; // DB 연결 시도 한 번이 기다리는 최대 시간. DB 서버가 응답 없이 사라졌을 때 연결 시도에 오래 묶이지 않게 한다.
constexpr int DB_IO_TIMEOUT_SECONDS = 5; // 연결된 뒤 쿼리를 주고받을 때 기다리는 최대 시간. 조회 중 DB 서버가 사라져도 끝없이 기다리지 않는다.
