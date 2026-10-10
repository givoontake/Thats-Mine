#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HintSubsystem.generated.h"

struct HINT_ENTRY;

// 서버가 보낸 힌트를 문장으로 만들어 내 캐릭터의 인벤토리에 넣는다.
// 문장 틀과 이름은 Content/Data/hint_texts.txt에서 읽고, 힌트 항목의 번호로 빈칸을 채운다. 온라인 게임 레벨에서만 동작한다.
UCLASS()
class THATSMINE_API UHintSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void LoadTexts();
	// 키의 문장. 파일에 없으면 키를 그대로 돌려준다.
	FString FindText(const FString& key) const;
	FString BuildSentence(const HINT_ENTRY& hint) const;
	void HandleHintPacket(const TArray<uint8>& payload);

	// 문장 파일의 키와 문장
	TMap<FString, FString> texts_;
	bool is_active_ = false;
};
