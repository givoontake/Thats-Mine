#include "LoginManager.h"
#include "GameServer.h"
#include "SessionManager.h"
#include "char_buffer.h"
#include "db_login_result.h"
#include "database_packets.h"
#include "packet_types.h"

LoginManager::LoginManager(GameServer& game_server)
	: game_server_(game_server)
{
}

bool LoginManager::IsLoginSession(const SessionState& state)
{
	return state.life == LifeState::ACTIVE && state.mode == ModeState::LOGIN;
}

// 로그인 요청을 조회할 세션인지 확인한다. 로그인 DB 스레드가 요청을 순서대로 처리하므로, 앞선 요청이 성공했으면 뒤의 요청은 여기서 걸러진다.
bool LoginManager::IsWaitingLogin(SessionKey session_key)
{
	auto* session = game_server_.GetSessionManager().FindSession(session_key);
	return session && IsLoginSession(session->GetState());
}

// 조회에 성공한 결과를 반영한다. 중복 로그인이 아니면 로비 상태로 확정하고 응답한다.
void LoginManager::ApplyLoginResult(SessionKey session_key, const DBLoginResult& result)
{
	auto& session_manager = game_server_.GetSessionManager();
	auto* session = session_manager.FindSession(session_key);
	if (!session) return;

	// AddPlayer는 중복 로그인을 확인하고, 통과하면 세션에 플레이어 정보를 기록하며 LOGIN → LOBBY로 바꾼다.
	// 조회하는 사이 연결 종료가 예약됐으면 바꾸지 않으므로 실패가 되며, 그때 보내는 오류는 송신되지 않는다.
	if (!session_manager.GetActivePlayerManager().AddPlayer(*session, result)) {
		game_server_.SendError(*session, ErrorCode::DUPLICATE_LOGIN_ID);
		return;
	}

	S2C_LOGIN_PACKET packet{};
	packet.header = { sizeof(packet), S2C_LOGIN };
	packet.player_id = result.player_id;
	StringToCharBuf(result.nickname, packet.nickname, sizeof(packet.nickname));
	session->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
