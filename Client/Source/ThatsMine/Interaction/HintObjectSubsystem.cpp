#include "HintObjectSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogHintObject, Log, All);

// 힌트 물체 목록 파일. 서버를 --export-hint-objects로 실행해 만들고, 패키징할 때 Content/Data 폴더를 함께 넣는다.
static const TCHAR* HINT_OBJECT_PATH = TEXT("Data/hint_objects.bin");

void UHintObjectSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	TArray<uint8> data;
	uint32 count = 0;
	if (!FFileHelper::LoadFileToArray(data, *(FPaths::ProjectContentDir() / HINT_OBJECT_PATH)) || data.Num() < sizeof(count))
	{
		UE_LOG(LogHintObject, Error, TEXT("Hint object file not found."));
		return;
	}
	FMemory::Memcpy(&count, data.GetData(), sizeof(count));
	if (data.Num() != sizeof(count) + count * sizeof(HintObjectRecord))
	{
		UE_LOG(LogHintObject, Error, TEXT("Invalid hint object file."));
		return;
	}
	const HintObjectRecord* records = reinterpret_cast<const HintObjectRecord*>(data.GetData() + sizeof(count));
	for (uint32 index = 0; index < count; ++index)
	{
		const HintObjectRecord& record = records[index];
		if (record.kind < static_cast<uint8>(HintObjectKind::COUNT))
		{
			cells_.FindOrAdd(FIntPoint(FMath::FloorToInt32(record.location[0] / CELL_SIZE), FMath::FloorToInt32(record.location[1] / CELL_SIZE))).Add(record);
		}
	}
}

TArray<int32> UHintObjectSubsystem::CountKinds(const FVector& location, float radius) const
{
	TArray<int32> counts;
	counts.Init(0, static_cast<int32>(HintObjectKind::COUNT));
	const FIntPoint center(FMath::FloorToInt32(location.X / CELL_SIZE), FMath::FloorToInt32(location.Y / CELL_SIZE));
	const double radius_sq = FMath::Square(static_cast<double>(radius));
	for (int32 x = center.X - 1; x <= center.X + 1; ++x)
	{
		for (int32 y = center.Y - 1; y <= center.Y + 1; ++y)
		{
			const TArray<HintObjectRecord>* records = cells_.Find(FIntPoint(x, y));
			if (!records)
			{
				continue;
			}
			for (const HintObjectRecord& record : *records)
			{
				if (FVector::DistSquared(location, FVector(record.location[0], record.location[1], record.location[2])) <= radius_sq)
				{
					++counts[record.kind];
				}
			}
		}
	}
	// 색 데이지·버섯은 색을 가리지 않은 종류에도 더한다.
	for (int32 kind = static_cast<int32>(HintObjectKind::DAISY_RED); kind <= static_cast<int32>(HintObjectKind::DAISY_WHITE); ++kind)
	{
		counts[static_cast<int32>(HintObjectKind::DAISY)] += counts[kind];
	}
	for (int32 kind = static_cast<int32>(HintObjectKind::MUSHROOM_RED); kind <= static_cast<int32>(HintObjectKind::MUSHROOM_BLUE); ++kind)
	{
		counts[static_cast<int32>(HintObjectKind::MUSHROOM)] += counts[kind];
	}
	return counts;
}
