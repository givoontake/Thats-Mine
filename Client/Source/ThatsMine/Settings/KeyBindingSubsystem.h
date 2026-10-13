#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "key_action.h"
#include "KeyBindingSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnKeyBindingsChanged);

// 동작마다 쓸 키. 내 컴퓨터의 사용자 설정 파일(GameUserSettings.ini)에 저장하고, 바꾸면 입력을 다시 묶도록 알린다.
UCLASS(Config = GameUserSettings)
class THATSMINE_API UKeyBindingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;

	static FKey GetDefaultKey(EKeyAction action);
	// 설정 조작 탭에 보이는 동작 이름
	static FText GetActionName(EKeyAction action);
	FKey GetKey(EKeyAction action) const { return keys_[static_cast<int32>(action)]; }
	// 동작 순서의 키로 모두 바꾸고 저장한 뒤 바뀐 것을 알린다.
	void SetKeys(const TArray<FKey>& keys);

	FOnKeyBindingsChanged on_keys_changed_;

private:
	// 동작 순서의 키
	UPROPERTY(Config)
	TArray<FKey> keys_;
};
