#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "HintFavoriteButton.generated.h"

DECLARE_DELEGATE_OneParam(FOnHintFavoriteClicked, int32);

// 인벤토리 힌트 줄의 즐겨찾기 별 버튼. 누르면 자기 힌트 번호(가진 힌트 목록의 번호)를 알린다.
UCLASS()
class THATSMINE_API UHintFavoriteButton : public UButton
{
	GENERATED_BODY()

public:
	int32 hint_index_ = INDEX_NONE;
	FOnHintFavoriteClicked on_favorite_clicked_;

	// 버튼 눌림(OnClicked)에 묶는다.
	UFUNCTION()
	void HandleClicked();
};
