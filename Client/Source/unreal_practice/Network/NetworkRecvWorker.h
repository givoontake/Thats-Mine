#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class NetworkConnection;

class NetworkRecvWorker final : public FRunnable
{
public:
	explicit NetworkRecvWorker(NetworkConnection& connection);
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	bool RecvPackets();
	NetworkConnection& connection_;
	TArray<uint8> recv_buffer_;
};
