#pragma once
#include <cmath>
#include <cstdint>
#include <iterator>
#include "settings.h"

// 블록 배제 판. 지도 범위(MAP_MIN~MAP_MAX)를 가로·세로 BLOCK_GRID_SIZE칸으로 나눈다.
// 지도 이미지와 같은 방향으로 행은 위(x가 작은 쪽)부터, 열은 왼쪽(y가 큰 쪽)부터이고, 칸 번호는 행 × BLOCK_GRID_SIZE + 열이다.
// 칸 모음은 칸 번호 자리의 비트를 켠 64비트 값이다.
constexpr int BLOCK_GRID_SIZE = 8;
constexpr int BLOCK_MAX_CELL_COUNT = 5;
constexpr int BLOCK_ROTATION_COUNT = 4;
constexpr float BLOCK_CELL_SIZE_X = (MAP_MAX[0] - MAP_MIN[0]) / BLOCK_GRID_SIZE; // 한 칸의 x 길이(cm)
constexpr float BLOCK_CELL_SIZE_Y = (MAP_MAX[1] - MAP_MIN[1]) / BLOCK_GRID_SIZE; // 한 칸의 y 길이(cm)

// 블록 하나의 모양. 칸은 (열, 행)이고 가장 작은 열과 행이 0이다.
struct BlockPiece {
	std::uint8_t cell_count;
	std::uint8_t cells[BLOCK_MAX_CELL_COUNT][2];
};

// 블로커스 블록 21개. 블록 번호는 이 표의 순서다.
constexpr BlockPiece BLOCK_PIECES[] = {
	{ 1, { { 0, 0 } } },
	{ 2, { { 0, 0 }, { 1, 0 } } },
	{ 3, { { 0, 0 }, { 1, 0 }, { 2, 0 } } },
	{ 3, { { 0, 0 }, { 0, 1 }, { 1, 1 } } },
	{ 4, { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } } },
	{ 4, { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } } },
	{ 4, { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 1, 1 } } },
	{ 4, { { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 } } },
	{ 4, { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 2 } } },
	{ 5, { { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 } } },
	{ 5, { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 } } },
	{ 5, { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 3 } } },
	{ 5, { { 1, 0 }, { 1, 1 }, { 0, 2 }, { 1, 2 }, { 0, 3 } } },
	{ 5, { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }, { 0, 2 } } },
	{ 5, { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 1, 1 }, { 1, 2 } } },
	{ 5, { { 0, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } } },
	{ 5, { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 } } },
	{ 5, { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 }, { 2, 2 } } },
	{ 5, { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 }, { 1, 2 } } },
	{ 5, { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 }, { 1, 3 } } },
	{ 5, { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 1, 2 }, { 2, 2 } } },
};
constexpr int BLOCK_PIECE_COUNT = static_cast<int>(std::size(BLOCK_PIECES));

// 지도 이미지에서 시계 방향으로 90도씩 rotation번 돌린 모양. 가장 작은 열과 행이 다시 0이 되게 옮긴다.
constexpr BlockPiece RotateBlockPiece(const BlockPiece& piece, int rotation)
{
	BlockPiece rotated = piece;
	for (int turn = 0; turn < rotation % BLOCK_ROTATION_COUNT; ++turn) {
		int max_row = 0;
		for (int cell = 0; cell < rotated.cell_count; ++cell) max_row = rotated.cells[cell][1] > max_row ? rotated.cells[cell][1] : max_row;
		for (int cell = 0; cell < rotated.cell_count; ++cell) {
			const std::uint8_t column = rotated.cells[cell][0];
			rotated.cells[cell][0] = static_cast<std::uint8_t>(max_row - rotated.cells[cell][1]);
			rotated.cells[cell][1] = column;
		}
	}
	return rotated;
}

// 블록을 돌려 가장 작은 열과 행이 (column, row) 칸에 오게 놓았을 때 덮는 칸 모음. 판 밖으로 나가는 칸이 있으면 0이다.
constexpr std::uint64_t GetBlockCells(int piece, int rotation, int column, int row)
{
	const BlockPiece rotated = RotateBlockPiece(BLOCK_PIECES[piece], rotation);
	std::uint64_t cells = 0;
	for (int cell = 0; cell < rotated.cell_count; ++cell) {
		const int cell_column = column + rotated.cells[cell][0];
		const int cell_row = row + rotated.cells[cell][1];
		if (cell_column < 0 || cell_column >= BLOCK_GRID_SIZE || cell_row < 0 || cell_row >= BLOCK_GRID_SIZE) return 0;
		cells |= 1ULL << (cell_row * BLOCK_GRID_SIZE + cell_column);
	}
	return cells;
}

// 위치(cm, x·y)가 속한 칸 번호. 맵 밖이면 가장 가까운 가장자리 칸이다.
inline int GetBlockCellIndex(float x, float y)
{
	const int row = static_cast<int>(std::floor((x - MAP_MIN[0]) / BLOCK_CELL_SIZE_X));
	const int column = static_cast<int>(std::floor((MAP_MAX[1] - y) / BLOCK_CELL_SIZE_Y));
	const auto clamp = [](int value) { return value < 0 ? 0 : value >= BLOCK_GRID_SIZE ? BLOCK_GRID_SIZE - 1 : value; };
	return clamp(row) * BLOCK_GRID_SIZE + clamp(column);
}
