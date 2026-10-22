#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MapExportCommandlet.generated.h"

// 추출 프로그램이 화면 없는 에디터로 부르는 맵 데이터 추출 명령(-run=MapExport).
// 공통: -map=맵 경로 -playmin=x,y -playmax=x,y (충돌 없는 물체를 넣을 플레이 영역)
// -mode=list -out=목록 파일: 맵 안의 에셋과 배치 수를 적는다.
// -mode=export -out=출력 폴더 [-collisions=충돌 모양을 쓸 항목 이름 목록 파일] [-placements=배치를 쓸 항목 이름 목록 파일] [-landscape] [-regions] [-rebuildgrass]:
//   낡은 풀 맵을 다시 만들어 저장하고, 고른 지형 파일과 항목별 폴더(Map/Meshes/항목 이름)의 충돌 모양·배치 파일, 영역 파일을 쓴다.
UCLASS()
class UMapExportCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UMapExportCommandlet();
	virtual int32 Main(const FString& params) override;
};
