#pragma once

#include "CoreMinimal.h"
#include "map_place.generated.h"

USTRUCT(BlueprintType)
struct FMapPlace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	FVector2D position = FVector2D::ZeroVector;

	// 배율 1에서의 글자 크기. 영역이 클수록 크게 하되, 편차가 크지 않게 14~28 사이로 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float font_size = 22.0f;
};

// 게임 지도의 지명. 큰 지도와 블록 판이 같이 쓴다.
inline const TArray<FMapPlace>& GetMapPlaces()
{
	static const TArray<FMapPlace> places = {
		{FText::AsCultureInvariant(TEXT("Dragon Ridge")), FVector2D(0.350, 0.151), 28},
		{FText::AsCultureInvariant(TEXT("Start Dome")), FVector2D(0.841, 0.070), 14},
		{FText::AsCultureInvariant(TEXT("Ring Ruins")), FVector2D(0.225, 0.269), 20},
		{FText::AsCultureInvariant(TEXT("Daisy Meadow")), FVector2D(0.188, 0.416), 25},
		{FText::AsCultureInvariant(TEXT("Bamboo\nGrove")), FVector2D(0.560, 0.309), 20},
		{FText::AsCultureInvariant(TEXT("Pyramid")), FVector2D(0.632, 0.404), 18},
		{FText::AsCultureInvariant(TEXT("Egyptian\nVillage")), FVector2D(0.236, 0.540), 18},
		{FText::AsCultureInvariant(TEXT("Asian\nVillage")), FVector2D(0.547, 0.552), 21},
		{FText::AsCultureInvariant(TEXT("Maple Peak")), FVector2D(0.779, 0.566), 21},
		{FText::AsCultureInvariant(TEXT("Star Peak")), FVector2D(0.251, 0.756), 23},
		{FText::AsCultureInvariant(TEXT("Ship Peak")), FVector2D(0.773, 0.818), 23},
		{FText::AsCultureInvariant(TEXT("Oak Lake")), FVector2D(0.409, 0.332), 23},
		{FText::AsCultureInvariant(TEXT("Mini Lake")), FVector2D(0.668, 0.318), 18},
		{FText::AsCultureInvariant(TEXT("Blue Lake")), FVector2D(0.608, 0.730), 17},
		{FText::AsCultureInvariant(TEXT("Pond")), FVector2D(0.817, 0.031), 14}
	};
	return places;
}
