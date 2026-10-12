#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/inventory_hint.h"
#include "HintGradeBadge.generated.h"

class UMaterialInterface;

// 힌트 등급 칸. 둥근 테두리 안에 등급 글자를 그린다. 인벤토리와 힌트 획득 안내가 같이 쓴다.
// 글자와 테두리 색은 떨어진 두루마리의 밴드 색과 같고, S·A는 위에서 아래로 바뀌는 재질로 그린다.
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
	EHintGrade grade_ = EHintGrade::C;
};
