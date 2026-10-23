#include "TestManager.h"
#include <WS2tcpip.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <numbers>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include "database_packets.h"
#include "ex_overlapped.h"
#include "packet_types.h"
#include "room_packets.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

// 패킷 구조체를 헤더와 함께 만든다.
template<typename T>
static T MakePacket(std::uint8_t type)
{
	T packet{};
	packet.header = { static_cast<std::uint16_t>(sizeof(T)), type };
	return packet;
}

TestManager::TestManager(const std::string& server_ip, std::string output_directory)
	: latency_recorder_(std::move(output_directory))
{
	for (auto& session : sessions_) session = std::make_unique<Session>();
	for (auto& room_key : room_keys_) room_key.store(0);
	for (auto& room_join_ready_time : room_join_ready_times_) room_join_ready_time.store(-1);

	WSAStartup(MAKEWORD(2, 2), &wsa_data_);
	server_address_.sin_family = AF_INET;
	server_address_.sin_port = htons(SERVER_PORT);
	if (inet_pton(AF_INET, server_ip.c_str(), &server_address_.sin_addr) != 1) throw std::invalid_argument("invalid server IPv4 address: " + server_ip);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

TestManager::~TestManager()
{
	for (auto& session : sessions_) {
		if (session->GetSocket() != INVALID_SOCKET) closesocket(session->GetSocket());
	}
	if (iocp_handle_) CloseHandle(iocp_handle_);
	WSACleanup();
}

void TestManager::Start(int client_count)
{
	target_client_count_.store((std::min)(client_count, MAX_CLIENT_COUNT));
	connect_signal_count_.store((std::min)(CONNECT_INFLIGHT_COUNT, target_client_count_.load()));
	connect_cv_.notify_all();
}

long long TestManager::GetCurrentTimeMs() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool TestManager::LoadConnectEx(SOCKET socket)
{
	std::lock_guard<std::mutex> lock(connect_ex_mutex_);
	if (connect_ex_) return true;
	GUID guid = WSAID_CONNECTEX;
	DWORD bytes = 0;
	return WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &connect_ex_, sizeof(connect_ex_), &bytes, nullptr, nullptr) == 0;
}

int TestManager::FindAvailableSessionIndex()
{
	const int target_count = target_client_count_.load();
	for (int i = 0; i < target_count; ++i) {
		if (sessions_[i]->TrySetState(SessionState::NONE, SessionState::CONNECTING)) return i;
	}
	return -1;
}

bool TestManager::ConnectToServer()
{
	const int session_index = FindAvailableSessionIndex();
	if (session_index < 0) return false;
	Session& session = *sessions_[session_index];

	SOCKET socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN local_address{};
	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (socket == INVALID_SOCKET || !LoadConnectEx(socket) || bind(socket, reinterpret_cast<SOCKADDR*>(&local_address), sizeof(local_address)) == SOCKET_ERROR) {
		if (socket != INVALID_SOCKET) closesocket(socket);
		session.SetState(SessionState::NONE);
		return false;
	}
	BOOL no_delay = TRUE;
	setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&no_delay), sizeof(no_delay));

	session.InitSession(session_index, socket);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), iocp_handle_, session_index, 0);

	ExOverlapped* connect_over = new ExOverlapped;
	connect_over->operation_type = OperationType::CONNECT;
	if (!connect_ex_(socket, reinterpret_cast<SOCKADDR*>(&server_address_), sizeof(server_address_), nullptr, 0, nullptr, &connect_over->over) && WSAGetLastError() != WSA_IO_PENDING) {
		delete connect_over;
		closesocket(socket);
		session.InitSession(-1, INVALID_SOCKET);
		session.SetState(SessionState::NONE);
		return false;
	}
	return true;
}

