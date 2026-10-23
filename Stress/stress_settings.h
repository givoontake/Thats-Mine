#pragma once
#include <cstdint>
#include "common_packets.h"
#include "game_packets.h"
#include "settings.h"

// 부하 시험 설정. 방 하나에 MAX_ROOM_PLAYER_COUNT명씩 넣고, 서버 한도(MAX_PLAYER_COUNT)까지 잰다.
constexpr int MAX_CLIENT_COUNT = MAX_PLAYER_COUNT;
constexpr int ROOM_CLIENT_COUNT = MAX_ROOM_PLAYER_COUNT;
constexpr int MAX_STRESS_ROOM_COUNT = MAX_CLIENT_COUNT / ROOM_CLIENT_COUNT;
constexpr int IO_THREAD_COUNT = 4;
constexpr int CONNECT_THREAD_COUNT = 2;
constexpr int SEND_THREAD_COUNT = 8;
constexpr int CONNECT_INFLIGHT_COUNT = 256;
constexpr int MEASUREMENT_SECONDS = 30;
// 내 이동 중계를 기다리는 보낸 이동 수. 넘치면 가장 오래된 것부터 버린다.
constexpr int SENT_MOVE_QUEUE_SIZE = 64;
// 받는 쪽 버퍼. 상자 배치처럼 BUF_SIZE보다 큰 패킷도 한 번에 담는다.
constexpr int RECV_BUFFER_SIZE = 65536;
// 방 키를 찾는 봇이 방 목록을 다시 요청하는 간격
constexpr int ROOM_LIST_RETRY_MS = 500;
// 방장이 방 키를 알린 뒤 다른 봇이 들어오기 시작하는 시각
constexpr int ROOM_JOIN_DELAY_MS = 100;
// 봇은 스폰 자리에서 이웃 스폰 자리까지 앞으로 걷고 뒤로 걸어 돌아오기를 반복한다.
constexpr float WALK_DISTANCE = 500.0f;
// 스폰 높이는 땅에 선 캡슐 중심보다 1m 높다.
constexpr float SPAWN_HEIGHT_OFFSET = 100.0f;
// 봇은 걷기 시작할 때 이 무기로 바꾸고, 바꾸는 시간이 지나면 FIRE_INTERVAL_MS마다 무작위 방향으로 쏜다.
constexpr WeaponType FIRE_WEAPON = WeaponType::PISTOL;
constexpr int FIRE_INTERVAL_MS = 1000;

// 시험 로그인. DB를 거치지 않고 player_id로 로그인한다. 부하 시험할 때만 서버에 넣는 C2S_TEST_LOGIN과 번호·모양이 같고, 서버 헤더와 겹치지 않게 이름만 다르게 둔다.
constexpr std::uint8_t C2S_STRESS_LOGIN = 127;

#pragma pack(push, 1)
struct C2S_STRESS_LOGIN_PACKET {
	PACKET_HEADER header;
	int player_id;
};
#pragma pack(pop)
