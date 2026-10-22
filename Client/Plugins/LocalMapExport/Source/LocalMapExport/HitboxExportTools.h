#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HitboxExportTools.generated.h"

class UMaterialInterface;
class UPhysicsAsset;
class USkeletalMesh;

// 서버 판정에 쓰는 뼈 계층, 히트박스, 애니메이션 키를 파일로 내보내고 검증하는 도구
UCLASS()
class UHitboxExportTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// 뼈 계층과 기본 자세, 히트박스 도형, 애니메이션 키(30Hz), 이동·고정 자세·동작 재생 규칙을 서버용 파일로 쓴다.
	// 좌표는 액터 기준 cm이고 배율은 위치에 미리 곱해 둔다. 회전은 쿼터니언(x, y, z, w)이다.
	UFUNCTION(BlueprintCallable, Category="Local Hitbox")
	static FString ExportServerData(USkeletalMesh* mesh, UPhysicsAsset* hitbox_asset, const FString& output_path);

	// 내보낸 파일의 키로 계층 계산한 뼈 위치·회전을, 언리얼이 같은 애니메이션을 재생해 계산한 위치·회전과 비교한다.
	UFUNCTION(BlueprintCallable, Category="Local Hitbox")
	static FString VerifyServerData(USkeletalMesh* mesh, const FString& file_path);

	// 서버 검증 프로그램이 쓴 자세를 언리얼 계산과 수치로 비교한다. 달리기 하체 뼈, 좌우 방향·위치를 준 달리기 다리 도형, 곡괭이 자세 상체 회전을 본다.
	UFUNCTION(BlueprintCallable, Category="Local Hitbox")
	static FString CompareServerPoses(USkeletalMesh* mesh, UPhysicsAsset* hitbox_asset, const FString& poses_path);

	// 서버 검증 프로그램이 쓴 상태마다 서버 뼈 자세로 메시를 세우고 서버 히트박스를 반투명하게 겹쳐 여러 방향에서 렌더링한다.
	UFUNCTION(BlueprintCallable, Category="Local Hitbox")
	static void RenderServerPoses(USkeletalMesh* mesh, const FString& poses_path, UMaterialInterface* material, const FString& output_directory);
};
