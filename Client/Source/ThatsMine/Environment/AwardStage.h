#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AwardStage.generated.h"

class UAnimSequence;
class UCameraComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// 시상 무대. 땅 위에 1위(가운데·가장 높음), 2위(카메라에서 왼쪽), 3위(오른쪽) 시상대와 무대를 바라보는 카메라를 둔다.
// 시상대는 땅속까지 이어 붙여 고르지 않은 땅에서도 밑이 뜨지 않게 한다. 액터 높이가 땅 높이다.
// 시상식 부문마다 시상대 위 전시용 캐릭터를 순위 수만큼 보이고, 캐릭터마다 시상 동작 중 하나를 무작위로 반복한다.
// 무대는 액터의 -X 쪽(카메라 쪽)을 바라본다. 충돌이 없어 게임 중에는 숨겨 두고 시상식에서만 보인다.
UCLASS()
class THATSMINE_API AAwardStage : public AActor
{
	GENERATED_BODY()

public:
	AAwardStage();

	// 무대를 보이고, 무대 둘레 지형 풀을 없애 풀이 무대를 뚫고 올라오지 않게 한다.
	void ShowStage();

	// 1위부터 count명의 캐릭터를 보이고 나머지는 숨긴다. 캐릭터 모습(메시·재질·배율)은 template_mesh를 따른다.
	void ShowFigures(int32 count, const USkeletalMeshComponent* template_mesh);
	// 그 순위 캐릭터 머리 위 이름 자리(월드 좌표)
	FVector GetNameLocation(int32 rank) const;
	// 그 순위 시상대 앞면(카메라 쪽) 가운데(월드 좌표). 순위 숫자를 여기에 그린다.
	FVector GetRankLocation(int32 rank) const;

	static constexpr int32 RANK_COUNT = 3;

private:
	// 땅 위로 보이는 시상대 높이(cm). 1위, 2위, 3위 순서다.
	static constexpr float PODIUM_HEIGHTS[RANK_COUNT] = { 110.0f, 85.0f, 65.0f };
	// 시상대를 땅속으로 묻는 깊이(cm)
	static constexpr float PODIUM_SINK = 40.0f;
	// 시상대 가로 간격과 블록 한 변 길이(cm)
	static constexpr float PODIUM_SPACING = 210.0f;
	static constexpr float BLOCK_SIZE = 200.0f;
	// 지형 풀을 없앨 때 무대 경계 상자 바깥으로 더 넓히는 거리(cm)
	static constexpr float GRASS_CLEAR_MARGIN = 100.0f;
	// 캐릭터 발에서 이름까지 높이(cm)
	static constexpr float NAME_HEIGHT = 175.0f;

	float GetPodiumY(int32 rank) const;

	UPROPERTY(VisibleAnywhere, Category = "Award Stage")
	TArray<TObjectPtr<UStaticMeshComponent>> podiums_;

	UPROPERTY(VisibleAnywhere, Category = "Award Stage")
	TArray<TObjectPtr<USkeletalMeshComponent>> figures_;

	UPROPERTY(VisibleAnywhere, Category = "Award Stage")
	TObjectPtr<UCameraComponent> camera_;

	// 캐릭터가 무작위로 고르는 시상 동작
	UPROPERTY(EditAnywhere, Category = "Award Stage")
	TArray<TObjectPtr<UAnimSequence>> poses_;
};
