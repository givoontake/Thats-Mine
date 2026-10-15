#pragma once
#include <array>
#include <DirectXMath.h>
#include "block_pieces.h"

class MapData;

// 돋보기 탐색 결과로 칸을 배제할 때 쓰는 배제 판 칸마다의 땅 높이 범위.
// 서버 시작 때 만든 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class HintExclusionData
{
	// [칸 번호] 칸 안 땅 높이의 최솟값과 최댓값(cm). MAGNIFIER_HEIGHT_SAMPLE_STEP 간격으로 잰다.
	std::array<DirectX::XMFLOAT2, BLOCK_GRID_SIZE * BLOCK_GRID_SIZE> cell_height_ranges_{};
	bool is_loaded_ = false;

public:
	bool Build(const MapData& map);
	bool IsLoaded() const { return is_loaded_; }
	// 칸 안 땅 높이의 최솟값(x)과 최댓값(y)(cm)
	const DirectX::XMFLOAT2& GetCellHeightRange(int cell) const { return cell_height_ranges_[cell]; }
};
