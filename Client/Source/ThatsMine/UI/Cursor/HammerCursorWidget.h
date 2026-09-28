#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HammerCursorWidget.generated.h"

class UTexture2D;

UCLASS()
class THATSMINE_API UHammerCursorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY(EditDefaultsOnly, Category = "Cursor")
	TSoftObjectPtr<UTexture2D> texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Shared/T_CursorHammerFlat.T_CursorHammerFlat")));

	UPROPERTY(EditDefaultsOnly, Category = "Cursor", meta = (ClampMin = "16", ClampMax = "96"))
	float size_ = 48.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Cursor")
	FVector2D hotspot_ = FVector2D(0.085, 0.36);
};
