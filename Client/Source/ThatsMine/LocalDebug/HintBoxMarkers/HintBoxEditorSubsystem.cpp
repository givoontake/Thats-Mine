#include "HintBoxEditorSubsystem.h"

#if WITH_EDITOR
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SceneInterface.h"
#include "SceneView.h"
#endif

bool UHintBoxEditorSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
#if WITH_EDITOR
	return world_type == EWorldType::Editor;
#else
	return false;
#endif
}

void UHintBoxEditorSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
#if WITH_EDITOR
	draw_handle_ = UDebugDrawService::Register(TEXT("Editor"), FDebugDrawDelegate::CreateUObject(this, &UHintBoxEditorSubsystem::DrawMarkers));
	actor_list_handle_ = GEngine->OnLevelActorListChanged().AddWeakLambda(this, [this]() { is_cache_dirty_ = true; });
	actor_added_handle_ = GEngine->OnLevelActorAdded().AddWeakLambda(this, [this](AActor* actor)
	{
		if (actor->GetWorld() == GetWorld())
		{
			is_cache_dirty_ = true;
		}
	});
	actor_deleted_handle_ = GEngine->OnLevelActorDeleted().AddWeakLambda(this, [this](AActor* actor)
	{
		if (actor->GetWorld() == GetWorld())
		{
			is_cache_dirty_ = true;
		}
	});
#endif
}

void UHintBoxEditorSubsystem::Deinitialize()
{
#if WITH_EDITOR
	UDebugDrawService::Unregister(draw_handle_);
	GEngine->OnLevelActorListChanged().Remove(actor_list_handle_);
	GEngine->OnLevelActorAdded().Remove(actor_added_handle_);
	GEngine->OnLevelActorDeleted().Remove(actor_deleted_handle_);
	boxes_.Reset();
#endif
	Super::Deinitialize();
}

#if WITH_EDITOR
void UHintBoxEditorSubsystem::DrawMarkers(UCanvas* canvas, APlayerController* controller)
{
	const FSceneView* view = canvas->SceneView;
	if (!view || !view->Family || !view->Family->Scene || view->Family->Scene->GetWorld() != GetWorld())
	{
		return;
	}

	// 상자 목록이 바뀔 때만 다시 수집하고, 위치는 현재 액터 좌표를 사용한다.
	if (is_cache_dirty_)
	{
		boxes_.Reset();
		for (TActorIterator<AStaticMeshActor> actor(GetWorld()); actor; ++actor)
		{
			if (actor->ActorHasTag(TEXT("HintBox")))
			{
				boxes_.Add(*actor);
			}
		}
		is_cache_dirty_ = false;
	}

	for (const TWeakObjectPtr<AActor>& box : boxes_)
	{
		AActor* actor = box.Get();
		if (!actor || actor->IsHiddenEd())
		{
			continue;
		}
		const FVector location = actor->GetComponentsBoundingBox().GetCenter();
		const FVector screen = canvas->Project(location);
		if (screen.Z <= 0.0 || screen.X < 0.0 || screen.X >= canvas->SizeX || screen.Y < 0.0 || screen.Y >= canvas->SizeY)
		{
			continue;
		}

		// 깊이 검사를 하지 않는 화면 표시라 지형과 건물 너머에서도 보인다.
		const FLinearColor color = actor->ActorHasTag(TEXT("HintGift")) ? FLinearColor(1.0f, 0.12f, 0.08f) : FLinearColor(1.0f, 0.7f, 0.12f);
		const FVector2D position(screen.X, screen.Y);
		canvas->K2_DrawBox(position - FVector2D(6.0, 6.0), FVector2D(12.0, 12.0), 4.0f, FLinearColor::Black);
		canvas->K2_DrawBox(position - FVector2D(6.0, 6.0), FVector2D(12.0, 12.0), 2.0f, color);
		canvas->K2_DrawText(GEngine->GetSmallFont(), actor->GetActorLabel().RightChop(8), position + FVector2D(9.0, -6.0), FVector2D::UnitVector, color, 0.0f, FLinearColor::Black, FVector2D::UnitVector, false, false, true);
	}
}
#endif
