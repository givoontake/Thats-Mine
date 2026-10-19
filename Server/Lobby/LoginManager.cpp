#include "LoginManager.h"
#include "GameServer.h"
#include "SessionManager.h"
#include "ServerThreadManager.h"
#include "DBThreadManager.h"
#include "char_buffer.h"
#include "db_login_result.h"
#include "db_login_task.h"
#include "database_packets.h"
#include "packet_types.h"

LoginManager::LoginManager(GameServer& game_server)
	: game_server_(game_server)
{
}

bool LoginManager::IsLoginMode(ModeState mode_state)
{
	return mode_state == ModeState::LOGIN || mode_state == ModeState::AUTHENTICATING;
}

bool LoginManager::ShouldBeListed(const SessionState& state)
{
	return state.life == LifeState::ACTIVE && IsLoginMode(state.mode);
}

// 세션의 로그인 목록 등록 여부를 현재 세션 상태에 맞춘다. 로비 목록의 맞추기와 같은 방식이다.
void LoginManager::SyncLoginList(int session_index)
{
	login_list_.Sync(session_index, [this, session_index]() {
		auto* session = game_server_.GetSessionManager().FindSessionByIndex(session_index);
		return session && ShouldBeListed(session->GetState());
	});
}

Session* LoginManager::FindLoginSession(int login_index)
{
	const std::int32_t session_index = login_list_.GetValue(login_index);
	if (session_index < 0) return nullptr;
	return game_server_.GetSessionManager().FindSessionByIndex(session_index);
}

// 새 접속 세션을 로그인 목록에 넣는다. 목록에 들어가기 전에 온 로그인 요청은 세션 작업 큐에 남아 있다가 목록에 들어간 뒤 처리된다.
void LoginManager::AddSession(SessionKey session_key)
{
	SyncLoginList(session_key.session_index);
}

// 로그인 전 세션의 패킷을 처리한다. 로그인 요청만 받는다.
void LoginManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet || reinterpret_cast<PACKET_HEADER*>(packet)->type != C2S_LOGIN) return;
	// 조회 중으로 먼저 바꿔, 결과가 오기 전의 추가 로그인 요청은 받지 않는다.
	if (!session.TryChangeMode(ModeState::LOGIN, ModeState::AUTHENTICATING)) return;

	const auto* request = reinterpret_cast<const C2S_LOGIN_PACKET*>(packet);
	DBLoginTask task{ session.GetSessionKey(), CharBufToString(request->login_id, sizeof(request->login_id)), CharBufToString(request->login_password, sizeof(request->login_password)) };
	if (game_server_.GetThreadManager().GetDBThreadManager().Enqueue(std::move(task))) return;

	// DB 스레드가 요청을 받지 못했다. 다시 로그인할 수 있게 되돌리고 실패를 알린다.
	session.TryChangeMode(ModeState::AUTHENTICATING, ModeState::LOGIN);
	game_server_.SendError(session, ErrorCode::SERVER_ERROR);
}

// 로그인 DB 스레드가 보낸 성공 결과를 반영한다. 로비 사전 작업에서 호출된다.
// 조회 실패는 DB 스레드가 직접 처리하므로 여기서 실패가 되는 경우는 중복 로그인뿐이지만, 어떤 실패든 같은 방식으로 되돌린다.
void LoginManager::ApplyLoginResult(SessionKey session_key, const DBLoginResult& result)
{
	auto& session_manager = game_server_.GetSessionManager();
	auto* session = session_manager.FindSession(session_key);
	// 조회하는 사이 연결이 끊겨 세션이 정리됐거나 정리 중이면 버린다.
	if (!session || session->GetModeState() != ModeState::AUTHENTICATING) return;

	int error_code = result.error_code;
	// AddPlayer는 중복 로그인을 확인하고, 통과하면 세션에 플레이어 정보를 기록하며 AUTHENTICATING → LOBBY로 바꾼다.
	if (error_code == ErrorCode::SUCCESS && !session_manager.GetActivePlayerManager().AddPlayer(*session, result)) error_code = ErrorCode::DUPLICATE_LOGIN_ID;
	if (error_code != ErrorCode::SUCCESS) {
		session->TryChangeMode(ModeState::AUTHENTICATING, ModeState::LOGIN);
		game_server_.SendError(*session, error_code);
		return;
	}

	// 로그인 목록에서 빼고 로비 목록에 넣은 뒤 응답한다. 응답을 받은 클라이언트의 로비 요청은 로비 목록에서 처리된다.
	const int session_index = session_key.session_index;
	SyncLoginList(session_index);
	game_server_.SyncLobbyList(session_index);

	S2C_LOGIN_PACKET packet{};
	packet.header = { sizeof(packet), S2C_LOGIN };
	packet.player_id = result.player_id;
	StringToCharBuf(result.nickname, packet.nickname, sizeof(packet.nickname));
	session->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
