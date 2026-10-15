#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Network/game_protocol.h"
#include "PingSubsystem.generated.h"

// 내가 찍은 핑 하나의 위치와, 찍은 것이 힌트에 나오는 물체면 그 종류. 나만 보고, 새로 찍으면 이전 핑을 대신한다.
// 게임 화면은 핑 표시와 종류, 내 발밑에서 핑까지 거리를, 미니맵과 큰 지도는 핑 위치를 매 프레임 읽어 그린다.
UCLASS()
class THATSMINE_API UPingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	const TOptional<FVector>& GetPingLocation() const { return ping_location_; }
	const TOptional<HintObjectKind>& GetPingObjectKind() const { return ping_object_kind_; }
	void SetPingLocation(const FVector& location) { ping_location_ = location; ping_object_kind_.Reset(); }
	// 핑을 맞은 곳에 찍고 맞은 물체의 힌트 종류를 정한다.
	void PlacePing(const FHitResult& hit);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	TOptional<FVector> ping_location_;
	TOptional<HintObjectKind> ping_object_kind_;
};
