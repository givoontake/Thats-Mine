#include "MapExportCommandlet.h"
#include "MapAssetCollector.h"
#include "grass_placement.h"
#include "map_file_writer.h"
#include "AssetCompilingManager.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Interface_CollisionDataProviderCore.h"
#include "LandscapeComponent.h"
#include "LandscapeEditTypes.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogMapExport, Display, All);

// 출력 폴더는 서버 Data 폴더와 같은 구성이다. 맵 파일은 출력 폴더/Map 아래에, 영역 파일은 출력 폴더 바로 아래에 쓴다.
// 항목마다 출력 폴더/Map/Meshes/항목 이름/ 아래에 파일을 쓴다. 충돌 배치는 충돌 모양(메시) 단위, 충돌 없는 물체는 폴리지 종류(액터는 메시) 단위 항목이다.
// 충돌 모양 파일(collision.bin): "TMCS", 버전 u32, 이름[64], 판정 방식 u8(0 단순 도형, 1 삼각형), 양면 u8,
//   구·상자·캡슐·볼록 도형 목록, 삼각형 정점·인덱스(서버 맵 파일과 같은 형식)
// 배치 파일(placements.bin): "TMPL", 버전 u32, 이름[64],
//   충돌 배치: 개수, 각각 플래그 u8(1 보이는 채널 막음, 2 폰 채널 막음, 4 움직이는 부품), 위치·회전·크기
//   충돌 없는 물체: 개수, 각각 위치 float x y z
// 지형 파일(출력 폴더/Map/Landscape.bin): "TMLS", 버전 u32, 개수, 각각 위치·회전·크기, 격자 칸 수, 칸 배율, 높이 u16 (칸+1)^2개, 칸 플래그 u8 칸^2개
static constexpr uint32 COLLISION_FILE_VERSION = 1;
static constexpr uint32 PLACEMENT_FILE_VERSION = 1;
static constexpr uint32 LANDSCAPE_FILE_VERSION = 1;

static void WriteSimpleShapes(FMapFileWriter& writer, const UBodySetup* body)
{
	const FKAggregateGeom& geometry = body->AggGeom;
	writer.Write(static_cast<uint32>(geometry.SphereElems.Num()));
	for (const FKSphereElem& sphere : geometry.SphereElems)
	{
		writer.WriteVector(sphere.Center);
		writer.Write(sphere.Radius);
	}
	writer.Write(static_cast<uint32>(geometry.BoxElems.Num()));
	for (const FKBoxElem& box : geometry.BoxElems)
	{
		writer.WriteVector(box.Center);
		writer.WriteQuat(box.Rotation.Quaternion());
		writer.WriteVector(FVector(box.X * 0.5, box.Y * 0.5, box.Z * 0.5));
	}
	writer.Write(static_cast<uint32>(geometry.SphylElems.Num()));
	for (const FKSphylElem& capsule : geometry.SphylElems)
	{
		writer.WriteVector(capsule.Center);
		writer.WriteQuat(capsule.Rotation.Quaternion());
		writer.Write(capsule.Radius);
		writer.Write(capsule.Length);
	}
	writer.Write(static_cast<uint32>(geometry.ConvexElems.Num()));
	for (const FKConvexElem& convex : geometry.ConvexElems)
	{
		// 볼록 도형의 점은 도형 자체 변환을 적용한 메시 기준으로 저장한다.
		const FTransform transform = convex.GetTransform();
		writer.Write(static_cast<uint32>(convex.VertexData.Num()));
		for (const FVector& vertex : convex.VertexData)
		{
			writer.WriteVector(transform.TransformPosition(vertex));
		}
		writer.Write(static_cast<uint32>(convex.IndexData.Num()));
		for (const int32 index : convex.IndexData)
		{
			writer.Write(static_cast<uint32>(index));
		}
	}
	writer.Write(static_cast<uint32>(0));
	writer.Write(static_cast<uint32>(0));
}

