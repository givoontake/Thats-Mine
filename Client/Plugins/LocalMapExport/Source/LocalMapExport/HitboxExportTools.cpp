#include "HitboxExportTools.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AssetCompilingManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Player/PlayerAnimInstance.h"
#include "Player/PlayerCharacter.h"
#include "Player/player_action.h"

static const TCHAR* EXPORT_CHARACTER_BLUEPRINT = TEXT("/Game/Thats_Mine/Actors/Characters/BP_KayKitBarbarian.BP_KayKitBarbarian");
static const TCHAR* EXPORT_BLEND_SPACE = TEXT("/Game/Thats_Mine/Animations/Move/BS_KayKitMovement.BS_KayKitMovement");
static const TCHAR* EXPORT_JUMP_SEQUENCE = TEXT("/Game/Thats_Mine/Animations/Jump/Rig_Medium_MovementBasicJump_Idle.Rig_Medium_MovementBasicJump_Idle");
static constexpr uint32 EXPORT_VERSION = 4;
static constexpr float EXPORT_SAMPLE_RATE = 30.0f;
// 정해진 순서로 쓰는 고정 자세. 서버도 같은 순서를 쓴다.
static const TCHAR* EXPORT_POSE_NAMES[] = { TEXT("IDLE"), TEXT("PICKAXE_HOLD"), TEXT("PISTOL_AIM"), TEXT("RIFLE_HOLD"), TEXT("RIFLE_AIM") };
static const TCHAR* EXPORT_POSE_SEQUENCES[] = {
	TEXT("/Game/Thats_Mine/Animations/Move/Rig_Medium_GeneralIdle_A.Rig_Medium_GeneralIdle_A"),
	TEXT("/Game/Thats_Mine/Animations/Actions/Mining/AS_PickaxeHold.AS_PickaxeHold"),
	TEXT("/Game/Thats_Mine/Animations/Actions/Pistol/AS_PistolAim.AS_PistolAim"),
	TEXT("/Game/Thats_Mine/Animations/Rig_Medium_MovementAdvancedRunning_HoldingRifle.Rig_Medium_MovementAdvancedRunning_HoldingRifle"),
	TEXT("/Game/Thats_Mine/Animations/Actions/Aim/AS_SniperAimShoulder.AS_SniperAimShoulder"),
};
// 애니메이션 블루프린트의 고정 자세 재생 위치(초)
static const float EXPORT_POSE_TIMES[] = { 0.0f, 0.0f, 1.066667f, 0.0f, 0.0f };
// 서버만 쓰는 동작 값. 무기를 바꿀 때 재생하는 꺼내기 동작과, 무기와 상관없는 동작을 나타낸다.
static constexpr uint8 EXPORT_ACTION_EQUIP = 255;
static constexpr uint8 EXPORT_ANY_WEAPON = 255;

namespace
{
	// 애니메이션 하나를 액터 기준 cm로 30Hz마다 뽑은 키. keys는 [키 번호 * 뼈 수 + 뼈 번호]다.
	struct FExportTrack
	{
		FString source;
		float start_time = 0.0f;
		float length = 0.0f;
		TArray<FTransform> keys;
	};

	struct FExportMontage
	{
		uint8 action = 0;
		uint8 weapon = 0;
		int32 track = INDEX_NONE;
		float play_rate = 1.0f;
		float blend_in = 0.0f;
		float blend_out = 0.0f;
		uint8 region = 0;
	};

	// 내보낼 뼈 목록과 메시 → 액터 변환. 위치에 곱할 누적 배율을 뼈마다 둔다.
	struct FExportSkeleton
	{
		TArray<FName> names;
		TArray<int32> parents;
		TArray<int32> mesh_indices;
		TArray<double> scales;
		FTransform mesh_to_actor;
	};

	class FByteWriter
	{
	public:
		TArray<uint8> bytes;

		template <typename T>
		void Write(const T& value)
		{
			bytes.Append(reinterpret_cast<const uint8*>(&value), sizeof(T));
		}

		void WriteName(const FString& name, int32 size)
		{
			TArray<uint8> buffer;
			buffer.SetNumZeroed(size);
			const FTCHARToUTF8 utf8(*name);
			FMemory::Memcpy(buffer.GetData(), utf8.Get(), FMath::Min(utf8.Length(), size - 1));
			bytes.Append(buffer);
		}

		void WriteTransform(const FTransform& transform)
		{
			const FQuat rotation = transform.GetRotation();
			const FVector position = transform.GetTranslation();
			for (const double value : { rotation.X, rotation.Y, rotation.Z, rotation.W, position.X, position.Y, position.Z })
			{
				Write(static_cast<float>(value));
			}
		}
	};

	class FByteReader
	{
	public:
		const TArray<uint8>& bytes;
		int32 offset = 0;

		explicit FByteReader(const TArray<uint8>& source) : bytes(source) {}

		template <typename T>
		T Read()
		{
			T value;
			FMemory::Memcpy(&value, bytes.GetData() + offset, sizeof(T));
			offset += sizeof(T);
			return value;
		}

		FString ReadName(int32 size)
		{
			const FString name = UTF8_TO_TCHAR(reinterpret_cast<const char*>(bytes.GetData() + offset));
			offset += size;
			return name;
		}

		FTransform ReadTransform()
		{
			float values[7];
			for (float& value : values)
			{
				value = Read<float>();
			}
			return FTransform(FQuat(values[0], values[1], values[2], values[3]), FVector(values[4], values[5], values[6]));
		}
	};
}

static const APlayerCharacter* GetCharacterDefaults()
{
	UBlueprint* blueprint = LoadObject<UBlueprint>(nullptr, EXPORT_CHARACTER_BLUEPRINT);
	check(blueprint);
	return CastChecked<APlayerCharacter>(blueprint->GeneratedClass->GetDefaultObject());
}

