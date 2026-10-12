#pragma once

#include "CoreMinimal.h"
#include "StunGaugeWidget.h"
#include "SearchGaugeWidget.generated.h"

// 돋보기 탐색 게이지. 기절 게이지와 같은 모양에 돋보기 아이콘을 두고, 시작하면 정한 시간 동안 일정하게 차오른다.
UCLASS()
class THATSMINE_API USearchGaugeWidget : public UStunGaugeWidget
{
	GENERATED_BODY()

public:
	USearchGaugeWidget(const FObjectInitializer& object_initializer);
	// 빈 게이지를 보이고 duration_seconds 동안 채운다.
	void Start(float duration_seconds);
	// 게이지를 숨긴다.
	void Stop();

protected:
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	float duration_ = 1.0f;
	// 시작한 뒤 지난 시간. 채우는 중이 아니면 음수다.
	float elapsed_ = -1.0f;
};
