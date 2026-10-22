#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CharacterExportCommandlet.generated.h"

// 추출 프로그램이 화면 없는 에디터로 부르는 캐릭터 판정 데이터 추출 명령(-run=CharacterExport).
// -out=파일 경로 -mesh=스켈레탈 메시 경로 -hitbox=히트박스 물리 에셋 경로:
//   서버 판정 데이터 파일을 쓰고, 쓴 파일의 뼈 자세를 언리얼 재생 결과와 비교한 결과를 기록에 남긴다.
UCLASS()
class UCharacterExportCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCharacterExportCommandlet();
	virtual int32 Main(const FString& params) override;
};
