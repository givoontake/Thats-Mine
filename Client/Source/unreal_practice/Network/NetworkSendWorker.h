#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class NetworkConnection;

class NetworkSendWorker final : public FRunnable
{
public:
	explicit NetworkSendWorker(NetworkConnection& connection);
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	NetworkConnection& connection_;
};
