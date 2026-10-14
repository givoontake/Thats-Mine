#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Network/game_protocol.h"
#include "HintSubsystem.generated.h"

// 돋보기 탐색 결과 하나. 보물을 찾았으면 원 밖, 못 찾았으면 원 안이 배제된다.
struct FSearchExclusion
{
	FVector2D center = FVector2D::ZeroVector;
	float radius = 0.0f;
	bool is_found = false;
};

class UHintNoticeWidget;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FOnExclusionMaskChanged);

// 서버가 보낸 힌트를 문장으로 만들어 내 캐릭터의 인벤토리에 넣는다.
// 문장 틀과 이름은 Content/Data/hint_texts.txt에서 읽고, 힌트 항목의 번호로 빈칸을 채운다. 온라인 게임 레벨에서만 동작한다.
// 배제 영역을 지도 범위(MAP_MIN~MAP_MAX)를 나눈 칸마다 배제됐는지로 모아 둔다. 어떤 배제든 칸을 "배제됨"으로 칠할 뿐이라 여러 배제가 겹쳐도 한 번만 배제된다.
// 방향 힌트는 문장으로만 알려 주고 배제하지 않는다. 기절해서 힌트를 잃으면 인벤토리에서 뺀다.
// 돋보기 탐색 결과를 배제 영역으로 칠하고 화면 위쪽 가운데에 결과 안내를 띄운다. 탐색 결과는 기절해도 잃지 않는다.
// 서버가 보낸 배제 판(block_pieces.h의 8×8칸)의 배제된 칸도 칠한다.
// 서버가 다음 영역 지우기까지 남은 시간을 보내면 정해 둔 남은 시간마다, 칸을 지우면 그때 같은 자리에 안내를 띄운다.
UCLASS()
class THATSMINE_API UHintSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 배제 칸 수. 지도 범위를 가로·세로로 이만큼 나눈다.
	static constexpr int32 EXCLUSION_MASK_SIZE = 512;

	// 칸마다 배제됐으면 1이다. 지도 이미지와 같은 방향으로 행은 위(x가 작은 쪽)부터, 열은 왼쪽(y가 큰 쪽)부터다. 배제를 받은 적 없으면 비어 있다.
	const TArray<uint8>& GetExclusionMask() const { return exclusion_mask_; }
	// 배제 칸을 그린 텍스처. 배제 칸은 알파 255, 나머지는 0이라 이미지 색의 불투명도와 곱해져 배제된 곳이 한 번만 어두워진다.
	// 배제 칸이 바뀔 때마다 새로 만들므로 on_exclusion_mask_changed_를 받으면 다시 가져간다. 배제를 받은 적 없으면 비어 있다.
	UTexture2D* GetExclusionTexture() const { return exclusion_texture_; }
	// 돋보기 탐색 결과를 배제 영역으로 칠하고 결과 안내를 띄운다. center는 탐색한 위치(cm, x·y), radius는 탐색 반경(cm)이다.
	void AddSearchResult(const FVector2D& center, float radius, bool is_found);
	// 서버가 보낸 배제 판. 칸 번호 자리의 비트가 켜진 칸이 배제된 칸이다.
	uint64 GetExclusionBoard() const { return exclusion_board_; }
	// 화면 위쪽 가운데에 안내를 띄운다. 안내 위젯이 없으면 먼저 만든다.
	void ShowNotice(const FText& text);

	FOnExclusionMaskChanged on_exclusion_mask_changed_;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void LoadTexts();
	// 키의 문장. 파일에 없으면 키를 그대로 돌려준다.
	FString FindText(const FString& key) const;
	FString BuildSentence(const HINT_ENTRY& hint) const;
	void HandleHintPacket(const TArray<uint8>& payload);
	void HandleHintLostPacket(const TArray<uint8>& payload);
	void HandleExclusionBoardPacket(const TArray<uint8>& payload);
	void HandleAreaRemovalTimePacket(const TArray<uint8>& payload);
	void HandleAreaRemovalPacket(const TArray<uint8>& payload);
	// 영역 지우기까지 seconds초 남았다는 안내를 띄운다.
	void ShowAreaRemovalNotice(int32 seconds);
	// 탐색 결과 하나를 배제 칸에 칠한다. 칸이 없으면 먼저 만든다.
	void PaintSearchExclusion(const FSearchExclusion& search);
	void PaintExclusionBoard();
	// 배제 칸을 비우고 탐색 결과와 배제 판으로 다시 칠한다.
	void RepaintExclusionMask();
	// 배제 칸으로 텍스처를 다시 만들고 배제가 바뀌었다고 알린다.
	void NotifyExclusionChanged();

	// 문장 파일의 키와 문장
	TMap<FString, FString> texts_;
	TArray<uint8> exclusion_mask_;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> exclusion_texture_;
	TArray<FSearchExclusion> search_exclusions_;
	uint64 exclusion_board_ = 0;
	// 남은 시간 안내마다 하나씩 건 타이머. 새 남은 시간을 받으면 모두 다시 건다.
	TArray<FTimerHandle> notice_timers_;
	UPROPERTY(Transient)
	TObjectPtr<UHintNoticeWidget> notice_widget_;
	bool is_active_ = false;
};
