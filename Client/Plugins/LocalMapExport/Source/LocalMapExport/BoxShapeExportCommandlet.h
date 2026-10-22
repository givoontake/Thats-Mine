#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BoxShapeExportCommandlet.generated.h"

// 추출 프로그램이 화면 없는 에디터로 부르는 상자 충돌 모양 추출 명령(-run=BoxShapeExport).
// -out=파일 경로: 서버 상자 충돌 모양 파일(box_shapes.txt)을 쓴다.
//   나무 상자 닫힘은 작은 상자 스켈레탈 메시의 물리 에셋 상자를 기본 자세로, 열림은 여는 애니메이션 마지막 자세로 놓은 모양이다.
//   선물 닫힘은 선물 메시의 상자 충돌이고, Bounds는 상자를 놓는 고정 메시의 경계 상자다.
UCLASS()
class UBoxShapeExportCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UBoxShapeExportCommandlet();
	virtual int32 Main(const FString& params) override;
};
