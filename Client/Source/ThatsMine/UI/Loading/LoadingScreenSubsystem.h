#pragma once

#include <atomic>
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Flow/client_state.h"
#include "LoadingScreenSubsystem.generated.h"

class SLoadingScreenWidget;
class UTexture2D;

// 온라인 게임의 로딩 상태 동안 로딩 화면을 띄운다.
// 맵을 여는 동안은 엔진 로딩 화면 기능이 별도 스레드에서 그리고, 맵이 열린 뒤에도 플레이 시작까지 같은 화면을 유지한다.
// 엔진 로딩 화면을 쓸 수 없는 에디터 플레이에서는 게임 화면 위에 띄워, 맵을 여는 동안 멈춘 화면이 로딩 화면이 되게 한다.
UCLASS()
class THATSMINE_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	// 진행률은 줄어들지 않는다.
	void SetProgress(float progress);
	void SetStatus(const FText& status);

private:
	UFUNCTION()
	void HandleClientStateChanged(EClientState state);
	void HandleMapLoaded(UWorld* world);
	void HandleLoadingFlushUpdate();
	void Show();
	void Hide();
	void AddToViewport();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> background_;

	TSharedPtr<SLoadingScreenWidget> widget_;
	// 맵을 여는 동안 로딩 화면 스레드가 읽는다.
	std::atomic<float> progress_{ 0.0f };
	FText status_;
	bool is_shown_ = false;
	bool is_in_viewport_ = false;
};
