#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoxLayoutSubsystem.generated.h"

class AHintBox;
class USoundBase;

// 게임 레벨에 이번 게임의 상자를 놓고, 상자 열기를 서버에 요청해 서버가 인정한 열림만 보여 준다.
// 온라인은 서버가 게임 시작 때 정해 보낸 상자 배치를 받아 놓는다. 로컬 플레이는 후보 데이터가 서버에만 있어 놓지 않는다.
UCLASS()
class THATSMINE_API UBoxLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 상자 열기를 서버에 요청한다. 이미 열린 상자이거나, 쏴서 열려는데 선물이 아니면 보내지 않는다.
	void RequestOpen(const AHintBox* box, bool is_shot, uint8 weapon) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandleBoxLayoutPacket(const TArray<uint8>& payload);
	void HandleBoxOpenPacket(const TArray<uint8>& payload);

	// 상자 번호 순서의 상자. 배치를 받을 때 이미 열린 선물은 만들지 않아 비어 있다.
	UPROPERTY()
	TArray<TObjectPtr<AHintBox>> boxes_;

	// 내가 연 상자에서만 재생하는 소리
	UPROPERTY()
	TObjectPtr<USoundBase> open_sound_;

	bool is_active_ = false;
};