static void WriteTriangles(FMapFileWriter& writer, UStaticMesh* mesh)
{
	for (int32 count = 0; count < 4; ++count)
	{
		writer.Write(static_cast<uint32>(0));
	}
	FTriMeshCollisionData triangles;
	mesh->GetPhysicsTriMeshData(&triangles, false);
	writer.Write(static_cast<uint32>(triangles.Vertices.Num()));
	for (const FVector3f& vertex : triangles.Vertices)
	{
		writer.WriteVector(FVector(vertex));
	}
	writer.Write(static_cast<uint32>(triangles.Indices.Num() * 3));
	for (const FTriIndices& triangle : triangles.Indices)
	{
		writer.Write(static_cast<uint32>(triangle.v0));
		writer.Write(static_cast<uint32>(triangle.v1));
		writer.Write(static_cast<uint32>(triangle.v2));
	}
}

static TOptional<FVector2D> ParseVector2D(const FString& params, const TCHAR* key)
{
	FString value;
	FString x;
	FString y;
	if (!FParse::Value(*params, key, value, false) || !value.Split(TEXT(","), &x, &y))
	{
		return {};
	}
	return FVector2D(FCString::Atod(*x), FCString::Atod(*y));
}

// 풀 맵이 없거나 낡은 지형 조각 수를 세고, 있으면 다시 만든다. 다시 만든 조각 수를 돌려준다.
// 텍스처 준비 전에 만든 지형 렌더 정보는 하위 구획 높이 좌표가 틀리고, 화면 갱신 없이는 다시 만들어지지 않으므로 준비를 끝낸 뒤 다시 만든다.
// 재질 텍스처가 덜 올라오면 풀 무게가 달라지므로 텍스처 스트리밍을 끄고(-dpcvars=r.TextureStreaming=0) 실행해야 한다.
static int32 RebuildOutdatedGrassMaps(UWorld* world)
{
	FAssetCompilingManager::Get().FinishAllCompilation();
	GShaderCompilingManager->FinishAllCompilation();
	int32 outdated = 0;
	for (TActorIterator<ALandscapeProxy> iterator(world); iterator; ++iterator)
	{
		for (ULandscapeComponent* component : iterator->LandscapeComponents)
		{
			const FLandscapeComponentGrassData& grass = component->GrassData.Get();
			outdated += (grass.NumElements < 0 || grass.GenerationHash != component->ComputeGrassMapGenerationHash()) ? 1 : 0;
			component->MarkRenderStateDirty();
		}
	}
	if (outdated == 0)
	{
		return 0;
	}
	world->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	world->GetSubsystem<ULandscapeSubsystem>()->BuildGrassMaps(UE::Landscape::EBuildFlags::WriteFinalLog);
	return outdated;
}

static bool WriteLandscapeFile(const TArray<const ULandscapeHeightfieldCollisionComponent*>& landscapes, const FString& path)
{
	FMapFileWriter writer;
	writer.bytes.Append(reinterpret_cast<const uint8*>("TMLS"), 4);
	writer.Write(LANDSCAPE_FILE_VERSION);
	writer.Write(static_cast<uint32>(landscapes.Num()));
	int32 out_of_bounds = 0;
	for (const ULandscapeHeightfieldCollisionComponent* landscape : landscapes)
	{
		const int32 size = landscape->CollisionSizeQuads;
		const int32 sample_count = (size + 1) * (size + 1);
		const FTransform transform = landscape->GetComponentTransform();
		writer.WriteTransform(transform);
		writer.Write(static_cast<uint32>(size));
		writer.Write(landscape->CollisionScale);
		FWordBulkData& heights_data = const_cast<ULandscapeHeightfieldCollisionComponent*>(landscape)->CollisionHeightData;
		const uint16* heights = static_cast<const uint16*>(heights_data.LockReadOnly());
		check(heights && heights_data.GetElementCount() == sample_count);
		// 높이 범위가 언리얼 부품 경계와 맞는지 본다. 높이 값은 32768이 0이고 1/128 단위다.
		float low = TNumericLimits<float>::Max();
		float high = TNumericLimits<float>::Lowest();
		for (int32 index = 0; index < sample_count; ++index)
		{
			writer.Write(heights[index]);
			const FVector local((index % (size + 1)) * landscape->CollisionScale, (index / (size + 1)) * landscape->CollisionScale, (static_cast<float>(heights[index]) - 32768.0f) / 128.0f);
			const float z = transform.TransformPosition(local).Z;
			low = FMath::Min(low, z);
			high = FMath::Max(high, z);
		}
		heights_data.Unlock();
		const FBox bounds = landscape->Bounds.GetBox();
		out_of_bounds += (low < bounds.Min.Z - 1.0f || high > bounds.Max.Z + 1.0f) ? 1 : 0;
		check(landscape->CollisionQuadFlags.IsEmpty() || landscape->CollisionQuadFlags.Num() == size * size);
		for (int32 index = 0; index < size * size; ++index)
		{
			writer.Write(landscape->CollisionQuadFlags.IsEmpty() ? static_cast<uint8>(0) : landscape->CollisionQuadFlags[index]);
		}
	}
	if (!FFileHelper::SaveArrayToFile(writer.bytes, *path))
	{
		UE_LOG(LogMapExport, Error, TEXT("지형 파일 저장 실패: %s"), *path);
		return false;
	}
	UE_LOG(LogMapExport, Display, TEXT("지형 조각=%d 높이가 부품 경계 밖=%d 크기=%d바이트"), landscapes.Num(), out_of_bounds, writer.bytes.Num());
	return true;
}

