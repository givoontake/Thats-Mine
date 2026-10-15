#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/inventory_hint.h"
#include "HintGradeBadge.generated.h"

class UMaterialInterface;

// 아이템 등급 칸. 둥근 테두리 안에 등급 글자를 그린다. 인벤토리 아이템 탭과 아이템 획득 안내가 같이 쓴다.
// S·A 글자와 테두리는 위에서 아래로 바뀌는 재질로 그린다.
UCLASS()
class THATSMINE_API UHintGradeBadge : public UUserWidget
{
	GENERATED_BODY()

public:
	UHintGradeBadge(const FObjectInitializer& object_initializer);
	// 그릴 등급. 부모에 붙이기 전에 정한다.
	void SetGrade(EHintGrade grade) { grade_ = grade; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// 등급 순서(S, A)의 글자·테두리 재질. 그 뒤 등급은 한 색이다.
	UPROPERTY(EditDefaultsOnly, Category = "Hint")
	TArray<TSoftObjectPtr<UMaterialInterface>> grade_text_materials_;

	UPROPERTY(EditDefaultsOnly, Category = "Hint")
	TArray<TSoftObjectPtr<UMaterialInterface>> grade_border_materials_;

private:
	// 등급 글자를 위로 옮기는 거리. 게임 글꼴의 영문 글자가 글자 칸 안에서 아래로 치우친 만큼이다.
	static constexpr double GLYPH_RAISE = 4.5;

	EHintGrade grade_ = EHintGrade::C;
};
