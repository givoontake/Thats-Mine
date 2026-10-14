#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PingSubsystem.generated.h"

// 내가 찍은 핑 하나의 위치. 나만 보고, 새로 찍으면 이전 핑을 대신한다.
// 게임 화면은 핑 표시와 내 발밑에서 핑까지 거리를, 미니맵과 큰 지도는 핑 위치를 매 프레임 읽어 그린다.
UCLASS()
class THATSMINE_API UPingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	const TOptional<FVector>& GetPingLocation() const { return ping_location_; }
	void SetPingLocation(const FVector& location) { ping_location_ = location; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	TOptional<FVector> ping_location_;
};
