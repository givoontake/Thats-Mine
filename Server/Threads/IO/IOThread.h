#pragma once
#include <WinSock2.h>
#include "ServerThread.h"
#include "types.h"

class GameServer;
class Session;
struct ExOverlapped;
struct SessionKey;
struct SendBuffer;

class IOThread final : public ServerThread
{
	GameServer& game_server_;

	void ProcessAcceptCompletion(BOOL result, ExOverlapped* ex_over);
	void ProcessSessionCompletion(BOOL result, DWORD transferred_bytes, ExOverlapped* ex_over);
	bool ProcessRecvBuffer(Session& session, int recv_bytes);
	void ProcessReceiveCompletion(BOOL result, DWORD transferred_bytes, Session& session, SessionKey session_key);
	void ProcessSendCompletion(BOOL result, DWORD transferred_bytes, SendBuffer* send_buffer, SessionKey session_key);

public:
	IOThread(GameServer& game_server);
	void Run() override;
	void Close() override;
};