static bool IsTriangleCollision(const FMapCollision& collision)
{
	return collision.body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple && collision.static_mesh;
}

static bool WriteCollisionFile(const FMapCollision& collision, const FString& path)
{
	FMapFileWriter writer;
	writer.bytes.Append(reinterpret_cast<const uint8*>("TMCS"), 4);
	writer.Write(COLLISION_FILE_VERSION);
	writer.WriteName(collision.name);
	const bool is_triangles = IsTriangleCollision(collision);
	writer.Write(static_cast<uint8>(is_triangles ? 1 : 0));
	writer.Write(static_cast<uint8>(collision.body->bDoubleSidedGeometry ? 1 : 0));
	if (is_triangles)
	{
		WriteTriangles(writer, collision.static_mesh);
	}
	else
	{
		WriteSimpleShapes(writer, collision.body);
	}
	if (!FFileHelper::SaveArrayToFile(writer.bytes, *path))
	{
		UE_LOG(LogMapExport, Error, TEXT("충돌 모양 파일 저장 실패: %s"), *path);
		return false;
	}
	UE_LOG(LogMapExport, Display, TEXT("충돌 모양 %s 판정=%s 크기=%d바이트"), *collision.name, is_triangles ? TEXT("삼각형") : TEXT("단순 도형"), writer.bytes.Num());
	return true;
}

static bool WritePlacementFile(const FMapEntry& entry, const TArray<FMapCollision>& collisions, const FString& path)
{
	static const TArray<FMapObject> NO_OBJECTS;
	const TArray<FMapObject>& objects = entry.collision != INDEX_NONE ? collisions[entry.collision].objects : NO_OBJECTS;
	FMapFileWriter writer;
	writer.bytes.Append(reinterpret_cast<const uint8*>("TMPL"), 4);
	writer.Write(PLACEMENT_FILE_VERSION);
	writer.WriteName(entry.name);
	writer.Write(static_cast<uint32>(objects.Num()));
	for (const FMapObject& object : objects)
	{
		writer.Write(object.flags);
		writer.WriteTransform(object.transform);
	}
	writer.Write(static_cast<uint32>(entry.decor_locations.Num()));
	for (const FVector& location : entry.decor_locations)
	{
		writer.WriteVector(location);
	}
	if (!FFileHelper::SaveArrayToFile(writer.bytes, *path))
	{
		UE_LOG(LogMapExport, Error, TEXT("배치 파일 저장 실패: %s"), *path);
		return false;
	}
	UE_LOG(LogMapExport, Display, TEXT("배치 %s 충돌 배치=%d 충돌 없는 물체=%d 크기=%d바이트"), *entry.name, objects.Num(), entry.decor_locations.Num(), writer.bytes.Num());
	return true;
}

