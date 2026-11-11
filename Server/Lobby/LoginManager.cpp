#include <algorithm>
#include "LoginManager.h"
#include "GameServer.h"
#include "ServerThreadManager.h"
#include "SessionManager.h"
#include "db_login_result.h"

LoginManager::LoginManager(GameServer& game_server)
	: game_server_(game_server)
{
}

// 접속을 받은 세션까지 중복 로그인 확인 때 훑을 끝을 늘린다.
void LoginManager::AddSession(SessionKey session_key)
{
	session_end_ = std::max(session_end_, session_key.session_index + 1);
}

// 로그인 대기 중 연결이 끊긴 세션의 정리 통보다. 정리 가능으로 바꾼다. 이 세션에 마지막으로 하는 일이다.
// 로그인에 성공했지만 로비로 옮기지 못한 세션도 소속이 그대로 LOGIN이므로 여기서 정리한다.
void LoginManager::CleanupSession(SessionKey session_key)
{
	auto* session = game_server_.GetSessionManager().FindSession(session_key);
	if (!session) return;
	session->SetDisconnectReady(ModeState::LOGIN);
}

// 로그인 요청을 조회할 세션인지 확인한다. 소속이 LOGIN인 세션은 이 스레드만 정리하므로, 세션 키 비교 결과가 처리하는 동안 바뀌지 않는다.
// 로그인 DB 스레드가 요청을 순서대로 처리하므로, 앞선 요청이 성공했으면 소속이 바뀌어 뒤의 요청은 여기서 걸러진다.
bool LoginManager::IsWaitingLogin(SessionKey session_key)
{
	auto* session = game_server_.GetSessionManager().FindSession(session_key);
	if (!session) return false;
	const SessionState state = session->GetState();
	return state.life == LifeState::ACTIVE && state.mode == ModeState::LOGIN;
}

// 같은 계정으로 로그인한 세션이 있는지 세션 배열을 끝까지 훑어 본다. 해제되기 전까지는 종료가 예약됐거나 정리 가능인 세션도 포함한다.
// 끝은 로그인 DB 스레드만 바꾼다. 새 세션 작업 때 늘리고, 훑기 직전에 끝 쪽의 빈 칸만큼 줄인다.
// 새 세션 작업은 그 세션의 로그인보다 먼저 처리되므로 플레이어 번호가 적힌 세션은 언제나 끝 안에 있다.
// 줄일 때 끝 밖으로 나간 빈 칸을 새 접속이 차지해도, 그 세션의 새 세션 작업이 로그인보다 먼저 와서 다시 늘린다.
// 플레이어 번호를 적는 쪽도 로그인 DB 스레드 하나뿐이고 다른 스레드는 해제 때 지우기만 하므로, 남아 있는 연결을 놓치지 않는다.
bool LoginManager::HasPlayer(int player_id)
{
	auto& session_manager = game_server_.GetSessionManager();
	while (session_end_ > 0 && !session_manager.FindSessionByIndex(session_end_ - 1)) --session_end_;
	for (int session_index = 0; session_index < session_end_; ++session_index) {
		const Session* session = session_manager.FindSessionByIndex(session_index);
		if (session && session->GetPlayerId() == player_id) return true;
	}
	return false;
}

// 조회에 성공한 결과를 반영한다. 같은 계정으로 로그인한 세션이 없으면 플레이어 정보를 기록하고, 마지막에 로비로 옮기는 요청을 보낸다.
// 로비가 받아 확정하고 응답한다. 연결 종료가 예약돼 옮기지 못하면 소속이 LOGIN으로 남아 정리 통보로 정리한다.
// 중복 확인과 기록은 로그인 DB 스레드 한 곳에서 차례로 하므로, 같은 계정의 로그인 두 개가 함께 통과하지 않는다.
void LoginManager::ApplyLoginResult(SessionKey session_key, const DBLoginResult& result)
{
	if (!IsWaitingLogin(session_key)) return;
	auto& session_manager = game_server_.GetSessionManager();
	auto* session = session_manager.FindSession(session_key);
	if (HasPlayer(result.player_id)) {
		game_server_.SendError(*session, ErrorCode::DUPLICATE_LOGIN_ID);
		return;
	}
	session->SetPlayerInfo(result);
	if (!session->TryChangeMode(ModeState::LOGIN, ModeState::LOBBY_PENDING)) return;
	game_server_.GetThreadManager().GetLobbyThreadManager().EnqueueTask({ LobbyTaskType::LOGIN, session_key });
}
