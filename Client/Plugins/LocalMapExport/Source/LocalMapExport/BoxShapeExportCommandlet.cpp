#include "BoxShapeExportCommandlet.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogBoxShapeExport, Display, All);

static const TCHAR* CHEST_SKELETAL_MESH = TEXT("/Game/Thats_Mine/Meshes/Props/Chests/SK_ChestSmall.SK_ChestSmall");
static const TCHAR* CHEST_OPEN_ANIMATION = TEXT("/Game/Thats_Mine/Meshes/Props/Chests/A_ChestSmall_Open60.A_ChestSmall_Open60");
static const TCHAR* CHEST_STATIC_MESH = TEXT("/Game/Thats_Mine/Meshes/Props/Chests/SM_Chest_Closed.SM_Chest_Closed");
static const TCHAR* GIFT_STATIC_MESH = TEXT("/Game/Thats_Mine/Meshes/Props/Gifts/SM_Gift_D_Red.SM_Gift_D_Red");

// 줄: 종류 상태 중심 xyz, 절반 크기 xyz, 회전 pitch yaw roll. 0에 가까운 값은 -0이 찍히지 않게 0으로 쓴다.
static FString ShapeLine(const TCHAR* kind, const TCHAR* state, const FTransform& box_to_mesh, const FVector& half_size)
{
	const auto clean = [](double value) { return FMath::Abs(value) < 0.0005 ? 0.0 : value; };
	const FVector center = box_to_mesh.GetTranslation();
	const FRotator rotation = box_to_mesh.Rotator();
	return FString::Printf(TEXT("%s %s %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f"), kind, state, clean(center.X), clean(center.Y), clean(center.Z),
		clean(half_size.X), clean(half_size.Y), clean(half_size.Z), clean(rotation.Pitch), clean(rotation.Yaw), clean(rotation.Roll));
}

// 뼈의 부모 기준 자세를 루트부터 곱해 메시 기준 자세로 바꾼다.
static TArray<FTransform> ToMeshSpace(const FReferenceSkeleton& reference, const TArray<FTransform>& locals)
{
	TArray<FTransform> poses;
	poses.SetNum(locals.Num());
	for (int32 bone = 0; bone < locals.Num(); ++bone)
	{
		const int32 parent = reference.GetParentIndex(bone);
		poses[bone] = parent == INDEX_NONE ? locals[bone] : locals[bone] * poses[parent];
	}
	return poses;
}

// 물리 에셋의 상자를 뼈 자세에 놓아 상자마다 한 줄씩 쓴다. 상자가 아닌 도형이 있으면 실패다.
static bool AddBodyLines(TArray<FString>& lines, const UPhysicsAsset* physics, const FReferenceSkeleton& reference, const TArray<FTransform>& poses, const TCHAR* state)
{
	for (const USkeletalBodySetup* body : physics->SkeletalBodySetups)
	{
		const int32 bone = reference.FindBoneIndex(body->BoneName);
		const FKAggregateGeom& geometry = body->AggGeom;
		if (bone == INDEX_NONE || geometry.BoxElems.IsEmpty() || geometry.SphereElems.Num() + geometry.SphylElems.Num() + geometry.ConvexElems.Num() > 0)
		{
			UE_LOG(LogBoxShapeExport, Error, TEXT("상자 하나짜리 몸체가 아니다: %s"), *body->BoneName.ToString());
			return false;
		}
		for (const FKBoxElem& box : geometry.BoxElems)
		{
			const FTransform box_to_mesh = box.GetTransform() * poses[bone];
			lines.Add(ShapeLine(TEXT("Treasure"), state, box_to_mesh, FVector(box.X, box.Y, box.Z) * 0.5 * box_to_mesh.GetScale3D()));
		}
	}
	return true;
}

static FString BoundsLine(const TCHAR* kind, const UStaticMesh* mesh)
{
	const FBoxSphereBounds bounds = mesh->GetBounds();
	return ShapeLine(kind, TEXT("Bounds"), FTransform(bounds.Origin), bounds.BoxExtent);
}