void TestManager::ProcessConnect()
{
	while (true) {
		{
			std::unique_lock<std::mutex> lock(connect_mutex_);
			connect_cv_.wait(lock, [this] { return connect_signal_count_.load() > 0 || !is_reconnect_enabled_.load(); });
		}
		if (!is_reconnect_enabled_.load()) return;
		if (started_connect_count_.load() >= target_client_count_.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		if (connect_signal_count_.fetch_sub(1) <= 0) {
			connect_signal_count_.fetch_add(1);
			continue;
		}
		if (ConnectToServer()) {
			started_connect_count_.fetch_add(1);
		}
		else {
			connect_signal_count_.fetch_add(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

void TestManager::CompleteConnect(int session_index, ExOverlapped* connect_over)
{
	delete connect_over;
	Session& session = *sessions_[session_index];
	setsockopt(session.GetSocket(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
	session.SetState(SessionState::LOGIN);
	session.RecvPacket(iocp_handle_);
	const int connected_count = connected_client_count_.fetch_add(1) + 1;
	if (connected_count % 1000 == 0 || connected_count == target_client_count_.load()) {
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cout << "connected=" << connected_count << std::endl;
	}
	SendTestLogin(session_index);
}

void TestManager::ProcessIO()
{
	DWORD transferred_bytes = 0;
	ULONG_PTR key = 0;
	WSAOVERLAPPED* over = nullptr;
	const BOOL result = GetQueuedCompletionStatus(iocp_handle_, &transferred_bytes, &key, &over, INFINITE);
	if (!over) return;

	const int session_index = static_cast<int>(key);
	Session& session = *sessions_[session_index];
	// 받기는 세션 안의 겹친 입출력이고, 보내기와 연결은 새로 만든 ExOverlapped다.
	const bool is_recv = over == &session.GetRecvOverlapped();
	ExOverlapped* ex_over = is_recv ? nullptr : reinterpret_cast<ExOverlapped*>(over);
	if (!result || (is_recv && transferred_bytes == 0)) {
		delete ex_over;
		Disconnect(session_index);
		return;
	}
	if (is_recv) {
		ProcessRecvBuffer(static_cast<int>(transferred_bytes), session_index);
		session.RecvPacket(iocp_handle_);
	}
	else if (ex_over->operation_type == OperationType::CONNECT) CompleteConnect(session_index, ex_over);
	else delete ex_over;
}

void TestManager::ProcessRecvBuffer(int recv_bytes, int session_index)
{
	Session& session = *sessions_[session_index];
	char* buffer = session.GetRecvBuffer();
	const int data_size = session.GetRemainingDataSize() + recv_bytes;
	int offset = 0;
	while (data_size - offset >= static_cast<int>(sizeof(PACKET_HEADER))) {
		const PACKET_HEADER* header = reinterpret_cast<const PACKET_HEADER*>(buffer + offset);
		if (header->size < sizeof(PACKET_HEADER)) {
			Disconnect(session_index);
			return;
		}
		if (data_size - offset < header->size) break;
		HandlePacket(buffer + offset, session_index);
		offset += header->size;
		const SessionState state = session.GetState();
		if (state == SessionState::NONE || state == SessionState::DISCONNECTING) return;
	}
	memmove(buffer, buffer + offset, data_size - offset);
	session.SetRemainingDataSize(data_size - offset);
}

void TestManager::SendTestLogin(int session_index)
{
	C2S_STRESS_LOGIN_PACKET packet = MakePacket<C2S_STRESS_LOGIN_PACKET>(C2S_STRESS_LOGIN);
	packet.player_id = session_index + 1;
	sessions_[session_index]->SendPacket(&packet, sizeof(packet));
}

void TestManager::SendCreateRoom(int host_index)
{
	Session& session = *sessions_[host_index];
	if (!session.TrySetState(SessionState::LOBBY, SessionState::ENTER_ROOM)) return;
	C2S_ADD_PUBLIC_ROOM_PACKET packet = MakePacket<C2S_ADD_PUBLIC_ROOM_PACKET>(C2S_ADD_PUBLIC_ROOM);
	std::snprintf(packet.room_name, sizeof(packet.room_name), "stress_%d", host_index / ROOM_CLIENT_COUNT);
	session.SendPacket(&packet, sizeof(packet));
}

// 방 만들기 응답에 방 키가 없고 방 목록은 로비에서만 받으므로, 방마다 두 번째 봇이 로비에서 방 목록을 받아 방 이름으로 키를 찾는다.
// 받은 방 목록의 키는 어느 방 것이든 모두 기록한다.
void TestManager::SendRoomListRequest(int session_index, long long now_ms)
{
	Session& session = *sessions_[session_index];
	session.SetLastRoomListMs(now_ms);
	C2S_REQUEST_ROOM_LIST_PACKET packet = MakePacket<C2S_REQUEST_ROOM_LIST_PACKET>(C2S_REQUEST_ROOM_LIST);
	session.SendPacket(&packet, sizeof(packet));
}

void TestManager::TryJoinRoom(int session_index)
{
	if (session_index % ROOM_CLIENT_COUNT == 0) return;
	Session& session = *sessions_[session_index];
	const int room_index = session_index / ROOM_CLIENT_COUNT;
	const RoomKey room_key = room_keys_[room_index].load();
	if (room_key == 0 || GetCurrentTimeMs() < room_join_ready_times_[room_index].load()) return;
	if (!session.TrySetState(SessionState::LOBBY, SessionState::ENTER_ROOM)) return;
	C2S_JOIN_PUBLIC_ROOM_PACKET packet = MakePacket<C2S_JOIN_PUBLIC_ROOM_PACKET>(C2S_JOIN_PUBLIC_ROOM);
	packet.room_key = room_key;
	session.SendPacket(&packet, sizeof(packet));
}

// 앞뒤 키 입력이 바뀌면 키 입력을 먼저 보내고 그 순간의 이동 상태를 바로 보낸다. 바뀌지 않으면 MOVE_SEND_INTERVAL_MS마다 이동 상태를 보낸다.
void TestManager::SendWalk(Session& session, long long now_ms)
{
	const WalkStep step = session.GetWalkStep(now_ms);
	const bool is_input_changed = !session.HasSentInput() || step.input_forward != session.GetSentInputForward();
	if (is_input_changed) {
		C2S_MOVE_INPUT_PACKET input_packet = MakePacket<C2S_MOVE_INPUT_PACKET>(C2S_MOVE_INPUT);
		input_packet.input_right = 0;
		input_packet.input_forward = step.input_forward;
		session.SendPacket(&input_packet, sizeof(input_packet));
		session.SetSentInputForward(step.input_forward);
	}
	if (!is_input_changed && now_ms - session.GetLastMoveSendMs() < MOVE_SEND_INTERVAL_MS) return;
	C2S_MOVE_PACKET move_packet = MakePacket<C2S_MOVE_PACKET>(C2S_MOVE);
	move_packet.move_state = step.move_state;
	session.AddSentMove(step.move_state.location, GetCurrentTimeMs());
	session.SendPacket(&move_packet, sizeof(move_packet));
	session.SetLastMoveSendMs(now_ms);
}

// 처음에는 FIRE_WEAPON으로 바꾸고, 바꾸는 시간이 지나면 FIRE_INTERVAL_MS마다 무작위 방향으로 쏜다.
void TestManager::SendFire(Session& session, long long now_ms)
{
	if (!session.HasTimeSync()) return;
	if (session.GetNextFireMs() < 0) {
		C2S_EQUIP_PACKET equip_packet = MakePacket<C2S_EQUIP_PACKET>(C2S_EQUIP);
		equip_packet.weapon = static_cast<std::uint8_t>(FIRE_WEAPON);
		equip_packet.action_time_ms = session.GetActionTimeMs(now_ms);
		session.SendPacket(&equip_packet, sizeof(equip_packet));
		session.SetNextFireMs(now_ms + EQUIP_DURATIONS_MS[static_cast<int>(FIRE_WEAPON)]);
		return;
	}
	if (now_ms < session.GetNextFireMs()) return;
	thread_local std::mt19937 random_engine(std::random_device{}());
	std::uniform_real_distribution<float> z_distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> angle_distribution(0.0f, 2.0f * std::numbers::pi_v<float>);
	const float z = z_distribution(random_engine);
	const float angle = angle_distribution(random_engine);
	const float horizontal = std::sqrt(1.0f - z * z);
	const WalkStep step = session.GetWalkStep(now_ms);
	C2S_FIRE_PACKET fire_packet = MakePacket<C2S_FIRE_PACKET>(C2S_FIRE);
	fire_packet.aim_direction[0] = horizontal * std::cos(angle);
	fire_packet.aim_direction[1] = horizontal * std::sin(angle);
	fire_packet.aim_direction[2] = z;
	fire_packet.camera[0] = step.move_state.location[0];
	fire_packet.camera[1] = step.move_state.location[1];
	fire_packet.camera[2] = step.move_state.location[2] + AIM_CAMERA_UP;
	fire_packet.action_time_ms = session.GetActionTimeMs(now_ms);
	session.SetFireSendMs(GetCurrentTimeMs());
	session.SendPacket(&fire_packet, sizeof(fire_packet));
	session.SetNextFireMs(session.GetNextFireMs() + FIRE_INTERVAL_MS);
}

void TestManager::NotifyLoginComplete()
{
	if (is_reconnect_enabled_.load() && started_connect_count_.load() < target_client_count_.load()) connect_signal_count_.fetch_add(1);
	connect_cv_.notify_one();
}

void TestManager::HandlePacket(char* packet, int session_index)
{
	Session& session = *sessions_[session_index];
	const bool is_host = session_index % ROOM_CLIENT_COUNT == 0;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case S2C_LOGIN: {
		NotifyLoginComplete();
		const auto* login_packet = reinterpret_cast<S2C_LOGIN_PACKET*>(packet);
		if (!session.TrySetState(SessionState::LOGIN, SessionState::LOBBY)) break;
		session.SetPlayerID(login_packet->player_id);
		if (is_host) SendCreateRoom(session_index);
		else TryJoinRoom(session_index);
		break;
	}
	case S2C_ADD_PUBLIC_ROOM: {
		session.SetState(SessionState::ROOM);
		if (is_host) break;
		C2S_READY_PACKET ready_packet = MakePacket<C2S_READY_PACKET>(C2S_READY);
		ready_packet.is_ready = true;
		session.SendPacket(&ready_packet, sizeof(ready_packet));
		break;
	}
	case S2C_ROOM_INFO: {
		const auto* info_packet = reinterpret_cast<S2C_ROOM_INFO_PACKET*>(packet);
		char name[MAX_ROOM_NAME_SIZE + 1]{};
		memcpy(name, info_packet->room_name, MAX_ROOM_NAME_SIZE);
		int room_index = -1;
		if (sscanf_s(name, "stress_%d", &room_index) != 1 || room_index < 0 || room_index >= MAX_STRESS_ROOM_COUNT) break;
		RoomKey expected = 0;
		if (room_keys_[room_index].compare_exchange_strong(expected, info_packet->room_key)) room_join_ready_times_[room_index].store(GetCurrentTimeMs() + ROOM_JOIN_DELAY_MS);
		break;
	}
	case S2C_READY: {
		const auto* ready_packet = reinterpret_cast<S2C_READY_PACKET*>(packet);
		if (!is_host || !ready_packet->is_ready || ready_packet->player_id == session.GetPlayerID()) break;
		if (session.IncrementReadyPlayerCount() != ROOM_CLIENT_COUNT - 1) break;
		// 모든 방이 준비되면 방장들이 한꺼번에 시작한다.
		const int target_room_count = target_client_count_.load() / ROOM_CLIENT_COUNT;
		if (ready_room_count_.fetch_add(1) + 1 != target_room_count) break;
		{
			std::lock_guard<std::mutex> lock(log_mutex_);
			std::cout << "all_rooms_ready=" << target_room_count << std::endl;
		}
		const C2S_START_PACKET start_packet = MakePacket<C2S_START_PACKET>(C2S_START);
		for (int host_index = 0; host_index < target_client_count_.load(); host_index += ROOM_CLIENT_COUNT) sessions_[host_index]->SendPacket(&start_packet, sizeof(start_packet));
		break;
	}
	case S2C_MULTI_START: {
		if (!session.TrySetState(SessionState::ROOM, SessionState::LOADING)) break;
		const C2S_LOAD_COMPLETE_PACKET load_packet = MakePacket<C2S_LOAD_COMPLETE_PACKET>(C2S_LOAD_COMPLETE);
		session.SendPacket(&load_packet, sizeof(load_packet));
		break;
	}
	case S2C_SPAWN: {
		const auto* spawn_packet = reinterpret_cast<S2C_SPAWN_PACKET*>(packet);
		if (spawn_packet->player_id == session.GetPlayerID()) session.SetWalkPath(spawn_packet->location[0], spawn_packet->location[1]);
		break;
	}
	case S2C_PLAY_START: {
		if (!session.TrySetState(SessionState::LOADING, SessionState::PLAYING)) break;
		// 방 안 봇들이 이동을 같은 순간에 보내지 않도록, 방 슬롯 순서대로 걷기 시작을 보내기 간격 안에 고르게 흩는다.
		session.StartWalk(GetCurrentTimeMs() + session_index % ROOM_CLIENT_COUNT * (MOVE_SEND_INTERVAL_MS / ROOM_CLIENT_COUNT));
		const int playing_count = playing_client_count_.fetch_add(1) + 1;
		if (playing_count % 1000 == 0 || playing_count == target_client_count_.load()) {
			std::lock_guard<std::mutex> lock(log_mutex_);
			std::cout << "playing=" << playing_count << std::endl;
		}
		break;
	}
	case S2C_GAME_START: {
		const int started_count = game_started_client_count_.fetch_add(1) + 1;
		if (started_count == target_client_count_.load()) UpdateMeasurement();
		break;
	}
	case S2C_MOVE: {
		const auto* move_packet = reinterpret_cast<S2C_MOVE_PACKET*>(packet);
		if (move_packet->player_id != session.GetPlayerID()) break;
		const std::optional<long long> send_time_ms = session.TakeSentMoveTime(move_packet->location);
		if (send_time_ms) latency_recorder_.Record(LatencyKind::MOVE, GetCurrentTimeMs() - *send_time_ms);
		break;
	}
	case S2C_TIME_SYNC: {
		session.SetTimeSync(reinterpret_cast<S2C_TIME_SYNC_PACKET*>(packet)->server_time_ms, GetCurrentTimeMs());
		break;
	}
	case S2C_AMMO_USE: {
		const long long send_time_ms = session.TakeFireSendMs();
		if (send_time_ms >= 0) latency_recorder_.Record(LatencyKind::FIRE, GetCurrentTimeMs() - send_time_ms);
		break;
	}
	case S2C_MOVE_CORRECTION: {
		C2S_MOVE_CORRECTION_ACK_PACKET ack_packet = MakePacket<C2S_MOVE_CORRECTION_ACK_PACKET>(C2S_MOVE_CORRECTION_ACK);
		ack_packet.correction_id = reinterpret_cast<S2C_MOVE_CORRECTION_PACKET*>(packet)->correction_id;
		session.SendPacket(&ack_packet, sizeof(ack_packet));
		break;
	}
	case S2C_ERROR: {
		const int error_code = reinterpret_cast<S2C_ERROR_PACKET*>(packet)->error_code;
		if (session.GetState() == SessionState::ENTER_ROOM) session.SetState(SessionState::LOBBY);
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cerr << "server_error session=" << session_index << " code=" << error_code << std::endl;
		break;
	}
	default:
		break;
	}
}

void TestManager::ProcessSend(int worker_index)
{
	const long long now_ms = GetCurrentTimeMs();
	const int target_count = target_client_count_.load();
	for (int session_index = worker_index; session_index < target_count; session_index += SEND_THREAD_COUNT) {
		Session& session = *sessions_[session_index];
		const SessionState state = session.GetState();
		const bool is_room_key_known = room_keys_[session_index / ROOM_CLIENT_COUNT].load() != 0;
		if (state == SessionState::LOBBY && !is_room_key_known && session_index % ROOM_CLIENT_COUNT == 1 && now_ms - session.GetLastRoomListMs() >= ROOM_LIST_RETRY_MS) SendRoomListRequest(session_index, now_ms);
		else if (state == SessionState::LOBBY) TryJoinRoom(session_index);
		else if (state == SessionState::PLAYING && session.IsWalking(now_ms)) {
			SendWalk(session, now_ms);
			SendFire(session, now_ms);
		}
	}
	if (worker_index == 0) UpdateMeasurement();
}

void TestManager::UpdateMeasurement()
{
	if (game_started_client_count_.load() == target_client_count_.load() && latency_recorder_.TryStart()) {
		is_reconnect_enabled_.store(false);
		connect_cv_.notify_all();
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cout << "measurement_started duration_seconds=" << MEASUREMENT_SECONDS << std::endl;
	}
	if (latency_recorder_.TryFinish()) {
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cout << "measurement_finished move_sample_count=" << latency_recorder_.GetSampleCount(LatencyKind::MOVE) << " fire_sample_count=" << latency_recorder_.GetSampleCount(LatencyKind::FIRE) << std::endl;
	}
}

void TestManager::Disconnect(int session_index)
{
	Session& session = *sessions_[session_index];
	SessionState previous_state = session.GetState();
	while (previous_state != SessionState::NONE && previous_state != SessionState::DISCONNECTING) {
		if (session.TrySetState(previous_state, SessionState::DISCONNECTING)) break;
		previous_state = session.GetState();
	}
	if (previous_state == SessionState::NONE || previous_state == SessionState::DISCONNECTING) return;
	{
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cerr << "disconnected session=" << session_index << std::endl;
	}
	if (previous_state != SessionState::CONNECTING) connected_client_count_.fetch_sub(1);
	if (previous_state == SessionState::PLAYING) playing_client_count_.fetch_sub(1);
	closesocket(session.GetSocket());
	// 측정은 처음 접속한 봇들로만 하므로, 끊긴 봇은 다시 접속하지 않는다.
	session.InitSession(-1, INVALID_SOCKET);
}
