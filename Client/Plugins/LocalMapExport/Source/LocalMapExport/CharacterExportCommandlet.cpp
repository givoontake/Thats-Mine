#include "CharacterExportCommandlet.h"
#include "HitboxExportTools.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterExport, Display, All);

// 보고 글을 줄마다 기록에 남긴다. 오류 줄은 오류로 남긴다.
static void LogReport(const FString& report)
{
	TArray<FString> lines;
	report.ParseIntoArrayLines(lines);
	for (const FString& line : lines)
	{
		if (line.StartsWith(TEXT("ERROR")))
		{
			UE_LOG(LogCharacterExport, Error, TEXT("%s"), *line);
		}
		else
		{
			UE_LOG(LogCharacterExport, Display, TEXT("%s"), *line);
		}
	}
}

// 검증에서 메시를 놓으려면 렌더 장면이 필요하다. 클라이언트가 아니면 엔진이 그리지 않는 빈 장면만 만든다.
UCharacterExportCommandlet::UCharacterExportCommandlet()
{
	IsClient = true;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCharacterExportCommandlet::Main(const FString& params)
{
	FString out;
	FString mesh_path;
	FString hitbox_path;
	if (!FParse::Value(*params, TEXT("out="), out) || !FParse::Value(*params, TEXT("mesh="), mesh_path) || !FParse::Value(*params, TEXT("hitbox="), hitbox_path))
	{
		UE_LOG(LogCharacterExport, Error, TEXT("인자 오류: -out= -mesh= -hitbox= 가 필요하다."));
		return 1;
	}
	USkeletalMesh* mesh = LoadObject<USkeletalMesh>(nullptr, *mesh_path);
	UPhysicsAsset* hitbox_asset = LoadObject<UPhysicsAsset>(nullptr, *hitbox_path);
	if (!mesh || !hitbox_asset)
	{
		UE_LOG(LogCharacterExport, Error, TEXT("메시나 히트박스 에셋을 불러오지 못했다: %s, %s"), *mesh_path, *hitbox_path);
		return 1;
	}
	UE_LOG(LogCharacterExport, Display, TEXT("캐릭터 판정 데이터 추출: %s"), *out);
	const FString report = UHitboxExportTools::ExportServerData(mesh, hitbox_asset, out);
	LogReport(report);
	if (report.Contains(TEXT("ERROR")))
	{
		return 1;
	}
	UE_LOG(LogCharacterExport, Display, TEXT("쓴 파일의 뼈 자세를 언리얼 재생 결과와 비교"));
	const FString verify = UHitboxExportTools::VerifyServerData(mesh, out);
	LogReport(verify);
	return verify.StartsWith(TEXT("ERROR")) ? 1 : 0;
}