UBoxShapeExportCommandlet::UBoxShapeExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UBoxShapeExportCommandlet::Main(const FString& params)
{
	FString out;
	if (!FParse::Value(*params, TEXT("out="), out))
	{
		UE_LOG(LogBoxShapeExport, Error, TEXT("인자 오류: -out= 가 필요하다."));
		return 1;
	}
	USkeletalMesh* chest = LoadObject<USkeletalMesh>(nullptr, CHEST_SKELETAL_MESH);
	UAnimSequence* open_animation = LoadObject<UAnimSequence>(nullptr, CHEST_OPEN_ANIMATION);
	UStaticMesh* chest_static = LoadObject<UStaticMesh>(nullptr, CHEST_STATIC_MESH);
	UStaticMesh* gift = LoadObject<UStaticMesh>(nullptr, GIFT_STATIC_MESH);
	const UPhysicsAsset* physics = chest ? chest->GetPhysicsAsset() : nullptr;
	if (!chest || !open_animation || !chest_static || !gift || !physics)
	{
		UE_LOG(LogBoxShapeExport, Error, TEXT("상자 에셋을 불러오지 못했거나 작은 상자에 물리 에셋이 없다."));
		return 1;
	}

	// 닫힘은 메시 기본 자세, 열림은 여는 애니메이션 마지막 자세다. 애니메이션에 트랙이 없는 뼈는 기본 자세를 쓴다.
	const FReferenceSkeleton& reference = chest->GetRefSkeleton();
	const FReferenceSkeleton& animation_reference = open_animation->GetSkeleton()->GetReferenceSkeleton();
	TArray<FTransform> open_locals = reference.GetRefBonePose();
	for (int32 bone = 0; bone < reference.GetNum(); ++bone)
	{
		const FName name = reference.GetBoneName(bone);
		const int32 animation_bone = animation_reference.FindBoneIndex(name);
		if (animation_bone != INDEX_NONE && open_animation->GetDataModel()->IsValidBoneTrackName(name))
		{
			open_animation->GetBoneTransform(open_locals[bone], FSkeletonPoseBoneIndex(animation_bone), FAnimExtractContext(static_cast<double>(open_animation->GetPlayLength())), true);
		}
	}

	TArray<FString> lines = {
		TEXT("# 상자 충돌 모양: 종류 상태 중심x 중심y 중심z 절반x 절반y 절반z pitch yaw roll"),
		TEXT("# 종류 = Treasure(나무 상자)/Gift(선물), 상태 = Closed(닫힘)/Open(열림)/Bounds(메시 경계 상자). 직육면체 하나가 한 줄이다."),
		TEXT("# 상자를 놓은 자리(바닥 가운데) 기준 좌표(cm)이고, 회전은 언리얼 회전 순서(도)다."),
		TEXT("# 나무 상자는 작은 상자 물리 에셋의 몸통·뚜껑 상자이고, 열린 모양은 여는 애니메이션 마지막 자세다. 선물은 열리면 사라져 열린 모양이 없다."),
		TEXT("# Bounds는 메시 경계 상자이고, 서버가 상호작용 범위 배율을 곱해 F키 시선 판정에 쓴다."),
	};
	if (!AddBodyLines(lines, physics, reference, ToMeshSpace(reference, reference.GetRefBonePose()), TEXT("Closed"))
		|| !AddBodyLines(lines, physics, reference, ToMeshSpace(reference, open_locals), TEXT("Open")))
	{
		return 1;
	}
	const UBodySetup* gift_body = gift->GetBodySetup();
	if (!gift_body || gift_body->AggGeom.BoxElems.IsEmpty())
	{
		UE_LOG(LogBoxShapeExport, Error, TEXT("선물 메시에 상자 충돌이 없다."));
		return 1;
	}
	for (const FKBoxElem& box : gift_body->AggGeom.BoxElems)
	{
		lines.Add(ShapeLine(TEXT("Gift"), TEXT("Closed"), box.GetTransform(), FVector(box.X, box.Y, box.Z) * 0.5));
	}
	lines.Add(BoundsLine(TEXT("Treasure"), chest_static));
	lines.Add(BoundsLine(TEXT("Gift"), gift));
	if (!FFileHelper::SaveStringToFile(FString::Join(lines, TEXT("\r\n")) + TEXT("\r\n"), *out, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogBoxShapeExport, Error, TEXT("상자 충돌 모양 파일 저장 실패: %s"), *out);
		return 1;
	}
	for (int32 index = 5; index < lines.Num(); ++index)
	{
		UE_LOG(LogBoxShapeExport, Display, TEXT("%s"), *lines[index]);
	}
	UE_LOG(LogBoxShapeExport, Display, TEXT("상자 충돌 모양 저장: %s"), *out);
	return 0;
}