// 영역 스플라인은 스플라인 부품만 가진 일반 액터이고, 점은 조종점의 월드 x, y(cm)다.
// 영역 파일(출력 폴더/map_regions.txt): 영역 번호 태그(Region번호) 순서로 "보물제외(TreasureExcluded 태그면 1) 점수 x1 y1 x2 y2 ... 이름" 줄을 쓴다.
//   서버는 이 순서를 영역 번호로 클라이언트에 보내므로 번호는 0부터 빠짐없이 이어져야 한다.
// 보물 금지 추가 영역 파일(출력 폴더/treasure_exclusions.txt): TreasureExclusion 태그 스플라인을 이름 순서로 "점수 x1 y1 x2 y2 ... 이름" 줄로 쓴다.
static FString RegionLine(const USplineComponent* spline)
{
	FString line = FString::FromInt(spline->GetNumberOfSplinePoints());
	for (int32 index = 0; index < spline->GetNumberOfSplinePoints(); ++index)
	{
		const FVector point = spline->GetLocationAtSplinePoint(index, ESplineCoordinateSpace::World);
		line += FString::Printf(TEXT(" %.1f %.1f"), point.X, point.Y);
	}
	return line;
}

static bool SaveLines(const TArray<FString>& lines, const FString& path)
{
	return FFileHelper::SaveStringToFile(FString::Join(lines, TEXT("\r\n")) + TEXT("\r\n"), *path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

static bool WriteRegionFiles(UWorld* world, const FString& out)
{
	TMap<int32, FString> regions;
	TArray<TPair<FString, FString>> exclusions;
	bool is_ok = true;
	for (TActorIterator<AActor> iterator(world); iterator; ++iterator)
	{
		const AActor* actor = *iterator;
		const USplineComponent* spline = actor->FindComponentByClass<USplineComponent>();
		if (actor->GetClass() != AActor::StaticClass() || !spline)
		{
			continue;
		}
		const FString label = actor->GetActorLabel();
		if (actor->ActorHasTag(TEXT("TreasureExclusion")))
		{
			exclusions.Add({ label, RegionLine(spline) + TEXT(" ") + label });
			continue;
		}
		int32 region = INDEX_NONE;
		for (const FName& tag : actor->Tags)
		{
			const FString text = tag.ToString();
			if (text.StartsWith(TEXT("Region")) && text.Mid(6).IsNumeric())
			{
				region = FCString::Atoi(*text.Mid(6));
			}
		}
		if (region == INDEX_NONE || regions.Contains(region))
		{
			UE_LOG(LogMapExport, Error, TEXT("영역 번호 태그가 없거나 겹치는 영역 스플라인: %s"), *label);
			is_ok = false;
			continue;
		}
		regions.Add(region, FString::Printf(TEXT("%d "), actor->ActorHasTag(TEXT("TreasureExcluded")) ? 1 : 0) + RegionLine(spline) + TEXT(" ") + label);
	}
	TArray<FString> region_lines = {
		TEXT("# 맵 영역(에디터 스플라인): 보물제외 점수 x1 y1 x2 y2 ... 이름"),
		TEXT("# 보물제외 1이면 최종 보물을 이 영역 안에 놓지 않는다. 점은 스플라인 조종점(cm)이고 순서대로 이어 닫은 다각형이다. 이름은 줄 끝까지(띄어쓰기 포함)."),
	};
	for (int32 region = 0; region < regions.Num(); ++region)
	{
		if (!regions.Contains(region))
		{
			UE_LOG(LogMapExport, Error, TEXT("영역 번호가 비어 있다: Region%d"), region);
			return false;
		}
		region_lines.Add(regions[region]);
	}
	exclusions.Sort([](const TPair<FString, FString>& first, const TPair<FString, FString>& second) { return first.Key < second.Key; });
	TArray<FString> exclusion_lines = {
		TEXT("# 보물을 놓지 않는 추가 영역(에디터 스플라인, TreasureExclusion 태그). 방향 힌트 기준 영역이 아니다."),
		TEXT("# 줄: 점수 x1 y1 x2 y2 ... 이름. 점은 스플라인 조종점(cm)이고 순서대로 이어 닫은 다각형이다."),
	};
	for (const TPair<FString, FString>& exclusion : exclusions)
	{
		exclusion_lines.Add(exclusion.Value);
	}
	if (!is_ok || regions.IsEmpty() || !SaveLines(region_lines, FPaths::Combine(out, TEXT("map_regions.txt"))) || !SaveLines(exclusion_lines, FPaths::Combine(out, TEXT("treasure_exclusions.txt"))))
	{
		UE_LOG(LogMapExport, Error, TEXT("영역 파일을 쓰지 못했다."));
		return false;
	}
	UE_LOG(LogMapExport, Display, TEXT("영역=%d 보물 금지 추가 영역=%d"), regions.Num(), exclusions.Num());
	return true;
}

// 목록 파일: 첫 줄 "LANDSCAPE<TAB>지형 조각 수",
// 항목마다 "ENTRY<TAB>종류 비트<TAB>이름<TAB>판정 방식(-1 충돌 모양 없음, 0 단순 도형, 1 삼각형)<TAB>충돌 배치 수<TAB>충돌 없는 물체 수(지형 풀은 계산 전이라 -1)<TAB>충돌 모양 경로"
static bool WriteListFile(const FMapAssetCollector& collector, const FString& path)
{
	TArray<FString> lines;
	lines.Add(FString::Printf(TEXT("LANDSCAPE	%d"), collector.GetLandscapes().Num()));
	for (const FMapEntry& entry : collector.GetEntries())
	{
		const FMapCollision* collision = entry.collision != INDEX_NONE ? &collector.GetCollisions()[entry.collision] : nullptr;
		const int32 decor_count = (entry.kinds & MAP_ASSET_LANDSCAPE_GRASS) ? -1 : entry.decor_locations.Num();
		lines.Add(FString::Printf(TEXT("ENTRY	%d	%s	%d	%d	%d	%s"), entry.kinds, *entry.name, collision ? (IsTriangleCollision(*collision) ? 1 : 0) : -1,
			collision ? collision->objects.Num() : 0, decor_count, collision ? *collision->path : TEXT("")));
	}
	return FFileHelper::SaveStringArrayToFile(lines, *path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// 이름 목록 파일(한 줄에 이름 하나)을 읽는다. 인자가 없으면 빈 목록이다.
static TOptional<TSet<FString>> LoadNameList(const FString& params, const TCHAR* key)
{
	TSet<FString> names;
	FString list_path;
	if (!FParse::Value(*params, key, list_path))
	{
		return names;
	}
	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *list_path))
	{
		UE_LOG(LogMapExport, Error, TEXT("이름 목록 파일을 읽지 못했다: %s"), *list_path);
		return {};
	}
	for (const FString& line : lines)
	{
		const FString name = line.TrimStartAndEnd();
		if (!name.IsEmpty())
		{
			names.Add(name);
		}
	}
	return names;
}

UMapExportCommandlet::UMapExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMapExportCommandlet::Main(const FString& params)
{
	FString map_path;
	FString mode;
	FString out;
	const TOptional<FVector2D> play_min = ParseVector2D(params, TEXT("playmin="));
	const TOptional<FVector2D> play_max = ParseVector2D(params, TEXT("playmax="));
	if (!FParse::Value(*params, TEXT("map="), map_path) || !FParse::Value(*params, TEXT("mode="), mode) || !FParse::Value(*params, TEXT("out="), out) || !play_min || !play_max
		|| (mode != TEXT("list") && mode != TEXT("export")))
	{
		UE_LOG(LogMapExport, Error, TEXT("인자 오류: -map= -mode=list|export -out= -playmin=x,y -playmax=x,y 가 필요하다."));
		return 1;
	}

	UE_LOG(LogMapExport, Display, TEXT("맵 불러오기: %s"), *map_path);
	UWorld* world = UEditorLoadingAndSavingUtils::LoadMap(map_path);
	if (!world || world->GetWorldPartition())
	{
		UE_LOG(LogMapExport, Error, TEXT("맵을 불러오지 못했거나 월드 파티션 맵이다: %s"), *map_path);
		return 1;
	}

	if (mode == TEXT("export") && FParse::Param(*params, TEXT("rebuildgrass")))
	{
		UE_LOG(LogMapExport, Display, TEXT("낡은 풀 맵 확인"));
		const int32 rebuilt = RebuildOutdatedGrassMaps(world);
		UE_LOG(LogMapExport, Display, TEXT("다시 만든 풀 맵 조각=%d"), rebuilt);
		if (rebuilt > 0 && !UEditorLoadingAndSavingUtils::SaveMap(world, map_path))
		{
			UE_LOG(LogMapExport, Error, TEXT("풀 맵을 다시 만든 맵 저장 실패: %s"), *map_path);
			return 1;
		}
	}

	UE_LOG(LogMapExport, Display, TEXT("배치 모으기"));
	FMapAssetCollector collector(world, FBox2D(*play_min, *play_max));
	UE_LOG(LogMapExport, Display, TEXT("항목=%d 충돌 모양=%d 지형 조각=%d 충돌 모양 없음=%d 서버 생성 액터 제외=%d 흩어진 탑 제외=%d"),
		collector.GetEntries().Num(), collector.GetCollisions().Num(), collector.GetLandscapes().Num(), collector.GetSkippedNoShape(), collector.GetSkippedDynamic(), collector.GetSkippedTowers());

	if (mode == TEXT("list"))
	{
		if (!WriteListFile(collector, out))
		{
			UE_LOG(LogMapExport, Error, TEXT("목록 파일 저장 실패: %s"), *out);
			return 1;
		}
		UE_LOG(LogMapExport, Display, TEXT("목록 저장: %s"), *out);
		return 0;
	}

	TOptional<TSet<FString>> loaded_collision_names = LoadNameList(params, TEXT("collisions="));
	TOptional<TSet<FString>> loaded_placement_names = LoadNameList(params, TEXT("placements="));
	if (!loaded_collision_names || !loaded_placement_names)
	{
		return 1;
	}
	TSet<FString>& collision_names = *loaded_collision_names;
	TSet<FString>& placement_names = *loaded_placement_names;
	collector.AddGrassLocations(world, placement_names);
	if (collector.GetInvalidGrassComponentCount() > 0)
	{
		UE_LOG(LogMapExport, Warning, TEXT("풀 맵이 없는 지형 조각=%d (지형 풀 위치가 빠짐)"), collector.GetInvalidGrassComponentCount());
	}

	bool is_ok = true;
	if (FParse::Param(*params, TEXT("landscape")))
	{
		is_ok &= WriteLandscapeFile(collector.GetLandscapes(), FPaths::Combine(out, TEXT("Map"), TEXT("Landscape.bin")));
	}
	if (FParse::Param(*params, TEXT("regions")))
	{
		is_ok &= WriteRegionFiles(world, out);
	}
	int32 collision_written = 0;
	int32 placement_written = 0;
	for (const FMapEntry& entry : collector.GetEntries())
	{
		const bool should_write_collision = collision_names.Remove(entry.name) > 0;
		const bool should_write_placement = placement_names.Remove(entry.name) > 0;
		if (!should_write_collision && !should_write_placement)
		{
			continue;
		}
		const FString entry_dir = FPaths::Combine(out, TEXT("Map"), TEXT("Meshes"), entry.name);
		IFileManager::Get().MakeDirectory(*entry_dir, true);
		if (should_write_collision && entry.collision == INDEX_NONE)
		{
			UE_LOG(LogMapExport, Warning, TEXT("충돌 모양이 없는 항목: %s"), *entry.name);
		}
		else if (should_write_collision)
		{
			const bool is_written = WriteCollisionFile(collector.GetCollisions()[entry.collision], FPaths::Combine(entry_dir, TEXT("collision.bin")));
			collision_written += is_written ? 1 : 0;
			is_ok &= is_written;
		}
		if (should_write_placement)
		{
			const bool is_written = WritePlacementFile(entry, collector.GetCollisions(), FPaths::Combine(entry_dir, TEXT("placements.bin")));
			placement_written += is_written ? 1 : 0;
			is_ok &= is_written;
		}
	}
	for (const FString& missing : collision_names.Union(placement_names))
	{
		UE_LOG(LogMapExport, Warning, TEXT("맵에 없는 항목: %s"), *missing);
	}
	UE_LOG(LogMapExport, Display, TEXT("추출 끝: 충돌 모양 파일=%d 배치 파일=%d 맵에 없는 이름=%d"), collision_written, placement_written, collision_names.Union(placement_names).Num());
	return is_ok ? 0 : 1;
}
