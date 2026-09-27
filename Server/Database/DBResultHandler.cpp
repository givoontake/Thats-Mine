#include "DBResultHandler.h"
#include "GameServer.h"
#include "char_buffer.h"
#include "SessionManager.h"
#include "database_packets.h"
#include "packet_types.h"

void DBResultHandler::HandleLogin(GameServer& server, Session& session, const DBLoginResult& result)
{
	int error_code = result.error_code;
	if (error_code == ErrorCode::SUCCESS && !server.GetSessionManager().GetActivePlayerManager().AddPlayer(session, result)) error_code = ErrorCode::DUPLICATE_LOGIN_ID;
	if (error_code != ErrorCode::SUCCESS) {
		server.SendError(session, error_code);
		return;
	}
	S2C_LOGIN_PACKET packet{};
	packet.header = {sizeof(packet), S2C_LOGIN};
	packet.player_id = result.player_id;
	StringToCharBuf(result.nickname, packet.nickname, sizeof(packet.nickname));
	session.SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
