#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "block_pieces.h"
#include "BlockSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnBlocksChanged);

// 내가 연 상자의 블록을 가지고, 블록을 배제 판에 놓겠다고 서버에 보내 결과를 받는다. 같은 블록을 여러 개 가질 수 있고, 하나마다 그 게임 동안 한 번 쓸 수 있다.
// 배제 판은 힌트 서브시스템이 받고, 결과 안내는 힌트 서브시스템의 화면 위쪽 안내로 띄운다. 온라인 게임 레벨에서만 동작한다.
UCLASS()
class THATSMINE_API UBlockSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 가졌고 아직 쓰지 않은 블록의 BLOCK_PIECES 번호. 번호 순서이고 같은 블록은 한 번만 들어간다.
	TArray<int32> GetUsableBlocks() const;
	// 가졌고 아직 쓰지 않은 그 블록의 수
	int32 GetBlockCount(int32 piece) const { return block_counts_[piece]; }
	// 블록을 시계 방향으로 rotation번 돌려 가장 작은 열과 행이 (column, row) 칸에 오게 놓겠다고 서버에 보낸다.
	void RequestUse(int32 piece, int32 rotation, int32 column, int32 row) const;

	FOnBlocksChanged on_blocks_changed_;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandleBlockPacket(const TArray<uint8>& payload);
	void HandleBlockResultPacket(const TArray<uint8>& payload);

	// 가졌고 아직 쓰지 않은 블록 수. BLOCK_PIECES 번호 순서다.
	uint8 block_counts_[BLOCK_PIECE_COUNT] = {};
	bool is_active_ = false;
};
