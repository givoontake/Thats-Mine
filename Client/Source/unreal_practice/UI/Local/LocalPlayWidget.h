#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LocalPlayWidget.generated.h"

UCLASS()
class UNREAL_PRACTICE_API ULocalPlayWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
