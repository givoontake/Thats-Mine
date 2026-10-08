#include "LoadingScreenSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Flow/ClientStateSubsystem.h"
#include "LoadingScreenWidget.h"
#include "Misc/CoreDelegates.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "LoadingScreen"

static const TCHAR* LOADING_BACKGROUND_PATH = TEXT("/Game/Thats_Mine/UI/Loading/T_LoadingPyramid.T_LoadingPyramid");
static const TCHAR* GAME_MAP_PACKAGE = TEXT("/Game/Thats_Mine/Levels/Demo_Landmass");
// 맵 패키지 불러오기가 차지하는 게이지 구간. 나머지는 맵이 열린 뒤 준비 단계가 채운다.
static constexpr float MAP_LOAD_PROGRESS = 0.8f;

static bool CanUseMoviePlayer()
{
	return !GIsEditor && IsMoviePlayerEnabled();
}

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	collection.InitializeDependency<UClientStateSubsystem>();
	GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->on_state_changed_.AddDynamic(this, &ULoadingScreenSubsystem::HandleClientStateChanged);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULoadingScreenSubsystem::HandleMapLoaded);
	FCoreDelegates::OnAsyncLoadingFlushUpdate.AddUObject(this, &ULoadingScreenSubsystem::HandleLoadingFlushUpdate);
}

void ULoadingScreenSubsystem::Deinitialize()
{
	Hide();
	if (UClientStateSubsystem* flow = GetGameInstance()->GetSubsystem<UClientStateSubsystem>())
	{
		flow->on_state_changed_.RemoveAll(this);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FCoreDelegates::OnAsyncLoadingFlushUpdate.RemoveAll(this);
	Super::Deinitialize();
}

void ULoadingScreenSubsystem::SetProgress(float progress)
{
	progress_.store(FMath::Max(progress_.load(), FMath::Clamp(progress, 0.0f, 1.0f)));
}

void ULoadingScreenSubsystem::SetStatus(const FText& status)
{
	status_ = status;
}

void ULoadingScreenSubsystem::HandleClientStateChanged(EClientState state)
{
	const bool should_show = state == EClientState::LOADING && !GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode();
	if (should_show && !is_shown_)
	{
		Show();
	}
	else if (!should_show && is_shown_)
	{
		Hide();
	}
}

void ULoadingScreenSubsystem::HandleMapLoaded(UWorld* world)
{
	if (!is_shown_ || !world || world->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	SetProgress(MAP_LOAD_PROGRESS);
	// 레벨이 바뀌면 화면 위 위젯이 모두 지워지므로, 새 레벨의 첫 화면 전에 다시 띄운다.
	AddToViewport();
}

// 맵을 여는 동안 게임 스레드가 비동기 로딩을 기다리며 주기적으로 부른다. 맵 패키지의 불러오기 진행률을 게이지에 반영한다.
void ULoadingScreenSubsystem::HandleLoadingFlushUpdate()
{
	if (!is_shown_)
	{
		return;
	}
	const float percent = GetAsyncLoadPercentage(FName(GAME_MAP_PACKAGE));
	if (percent >= 0.0f)
	{
		SetProgress(MAP_LOAD_PROGRESS * percent / 100.0f);
	}
}

void ULoadingScreenSubsystem::Show()
{
	background_ = LoadObject<UTexture2D>(nullptr, LOADING_BACKGROUND_PATH);
	progress_.store(0.0f);
	status_ = LOCTEXT("LoadingMap", "맵을 불러오는 중...");
	widget_ = SNew(SLoadingScreenWidget)
		.background(background_)
		.progress_Lambda([this]() { return progress_.load(); })
		.status_Lambda([this]() { return status_; });
	is_shown_ = true;
	// 맵 열기는 다음 프레임에 시작되므로, 그 전에 한 번 그려진 로딩 화면이 맵을 여는 동안 멈춘 화면으로 남는다.
	if (!CanUseMoviePlayer())
	{
		AddToViewport();
		return;
	}
	// 맵 로드가 시작되면 엔진이 이 화면을 별도 스레드에서 재생하고, 맵이 열리면 끝낸다. 이후 대기는 뷰포트 위젯이 이어 받는다.
	FLoadingScreenAttributes attributes;
	attributes.WidgetLoadingScreen = widget_;
	attributes.bMoviesAreSkippable = false;
	GetMoviePlayer()->SetupLoadingScreen(attributes);
}

void ULoadingScreenSubsystem::Hide()
{
	if (!is_shown_)
	{
		return;
	}
	if (CanUseMoviePlayer())
	{
		// 재생 전이면 다음 맵 로드에서 재생되지 않도록 설정을 비운다.
		if (GetMoviePlayer()->IsMovieCurrentlyPlaying())
		{
			GetMoviePlayer()->StopMovie();
		}
		else
		{
			GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
		}
	}
	UGameViewportClient* viewport = GetGameInstance()->GetGameViewportClient();
	if (is_in_viewport_ && viewport)
	{
		viewport->RemoveViewportWidgetContent(widget_.ToSharedRef());
	}
	is_in_viewport_ = false;
	is_shown_ = false;
	widget_.Reset();
	background_ = nullptr;
}

// 에디터 플레이에서 창을 여러 개 띄워도 각자의 창에 뜨도록 게임 인스턴스의 뷰포트에 붙인다.
void ULoadingScreenSubsystem::AddToViewport()
{
	UGameViewportClient* viewport = GetGameInstance()->GetGameViewportClient();
	if (!viewport)
	{
		return;
	}
	if (is_in_viewport_)
	{
		viewport->RemoveViewportWidgetContent(widget_.ToSharedRef());
	}
	viewport->AddViewportWidgetContent(widget_.ToSharedRef(), 1000);
	is_in_viewport_ = true;
}

#undef LOCTEXT_NAMESPACE