// 손 소켓용 뼈와 가상 뼈를 뺀 뼈를 부모 먼저 오는 순서로 고른다. 메시 → 액터 배율과 기본 자세 배율을 누적해 둔다.
static FExportSkeleton BuildExportSkeleton(USkeletalMesh* mesh)
{
	FExportSkeleton skeleton;
	skeleton.mesh_to_actor = GetCharacterDefaults()->GetMesh()->GetRelativeTransform();
	const FReferenceSkeleton& reference = mesh->GetRefSkeleton();
	TArray<double> mesh_scales;
	mesh_scales.SetNum(reference.GetNum());
	for (int32 index = 0; index < reference.GetNum(); ++index)
	{
		const int32 parent = reference.GetParentIndex(index);
		mesh_scales[index] = (parent == INDEX_NONE ? skeleton.mesh_to_actor.GetScale3D().X : mesh_scales[parent]) * reference.GetRefBonePose()[index].GetScale3D().X;
		const FString name = reference.GetBoneName(index).ToString();
		if (name.Contains(TEXT("handslot")) || name.StartsWith(TEXT("VB ")))
		{
			continue;
		}
		const int32 export_parent = parent == INDEX_NONE ? INDEX_NONE : skeleton.mesh_indices.IndexOfByKey(parent);
		check(parent == INDEX_NONE || export_parent != INDEX_NONE);
		skeleton.names.Add(reference.GetBoneName(index));
		skeleton.parents.Add(export_parent);
		skeleton.mesh_indices.Add(index);
		skeleton.scales.Add(mesh_scales[index]);
	}
	return skeleton;
}

// 뼈의 부모 기준 자세를 서버용으로 바꾼다. 위치에 부모까지의 누적 배율을 곱하고, 루트는 메시 → 액터 변환을 적용해 액터 기준으로 만든다.
static FTransform ToServerLocal(const FExportSkeleton& skeleton, int32 bone, const FTransform& local)
{
	const int32 parent = skeleton.parents[bone];
	if (parent == INDEX_NONE)
	{
		const FTransform& mesh_to_actor = skeleton.mesh_to_actor;
		return FTransform(mesh_to_actor.GetRotation() * local.GetRotation(), mesh_to_actor.TransformPosition(local.GetTranslation()));
	}
	return FTransform(local.GetRotation(), local.GetTranslation() * skeleton.scales[parent]);
}

// 애니메이션의 지정 구간을 30Hz로 뽑는다. 트랙이 없는 뼈는 메시 기본 자세를 쓴다. 키의 배율이 기본 자세와 다르면 report에 적는다.
static FExportTrack SampleTrack(const FExportSkeleton& skeleton, USkeletalMesh* mesh, UAnimSequence* sequence, float start_time, float length, FString& report)
{
	FExportTrack track;
	track.source = sequence->GetPathName();
	track.start_time = start_time;
	track.length = length;
	const FReferenceSkeleton& skeleton_reference = sequence->GetSkeleton()->GetReferenceSkeleton();
	const FReferenceSkeleton& mesh_reference = mesh->GetRefSkeleton();
	const int32 key_count = FMath::CeilToInt(length * EXPORT_SAMPLE_RATE - 0.001f) + 1;
	double max_scale_error = 0.0;
	FString scale_error_bones;
	for (int32 key = 0; key < key_count; ++key)
	{
		const double time = start_time + FMath::Min(key / EXPORT_SAMPLE_RATE, length);
		for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
		{
			const int32 skeleton_index = skeleton_reference.FindBoneIndex(skeleton.names[bone]);
			FTransform local = mesh_reference.GetRefBonePose()[skeleton.mesh_indices[bone]];
			if (skeleton_index != INDEX_NONE && sequence->GetDataModel()->IsValidBoneTrackName(skeleton.names[bone]))
			{
				sequence->GetBoneTransform(local, FSkeletonPoseBoneIndex(skeleton_index), FAnimExtractContext(time), true);
			}
			const double scale_error = (local.GetScale3D() - mesh_reference.GetRefBonePose()[skeleton.mesh_indices[bone]].GetScale3D()).GetAbsMax();
			if (scale_error > 0.001 && !scale_error_bones.Contains(skeleton.names[bone].ToString()))
			{
				scale_error_bones += skeleton.names[bone].ToString() + FString::Printf(TEXT("(%s) "), *local.GetScale3D().ToString());
			}
			max_scale_error = FMath::Max(max_scale_error, scale_error);
			track.keys.Add(ToServerLocal(skeleton, bone, local));
		}
	}
	if (max_scale_error > 0.001)
	{
		report += FString::Printf(TEXT("WARN scale differs from reference %s max=%.4f bones=%s\n"), *track.source, max_scale_error, *scale_error_bones);
	}
	return track;
}

// 키 하나의 뼈 부모 기준 자세를 루트부터 차례로 곱해 액터 기준 자세를 구한다. 서버의 계층 계산과 같은 식이다.
static TArray<FTransform> ComposeKey(const FExportSkeleton& skeleton, const FExportTrack& track, int32 key)
{
	const int32 bone_count = skeleton.names.Num();
	TArray<FTransform> poses;
	poses.SetNum(bone_count);
	for (int32 bone = 0; bone < bone_count; ++bone)
	{
		const FTransform& local = track.keys[key * bone_count + bone];
		const int32 parent = skeleton.parents[bone];
		poses[bone] = parent == INDEX_NONE ? local : FTransform(poses[parent].GetRotation() * local.GetRotation(), poses[parent].GetTranslation() + poses[parent].GetRotation().RotateVector(local.GetTranslation()));
	}
	return poses;
}

