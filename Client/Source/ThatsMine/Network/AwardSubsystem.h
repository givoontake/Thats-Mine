#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AwardSubsystem.generated.h"

class AAwardStage;
class UAwardWidget;
class USoundBase;

// 시상식. 게임이 끝나고 서버가 부문별 순위를 보내면 화면을 시상 무대 카메라로 바꾸고 부문마다 AWARD_SCENE_MS씩 보인다.
// 부문이 바뀔 때는 잠깐 어두워졌다 밝아지고, 부문을 보일 때마다 시상 소리를 낸다. 건너뛰기를 누르면 바로 다음 부문으로 넘어간다.
// 마지막 부문이 끝나거나 건너뛰면 서버에 끝났다고 보내 먼저 방으로 돌아간다. 온라인 게임 레벨에서만 동작한다.
UCLASS()
class THATSMINE_API UAwardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 지금 부문을 끝내고 다음 부문(없으면 시상식 끝)으로 넘어간다.
	void Skip();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	// 부문 전환 때 어두워지는(밝아지는) 시간(초)
	static constexpr float FADE_SECONDS = 0.3f;

	// 부문 하나: 부문 제목과 1위부터의 닉네임·횟수 문장
	struct FAwardScene
	{
		FText title;
		TArray<FString> nicknames;
		TArray<FText> records;
	};

	void HandleAwardPacket(const TArray<uint8>& payload);
	void StartCeremony();
	void ShowScene();
	void FinishCeremony();

	TArray<FAwardScene> scenes_;
	int32 scene_index_ = INDEX_NONE;
	bool is_switching_ = false;
	bool is_finished_ = false;
	FTimerHandle scene_timer_;

	UPROPERTY(Transient)
	TObjectPtr<AAwardStage> stage_;

	UPROPERTY(Transient)
	TObjectPtr<UAwardWidget> widget_;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> award_sound_;

	bool is_active_ = false;
};