FString UHitboxExportTools::ExportServerData(USkeletalMesh* mesh, UPhysicsAsset* hitbox_asset, const FString& output_path)
{
	check(mesh && hitbox_asset);
	FString report;
	const FExportSkeleton skeleton = BuildExportSkeleton(mesh);
	const APlayerCharacter* character_defaults = GetCharacterDefaults();
	const USkeletalMeshComponent* template_mesh = character_defaults->GetMesh();
	report += FString::Printf(TEXT("mesh_to_actor %s\n"), *skeleton.mesh_to_actor.ToString());
	for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
	{
		report += FString::Printf(TEXT("BONE %d %s parent=%d scale=%.4f\n"), bone, *skeleton.names[bone].ToString(), skeleton.parents[bone], skeleton.scales[bone]);
	}

	TArray<FExportTrack> tracks;
	TMap<FString, int32> track_lookup;
	const auto add_track = [&](UAnimSequence* sequence, float start_time, float length)
	{
		check(sequence);
		const FString key = FString::Printf(TEXT("%s@%.4f+%.4f"), *sequence->GetPathName(), start_time, length);
		if (const int32* found = track_lookup.Find(key))
		{
			return *found;
		}
		tracks.Add(SampleTrack(skeleton, mesh, sequence, start_time, length, report));
		return track_lookup.Add(key, tracks.Num() - 1);
	};

	// 이동: 블렌드 스페이스 샘플마다 애니메이션 전체를 반복 재생용으로 뽑는다.
	const UBlendSpace* blend_space = LoadObject<UBlendSpace>(nullptr, EXPORT_BLEND_SPACE);
	check(blend_space);
	struct FLocomotionSample { float speed; float direction; int32 track; float rate_scale; };
	TArray<FLocomotionSample> locomotion_samples;
	for (const FBlendSample& sample : blend_space->GetBlendSamples())
	{
		UAnimSequence* sequence = sample.Animation.Get();
		locomotion_samples.Add({ static_cast<float>(sample.SampleValue.X), static_cast<float>(sample.SampleValue.Y), add_track(sequence, 0.0f, sequence->GetPlayLength()), sample.RateScale });
	}
	// 고정 자세: 정해진 재생 위치의 키 하나만 뽑는다.
	TArray<int32> pose_tracks;
	for (int32 pose = 0; pose < UE_ARRAY_COUNT(EXPORT_POSE_NAMES); ++pose)
	{
		UAnimSequence* sequence = LoadObject<UAnimSequence>(nullptr, EXPORT_POSE_SEQUENCES[pose]);
		pose_tracks.Add(add_track(sequence, FMath::Min(EXPORT_POSE_TIMES[pose], static_cast<float>(sequence->GetPlayLength())), 0.0f));
	}
	UAnimSequence* jump_sequence = LoadObject<UAnimSequence>(nullptr, EXPORT_JUMP_SEQUENCE);
	const int32 jump_track = add_track(jump_sequence, 0.0f, jump_sequence->GetPlayLength());

	// 동작: 캐릭터 블루프린트의 무기·줍기 설정에서 몽타주와 재생 속도를 읽는다. 몽타주의 첫 구간 애니메이션을 뽑는다.
	TArray<FExportMontage> montages;
	const auto add_montage = [&](UAnimMontage* montage, uint8 action, uint8 weapon, float play_rate)
	{
		if (!montage || montage->SlotAnimTracks.IsEmpty() || montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty())
		{
			return;
		}
		const FSlotAnimationTrack& slot = montage->SlotAnimTracks[0];
		const FAnimSegment& segment = slot.AnimTrack.AnimSegments[0];
		if (slot.AnimTrack.AnimSegments.Num() > 1 || !FMath::IsNearlyEqual(segment.AnimPlayRate, 1.0f) || segment.LoopingCount != 1)
		{
			report += FString::Printf(TEXT("WARN montage %s has segments=%d rate=%.3f loops=%d, only the first segment at rate 1 is exported\n"), *montage->GetName(), slot.AnimTrack.AnimSegments.Num(), segment.AnimPlayRate, segment.LoopingCount);
		}
		FExportMontage entry;
		entry.action = action;
		entry.weapon = weapon;
		entry.track = add_track(CastChecked<UAnimSequence>(segment.GetAnimReference()), segment.AnimStartTime, segment.AnimEndTime - segment.AnimStartTime);
		entry.play_rate = play_rate;
		entry.blend_in = montage->BlendIn.GetBlendTime();
		entry.blend_out = montage->BlendOut.GetBlendTime();
		// 전신 슬롯은 0, 허리 아래 상체 슬롯은 1, 오른팔 슬롯은 2
		entry.region = slot.SlotName == TEXT("FireSlot") ? 2 : (slot.SlotName == TEXT("DefaultSlot") ? 0 : 1);
		montages.Add(entry);
		report += FString::Printf(TEXT("MONTAGE %s action=%d weapon=%d slot=%s rate=%.4f blend=%.3f/%.3f region=%d\n"), *montage->GetName(), action, weapon, *slot.SlotName.ToString(), play_rate, entry.blend_in, entry.blend_out, entry.region);
	};
	const TMap<EWeaponType, FWeaponSettings>& weapon_settings = *FindFProperty<FMapProperty>(APlayerCharacter::StaticClass(), TEXT("weapon_settings_"))->ContainerPtrToValuePtr<TMap<EWeaponType, FWeaponSettings>>(character_defaults);
	const FActionSettings& pickup_action = *FindFProperty<FStructProperty>(APlayerCharacter::StaticClass(), TEXT("pickup_action_"))->ContainerPtrToValuePtr<FActionSettings>(character_defaults);
	for (const TPair<EWeaponType, FWeaponSettings>& pair : weapon_settings)
	{
		const uint8 weapon = static_cast<uint8>(pair.Key);
		const FWeaponSettings& settings = pair.Value;
		if (settings.melee_hit.is_enabled || settings.hitscan.is_enabled)
		{
			add_montage(settings.primary_action.montage, static_cast<uint8>(settings.melee_hit.is_enabled ? EPlayerAction::MELEE : EPlayerAction::FIRE), weapon, settings.primary_action.play_rate);
		}
		if (settings.hitscan.is_enabled && settings.hitscan.reload_montage)
		{
			add_montage(settings.hitscan.reload_montage, static_cast<uint8>(EPlayerAction::RELOAD), weapon, settings.hitscan.reload_montage->GetPlayLength() / FMath::Max(0.01f, settings.hitscan.reload_duration));
		}
		if (settings.equip_montage)
		{
			add_montage(settings.equip_montage, EXPORT_ACTION_EQUIP, weapon, settings.equip_montage->GetPlayLength() / APlayerCharacter::GetEquipDuration(static_cast<EWeaponType>(weapon)));
		}
	}
	add_montage(pickup_action.montage, static_cast<uint8>(EPlayerAction::PICKUP), EXPORT_ANY_WEAPON, pickup_action.play_rate);

	// 고개 상하 제한은 애니메이션 블루프린트 기본값에서 읽는다.
	const UObject* anim_defaults = const_cast<USkeletalMeshComponent*>(template_mesh)->GetAnimClass()->GetDefaultObject();
	const float head_pitch_up = *FindFProperty<FFloatProperty>(UPlayerAnimInstance::StaticClass(), TEXT("head_pitch_up_limit_"))->ContainerPtrToValuePtr<float>(anim_defaults);
	const float head_pitch_down = *FindFProperty<FFloatProperty>(UPlayerAnimInstance::StaticClass(), TEXT("head_pitch_down_limit_"))->ContainerPtrToValuePtr<float>(anim_defaults);

	// 히트박스: 도형을 뼈 기준 cm로 바꾼다(위치·크기에 뼈의 누적 배율을 곱한다).
	FByteWriter shapes_writer;
	uint32 body_count = 0;
	for (const USkeletalBodySetup* body : hitbox_asset->SkeletalBodySetups)
	{
		const int32 bone = skeleton.names.IndexOfByKey(body->BoneName);
		check(bone != INDEX_NONE);
		const FString name = body->BoneName.ToString();
		// 부위: 0 머리, 1 몸통, 2 팔, 3 손, 4 다리, 5 발
		const uint8 part = name == TEXT("head") ? 0 : (name == TEXT("hips") || name == TEXT("spine") ? 1 : (name.StartsWith(TEXT("upperarm")) || name.StartsWith(TEXT("lowerarm")) ? 2 : (name.StartsWith(TEXT("hand")) ? 3 : (name.StartsWith(TEXT("foot")) ? 5 : 4))));
		const double scale = skeleton.scales[bone];
		const FKAggregateGeom& geometry = body->AggGeom;
		shapes_writer.Write(static_cast<int32>(bone));
		shapes_writer.Write(part);
		shapes_writer.Write(static_cast<uint32>(geometry.SphereElems.Num() + geometry.SphylElems.Num() + geometry.BoxElems.Num()));
		const auto write_shape = [&](uint8 type, const FTransform& element, float radius, float length, const FVector& half_size)
		{
			shapes_writer.Write(type);
			shapes_writer.WriteTransform(FTransform(element.GetRotation(), element.GetTranslation() * scale));
			shapes_writer.Write(static_cast<float>(radius * scale));
			shapes_writer.Write(static_cast<float>(length * scale));
			for (const double value : { half_size.X, half_size.Y, half_size.Z })
			{
				shapes_writer.Write(static_cast<float>(value * scale));
			}
		};
		for (const FKSphereElem& sphere : geometry.SphereElems)
		{
			write_shape(0, FTransform(sphere.Center), sphere.Radius, 0.0f, FVector::ZeroVector);
		}
		for (const FKSphylElem& capsule : geometry.SphylElems)
		{
			write_shape(1, capsule.GetTransform(), capsule.Radius, capsule.Length, FVector::ZeroVector);
		}
		for (const FKBoxElem& box : geometry.BoxElems)
		{
			write_shape(2, box.GetTransform(), 0.0f, 0.0f, FVector(box.X, box.Y, box.Z) * 0.5);
		}
		++body_count;
	}
	// 뼈별 정점 구: 뼈에 조금이라도 묶인 정점을 기본 자세에서 그 뼈 기준(cm, 배율 없음)으로 모아 감싸는 구를 구한다.
	// 정점은 묶인 뼈들이 각자 옮긴 위치의 가중 평균에 놓이므로, 이 구들을 모두 덮는 볼록한 도형은 어떤 자세의 메시도 덮는다.
	FExportTrack reference_track;
	for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
	{
		reference_track.keys.Add(ToServerLocal(skeleton, bone, mesh->GetRefSkeleton().GetRefBonePose()[skeleton.mesh_indices[bone]]));
	}
	const TArray<FTransform> reference_poses = ComposeKey(skeleton, reference_track, 0);
	TArray<TArray<FVector>> bone_points;
	bone_points.SetNum(skeleton.names.Num());
	int32 remapped_influences = 0;
	for (const FSkelMeshSection& section : mesh->GetImportedModel()->LODModels[0].Sections)
	{
		for (const FSoftSkinVertex& vertex : section.SoftVertices)
		{
			const FVector position = skeleton.mesh_to_actor.TransformPosition(FVector(vertex.Position));
			for (int32 influence = 0; influence < MAX_TOTAL_INFLUENCES; ++influence)
			{
				if (vertex.InfluenceWeights[influence] == 0)
				{
					continue;
				}
				// 내보내지 않은 뼈(손 소켓 등)에 묶인 정점은 내보낸 가장 가까운 조상 뼈에 넣는다.
				int32 mesh_bone = section.BoneMap[vertex.InfluenceBones[influence]];
				int32 bone = skeleton.mesh_indices.IndexOfByKey(mesh_bone);
				while (bone == INDEX_NONE && mesh_bone != INDEX_NONE)
				{
					mesh_bone = mesh->GetRefSkeleton().GetParentIndex(mesh_bone);
					bone = skeleton.mesh_indices.IndexOfByKey(mesh_bone);
					++remapped_influences;
				}
				if (bone != INDEX_NONE)
				{
					bone_points[bone].Add(reference_poses[bone].GetRotation().UnrotateVector(position - reference_poses[bone].GetTranslation()));
				}
			}
		}
	}
	// 바도이우-클락슨 방식으로 중심을 가장 먼 점 쪽으로 조금씩 옮긴 뒤, 반지름은 가장 먼 점까지로 정해 모든 점을 덮는다.
	TArray<FVector4f> bone_spheres;
	for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
	{
		const TArray<FVector>& points = bone_points[bone];
		if (points.IsEmpty())
		{
			bone_spheres.Add(FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
			continue;
		}
		FVector center = points[0];
		for (int32 step = 1; step <= 300; ++step)
		{
			const FVector* farthest = &points[0];
			for (const FVector& point : points)
			{
				farthest = FVector::DistSquared(point, center) > FVector::DistSquared(*farthest, center) ? &point : farthest;
			}
			center += (*farthest - center) / (step + 1.0);
		}
		double radius = 0.0;
		for (const FVector& point : points)
		{
			radius = FMath::Max(radius, FVector::Distance(point, center));
		}
		bone_spheres.Add(FVector4f(FVector3f(center), static_cast<float>(radius)));
		report += FString::Printf(TEXT("BONE_SPHERE %s points=%d center=%s radius=%.1fcm\n"), *skeleton.names[bone].ToString(), points.Num(), *center.ToString(), radius);
	}
	report += FString::Printf(TEXT("REMAPPED_INFLUENCES %d\n"), remapped_influences);

	FByteWriter writer;
	writer.bytes.Append(reinterpret_cast<const uint8*>("TMHB"), 4);
	writer.Write(EXPORT_VERSION);
	writer.Write(static_cast<uint32>(skeleton.names.Num()));
	for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
	{
		writer.WriteName(skeleton.names[bone].ToString(), 32);
		writer.Write(static_cast<int32>(skeleton.parents[bone]));
	}
	writer.Write(head_pitch_up);
	writer.Write(head_pitch_down);
	// 무기별 상체 자세(0 없음, 1 곡괭이, 2 권총, 3 소총), 조준 상하 제한, 총(광선 판정) 여부와 사거리, 탄창 크기, 재장전 시간, 발사 간격. 무기 값 순서대로 쓴다.
	writer.Write(static_cast<uint32>(weapon_settings.Num()));
	for (uint8 weapon = 0; weapon < weapon_settings.Num(); ++weapon)
	{
		const FWeaponSettings& settings = weapon_settings.FindChecked(static_cast<EWeaponType>(weapon));
		writer.Write(static_cast<uint8>(settings.uses_pickaxe_pose ? 1 : (settings.uses_pistol_pose ? 2 : (settings.uses_rifle_pose ? 3 : 0))));
		writer.Write(settings.aim_pitch_up_limit);
		writer.Write(settings.aim_pitch_down_limit);
		writer.Write(static_cast<uint8>(settings.hitscan.is_enabled ? 1 : 0));
		writer.Write(settings.hitscan.distance);
		writer.Write(static_cast<uint32>(FMath::Max(1, settings.hitscan.magazine_capacity)));
		writer.Write(FMath::Max(0.01f, settings.hitscan.reload_duration));
		writer.Write(FMath::Max(0.01f, settings.hitscan.fire_interval));
	}
	writer.Write(body_count);
	writer.bytes.Append(shapes_writer.bytes);
	writer.Write(static_cast<uint32>(tracks.Num()));
	for (const FExportTrack& track : tracks)
	{
		writer.WriteName(track.source, 256);
		writer.Write(track.start_time);
		writer.Write(track.length);
		writer.Write(static_cast<uint32>(track.keys.Num() / skeleton.names.Num()));
		for (const FTransform& key : track.keys)
		{
			writer.WriteTransform(key);
		}
	}
	writer.Write(static_cast<uint32>(locomotion_samples.Num()));
	for (const FLocomotionSample& sample : locomotion_samples)
	{
		writer.Write(sample.speed);
		writer.Write(sample.direction);
		writer.Write(static_cast<uint32>(sample.track));
		writer.Write(sample.rate_scale);
	}
	writer.Write(static_cast<uint32>(pose_tracks.Num()));
	for (int32 pose = 0; pose < pose_tracks.Num(); ++pose)
	{
		writer.WriteName(EXPORT_POSE_NAMES[pose], 32);
		writer.Write(static_cast<uint32>(pose_tracks[pose]));
	}
	writer.Write(static_cast<uint32>(jump_track));
	writer.Write(static_cast<uint32>(montages.Num()));
	for (const FExportMontage& montage : montages)
	{
		writer.Write(montage.action);
		writer.Write(montage.weapon);
		writer.Write(static_cast<uint32>(montage.track));
		writer.Write(montage.play_rate);
		writer.Write(montage.blend_in);
		writer.Write(montage.blend_out);
		writer.Write(montage.region);
	}
	// 뼈 순서대로 뼈별 정점 구(중심 xyz, 반지름). 정점이 없는 뼈는 반지름 0이다.
	writer.Write(static_cast<uint32>(bone_spheres.Num()));
	for (const FVector4f& sphere : bone_spheres)
	{
		for (const float value : { sphere.X, sphere.Y, sphere.Z, sphere.W })
		{
			writer.Write(value);
		}
	}
	if (!FFileHelper::SaveArrayToFile(writer.bytes, *output_path))
	{
		return report + TEXT("ERROR save failed\n");
	}
	report += FString::Printf(TEXT("EXPORTED bones=%d bodies=%d tracks=%d locomotion=%d poses=%d montages=%d head=%.0f/%.0f bytes=%d\n"), skeleton.names.Num(), body_count, tracks.Num(), locomotion_samples.Num(), pose_tracks.Num(), montages.Num(), head_pitch_up, head_pitch_down, writer.bytes.Num());
	for (int32 index = 0; index < tracks.Num(); ++index)
	{
		report += FString::Printf(TEXT("TRACK %d %s start=%.4f length=%.4f keys=%d\n"), index, *tracks[index].source, tracks[index].start_time, tracks[index].length, tracks[index].keys.Num() / skeleton.names.Num());
	}
	return report;
}

FString UHitboxExportTools::VerifyServerData(USkeletalMesh* mesh, const FString& file_path)
{
	check(mesh);
	TArray<uint8> bytes;
	if (!FFileHelper::LoadFileToArray(bytes, *file_path))
	{
		return TEXT("ERROR load failed\n");
	}
	FByteReader reader(bytes);
	const FString magic = FString::ConstructFromPtrSize(reinterpret_cast<const ANSICHAR*>(bytes.GetData()), 4);
	reader.offset = 4;
	const uint32 version = reader.Read<uint32>();
	if (magic != TEXT("TMHB") || version != EXPORT_VERSION)
	{
		return TEXT("ERROR bad header\n");
	}
	FExportSkeleton skeleton;
	const uint32 bone_count = reader.Read<uint32>();
	for (uint32 bone = 0; bone < bone_count; ++bone)
	{
		skeleton.names.Add(FName(*reader.ReadName(32)));
		skeleton.parents.Add(reader.Read<int32>());
	}
	reader.Read<float>();
	reader.Read<float>();
	const uint32 weapon_count = reader.Read<uint32>();
	reader.offset += weapon_count * (2 + 5 * sizeof(float) + sizeof(uint32));
	const uint32 body_count = reader.Read<uint32>();
	for (uint32 body = 0; body < body_count; ++body)
	{
		reader.Read<int32>();
		reader.Read<uint8>();
		const uint32 shape_count = reader.Read<uint32>();
		reader.offset += shape_count * (1 + 7 * sizeof(float) + 5 * sizeof(float));
	}
	TArray<FExportTrack> tracks;
	const uint32 track_count = reader.Read<uint32>();
	for (uint32 index = 0; index < track_count; ++index)
	{
		FExportTrack& track = tracks.AddDefaulted_GetRef();
		track.source = reader.ReadName(256);
		track.start_time = reader.Read<float>();
		track.length = reader.Read<float>();
		const uint32 key_count = reader.Read<uint32>();
		for (uint32 key = 0; key < key_count * bone_count; ++key)
		{
			track.keys.Add(reader.ReadTransform());
		}
	}

	// 캐릭터 블루프린트와 같은 상대 변환으로 메시를 놓고, 키 시각마다 언리얼이 계산한 뼈 자세와 파일로 계산한 자세를 비교한다.
	const USkeletalMeshComponent* template_mesh = GetCharacterDefaults()->GetMesh();
	FAssetCompilingManager::Get().FinishAllCompilation();
	UWorld* world = GEditor->GetEditorWorldContext().World();
	AActor* actor = world->SpawnActor<AActor>();
	USceneComponent* root = NewObject<USceneComponent>(actor);
	actor->SetRootComponent(root);
	root->RegisterComponent();
	USkeletalMeshComponent* body = NewObject<USkeletalMeshComponent>(actor);
	body->SetSkeletalMesh(mesh);
	body->SetupAttachment(root);
	body->SetRelativeTransform(template_mesh->GetRelativeTransform());
	body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	body->RegisterComponent();
	FString result;
	double total_max_position = 0.0;
	double total_max_angle = 0.0;
	for (const FExportTrack& track : tracks)
	{
		UAnimSequence* sequence = LoadObject<UAnimSequence>(nullptr, *track.source);
		check(sequence);
		body->SetAnimation(sequence);
		const int32 key_count = track.keys.Num() / bone_count;
		double max_position = 0.0;
		double max_angle = 0.0;
		FString worst;
		for (const int32 key : { 0, key_count / 2, key_count - 1 })
		{
			body->SetPosition(track.start_time + FMath::Min(key / EXPORT_SAMPLE_RATE, track.length), false);
			body->TickAnimation(0.0f, false);
			body->RefreshBoneTransforms();
			const TArray<FTransform> poses = ComposeKey(skeleton, track, key);
			for (uint32 bone = 0; bone < bone_count; ++bone)
			{
				const FTransform engine = body->GetBoneTransform(body->GetBoneIndex(skeleton.names[bone]));
				const double position_error = FVector::Distance(engine.GetTranslation(), poses[bone].GetTranslation());
				const double angle_error = FMath::RadiansToDegrees(engine.GetRotation().AngularDistance(poses[bone].GetRotation()));
				if (position_error > max_position)
				{
					worst = FString::Printf(TEXT("%s key=%d"), *skeleton.names[bone].ToString(), key);
				}
				max_position = FMath::Max(max_position, position_error);
				max_angle = FMath::Max(max_angle, angle_error);
			}
		}
		total_max_position = FMath::Max(total_max_position, max_position);
		total_max_angle = FMath::Max(total_max_angle, max_angle);
		result += FString::Printf(TEXT("VERIFY %s max_position=%.3fcm max_angle=%.3fdeg worst=%s\n"), *FPaths::GetBaseFilename(track.source), max_position, max_angle, *worst);
	}
	actor->Destroy();
	return FString::Printf(TEXT("VERIFY_ALL max_position=%.3fcm max_angle=%.3fdeg\n"), total_max_position, total_max_angle) + result;
}

namespace
{
	// 서버 검증 프로그램이 쓴 상태 하나. 뼈는 액터 기준, 도형은 월드 기준이다.
	struct FServerPoseState
	{
		struct FShape { int32 type; int32 part; FTransform transform; float radius; float length; FVector half_size; };
		FString name;
		float yaw = 0.0f;
		FVector location = FVector::ZeroVector;
		TArray<FTransform> bones;
		TArray<FShape> shapes;
	};
}

static TArray<FServerPoseState> LoadServerPoses(const FString& poses_path)
{
	TArray<FString> lines;
	FFileHelper::LoadFileToStringArray(lines, *poses_path);
	TArray<FServerPoseState> states;
	for (const FString& line : lines)
	{
		TArray<FString> words;
		line.ParseIntoArrayWS(words);
		if (words.IsEmpty())
		{
			continue;
		}
		const auto number = [&words](int32 index) { return FCString::Atof(*words[index]); };
		if (words[0] == TEXT("STATE"))
		{
			FServerPoseState& state = states.AddDefaulted_GetRef();
			state.name = words[1];
			state.yaw = number(2);
			state.location = FVector(number(3), number(4), number(5));
		}
		else if (words[0] == TEXT("BONE"))
		{
			states.Last().bones.Add(FTransform(FQuat(number(2), number(3), number(4), number(5)), FVector(number(6), number(7), number(8))));
		}
		else if (words[0] == TEXT("SHAPE"))
		{
			states.Last().shapes.Add({ FCString::Atoi(*words[1]), FCString::Atoi(*words[2]), FTransform(FQuat(number(3), number(4), number(5), number(6)), FVector(number(7), number(8), number(9))), number(10), number(11), FVector(number(12), number(13), number(14)) });
		}
	}
	return states;
}

FString UHitboxExportTools::CompareServerPoses(USkeletalMesh* mesh, UPhysicsAsset* hitbox_asset, const FString& poses_path)
{
	check(mesh && hitbox_asset);
	const FExportSkeleton skeleton = BuildExportSkeleton(mesh);
	const TArray<FServerPoseState> states = LoadServerPoses(poses_path);
	const USkeletalMeshComponent* template_mesh = GetCharacterDefaults()->GetMesh();
	FAssetCompilingManager::Get().FinishAllCompilation();
	UWorld* world = GEditor->GetEditorWorldContext().World();
	AActor* actor = world->SpawnActor<AActor>();
	USceneComponent* root = NewObject<USceneComponent>(actor);
	actor->SetRootComponent(root);
	root->RegisterComponent();
	USkeletalMeshComponent* body = NewObject<USkeletalMeshComponent>(actor);
	body->SetSkeletalMesh(mesh);
	body->SetupAttachment(root);
	body->SetRelativeTransform(template_mesh->GetRelativeTransform());
	body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	body->RegisterComponent();
	const auto pose_engine = [&](const TCHAR* sequence_path, float time, float yaw, const FVector& location)
	{
		root->SetWorldLocationAndRotation(location, FRotator(0.0, yaw, 0.0));
		body->SetAnimation(LoadObject<UAnimSequence>(nullptr, sequence_path));
		body->SetPosition(time, false);
		body->TickAnimation(0.0f, false);
		body->RefreshBoneTransforms();
	};
	const auto find_state = [&states](const TCHAR* name) { return states.FindByPredicate([name](const FServerPoseState& state) { return state.name == name; }); };
	const auto is_lower_body = [](const FString& name) { return name == TEXT("Rig_Medium") || name == TEXT("root") || name == TEXT("hips") || name.Contains(TEXT("leg")) || name.Contains(TEXT("foot")) || name.Contains(TEXT("toes")); };
	const TCHAR* running = TEXT("/Game/Thats_Mine/Animations/Move/Rig_Medium_MovementBasicRunning_A.Rig_Medium_MovementBasicRunning_A");
	FString result;

	// 1. 앞으로 600 달리기 0.5초 뒤: 하체 뼈가 달리기 애니메이션 0.2초(정규화 0.25) 자세와 같아야 한다.
	if (const FServerPoseState* state = find_state(TEXT("check_run_forward")))
	{
		pose_engine(running, 0.2f, 0.0f, FVector::ZeroVector);
		double max_position = 0.0;
		double max_angle = 0.0;
		for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
		{
			if (!is_lower_body(skeleton.names[bone].ToString()))
			{
				continue;
			}
			const FTransform engine = body->GetBoneTransform(body->GetBoneIndex(skeleton.names[bone]));
			max_position = FMath::Max(max_position, FVector::Distance(engine.GetTranslation(), state->bones[bone].GetTranslation()));
			max_angle = FMath::Max(max_angle, FMath::RadiansToDegrees(engine.GetRotation().AngularDistance(state->bones[bone].GetRotation())));
		}
		result += FString::Printf(TEXT("COMPARE run_forward lower_body bones max_position=%.3fcm max_angle=%.3fdeg\n"), max_position, max_angle);
	}
	// 2. 같은 달리기를 좌우 90도·위치 (100,200,300)에서: 다리 도형의 월드 위치·회전이 언리얼 물리 바디 계산과 같아야 한다.
	if (const FServerPoseState* state = find_state(TEXT("check_run_forward_yaw")))
	{
		pose_engine(running, 0.2f, 90.0f, FVector(100.0, 200.0, 300.0));
		double max_position = 0.0;
		double max_angle = 0.0;
		double max_radius = 0.0;
		int32 shape_index = 0;
		for (const USkeletalBodySetup* setup : hitbox_asset->SkeletalBodySetups)
		{
			const FTransform bone_to_world = body->GetBoneTransform(body->GetBoneIndex(setup->BoneName));
			const double scale = bone_to_world.GetScale3D().X;
			const bool is_compared = is_lower_body(setup->BoneName.ToString());
			const auto compare = [&](const FTransform& element, float radius)
			{
				const FServerPoseState::FShape& shape = state->shapes[shape_index++];
				if (!is_compared)
				{
					return;
				}
				const FTransform engine = element * bone_to_world;
				max_position = FMath::Max(max_position, FVector::Distance(engine.GetTranslation(), shape.transform.GetTranslation()));
				max_angle = FMath::Max(max_angle, FMath::RadiansToDegrees(engine.GetRotation().AngularDistance(shape.transform.GetRotation())));
				max_radius = FMath::Max(max_radius, FMath::Abs(radius * scale - shape.radius));
			};
			for (const FKSphereElem& element : setup->AggGeom.SphereElems)
			{
				compare(FTransform(element.Center), element.Radius);
			}
			for (const FKSphylElem& element : setup->AggGeom.SphylElems)
			{
				compare(element.GetTransform(), element.Radius);
			}
			for (const FKBoxElem& element : setup->AggGeom.BoxElems)
			{
				compare(element.GetTransform(), 0.0f);
			}
		}
		result += FString::Printf(TEXT("COMPARE run_forward_yaw leg shapes max_position=%.3fcm max_angle=%.3fdeg max_radius=%.3fcm\n"), max_position, max_angle, max_radius);
	}
	// 3. 곡괭이 대기: 허리부터 위 뼈(고개 제외)의 액터 기준 회전이 곡괭이 자세 애니메이션과 같아야 한다.
	if (const FServerPoseState* state = find_state(TEXT("check_pickaxe_idle")))
	{
		pose_engine(TEXT("/Game/Thats_Mine/Animations/Actions/Mining/AS_PickaxeHold.AS_PickaxeHold"), 0.0f, 0.0f, FVector::ZeroVector);
		double max_angle = 0.0;
		for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
		{
			const FString name = skeleton.names[bone].ToString();
			if (is_lower_body(name) || name == TEXT("head"))
			{
				continue;
			}
			const FTransform engine = body->GetBoneTransform(body->GetBoneIndex(skeleton.names[bone]));
			max_angle = FMath::Max(max_angle, FMath::RadiansToDegrees(engine.GetRotation().AngularDistance(state->bones[bone].GetRotation())));
		}
		result += FString::Printf(TEXT("COMPARE pickaxe_idle upper_body rotations max_angle=%.3fdeg\n"), max_angle);
	}
	actor->Destroy();
	return result;
}

void UHitboxExportTools::RenderServerPoses(USkeletalMesh* mesh, const FString& poses_path, UMaterialInterface* material, const FString& output_directory)
{
	check(mesh && material);
	const FExportSkeleton skeleton = BuildExportSkeleton(mesh);
	const USkeletalMeshComponent* template_mesh = GetCharacterDefaults()->GetMesh();
	UStaticMesh* cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	FAssetCompilingManager::Get().FinishAllCompilation();
	UWorld* world = GEditor->GetEditorWorldContext().World();
	ADirectionalLight* light = world->SpawnActor<ADirectionalLight>();
	light->GetComponent()->SetIntensity(10.0f);
	ASceneCapture2D* camera = world->SpawnActor<ASceneCapture2D>();
	USceneCaptureComponent2D* capture = camera->GetCaptureComponent2D();
	capture->bCaptureEveryFrame = false;
	capture->bCaptureOnMovement = false;
	capture->FOVAngle = 40.0f;
	capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	capture->PostProcessSettings.AutoExposureBias = 0.0f;
	capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	constexpr int32 SIZE = 640;
	UTextureRenderTarget2D* target = NewObject<UTextureRenderTarget2D>();
	target->RenderTargetFormat = RTF_RGBA8;
	target->ClearColor = FLinearColor(0.08f, 0.09f, 0.11f, 1.0f);
	target->InitAutoFormat(SIZE, SIZE);
	target->UpdateResourceImmediate();
	capture->TextureTarget = target;
	const FVector look_at(0.0, 0.0, -15.0);
	const TArray<TTuple<FString, FVector>> views = {
		{ TEXT("front"), FVector(320.0, 0.0, 0.0) },
		{ TEXT("right"), FVector(0.0, 320.0, 0.0) },
		{ TEXT("front_right"), FVector(225.0, 225.0, 40.0) },
		{ TEXT("back_left"), FVector(-225.0, -225.0, 40.0) },
	};
	// 부위별 색: 머리 빨강, 몸통 주황, 팔·손 파랑, 다리·발 초록, 1차 판정 캡슐(부위 6) 회색
	const FLinearColor part_colors[] = { FLinearColor(1.0f, 0.1f, 0.1f), FLinearColor(1.0f, 0.6f, 0.1f), FLinearColor(0.1f, 0.6f, 1.0f), FLinearColor(0.1f, 0.6f, 1.0f), FLinearColor(0.2f, 1.0f, 0.3f), FLinearColor(0.2f, 1.0f, 0.3f), FLinearColor(0.85f, 0.85f, 0.85f) };
	for (const FServerPoseState& state : LoadServerPoses(poses_path))
	{
		if (!state.location.IsZero() || state.yaw != 0.0f)
		{
			continue;
		}
		AActor* actor = world->SpawnActor<AActor>();
		USceneComponent* root = NewObject<USceneComponent>(actor);
		actor->SetRootComponent(root);
		root->RegisterComponent();
		// 서버가 계산한 액터 기준 뼈 자세로 메시를 세운다. 언리얼 뼈 자세에는 배율이 들어가므로 뼈마다 누적 배율을 넣는다.
		UPoseableMeshComponent* body = NewObject<UPoseableMeshComponent>(actor);
		body->SetSkinnedAssetAndUpdate(mesh);
		for (int32 index = 0; index < template_mesh->OverrideMaterials.Num(); ++index)
		{
			if (template_mesh->OverrideMaterials[index])
			{
				body->SetMaterial(index, template_mesh->OverrideMaterials[index]);
			}
		}
		body->SetupAttachment(root);
		body->RegisterComponent();
		// 부모 뼈 자세에 대한 상대 자세를 직접 넣는다. 내보내지 않은 뼈(손 소켓 등)는 기본 자세 그대로 부모를 따른다.
		for (int32 bone = 0; bone < skeleton.names.Num(); ++bone)
		{
			const FTransform bone_world(state.bones[bone].GetRotation(), state.bones[bone].GetTranslation(), FVector(skeleton.scales[bone]));
			const int32 parent = skeleton.parents[bone];
			const FTransform parent_world = parent == INDEX_NONE ? FTransform::Identity : FTransform(state.bones[parent].GetRotation(), state.bones[parent].GetTranslation(), FVector(skeleton.scales[parent]));
			body->BoneSpaceTransforms[skeleton.mesh_indices[bone]] = bone_world.GetRelativeTransform(parent_world);
		}
		body->RefreshBoneTransforms();
		const auto add_shape = [&](UStaticMesh* static_mesh, const FTransform& shape_to_world, int32 part)
		{
			UStaticMeshComponent* component = NewObject<UStaticMeshComponent>(actor);
			component->SetStaticMesh(static_mesh);
			component->SetupAttachment(root);
			component->SetWorldTransform(shape_to_world);
			component->SetCastShadow(false);
			component->RegisterComponent();
			UMaterialInstanceDynamic* tinted = component->CreateDynamicMaterialInstance(0, material);
			tinted->SetVectorParameterValue(TEXT("Color"), part_colors[FMath::Clamp(part, 0, 6)]);
		};
		for (const FServerPoseState::FShape& shape : state.shapes)
		{
			if (shape.type == 0)
			{
				add_shape(sphere, FTransform(FQuat::Identity, shape.transform.GetTranslation(), FVector(shape.radius / 50.0f)), shape.part);
			}
			else if (shape.type == 1)
			{
				add_shape(cylinder, FTransform(FQuat::Identity, FVector::ZeroVector, FVector(shape.radius / 50.0f, shape.radius / 50.0f, shape.length / 100.0f)) * shape.transform, shape.part);
				for (const float end : { -0.5f, 0.5f })
				{
					add_shape(sphere, FTransform(FQuat::Identity, FVector(0.0, 0.0, shape.length * end), FVector(shape.radius / 50.0f)) * shape.transform, shape.part);
				}
			}
			else
			{
				add_shape(cube, FTransform(FQuat::Identity, FVector::ZeroVector, shape.half_size / 50.0f) * shape.transform, shape.part);
			}
		}
		const FString folder = FPaths::Combine(output_directory, state.name);
		for (const TTuple<FString, FVector>& view : views)
		{
			const FRotator rotation = (look_at - view.Get<1>()).Rotation();
			camera->SetActorLocationAndRotation(view.Get<1>(), rotation);
			light->SetActorRotation(FRotator(rotation.Pitch - 25.0, rotation.Yaw + 20.0, 0.0));
			world->SendAllEndOfFrameUpdates();
			FlushRenderingCommands();
			capture->CaptureScene();
			FlushRenderingCommands();
			TArray<FColor> pixels;
			FReadSurfaceDataFlags flags(RCM_UNorm);
			flags.SetLinearToGamma(false);
			target->GameThread_GetRenderTargetResource()->ReadPixels(pixels, flags);
			for (FColor& pixel : pixels)
			{
				pixel.A = 255;
			}
			TArray64<uint8> png;
			FImageUtils::PNGCompressImageArray(SIZE, SIZE, pixels, png);
			FFileHelper::SaveArrayToFile(png, *FPaths::Combine(folder, view.Get<0>() + TEXT(".png")));
		}
		actor->Destroy();
	}
	camera->Destroy();
	light->Destroy();
}
