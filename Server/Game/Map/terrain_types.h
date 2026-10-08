#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>

// 배치물이 막는 채널. 추출 파일의 배치 플래그와 같은 값이다.
constexpr std::uint8_t TERRAIN_BLOCKS_VISIBILITY = 1; // 총알(보이는 채널)을 막는다.
constexpr std::uint8_t TERRAIN_BLOCKS_PAWN = 2; // 캐릭터 이동(폰 채널)을 막는다.

// 메시 기준 상자. 회전은 쿼터니언(x, y, z, w)이다.
struct TerrainBox {
	DirectX::XMFLOAT3 center;
	DirectX::XMFLOAT4 rotation;
	DirectX::XMFLOAT3 half_size;
};

// 메시 기준 구·캡슐. 배치의 크기 배율은 언리얼 규칙으로 따로 적용하므로, 배치마다 월드 캡슐로 바꿔 둔다.
struct TerrainSphere {
	DirectX::XMFLOAT3 center;
	float radius;
};

struct TerrainCapsule {
	DirectX::XMFLOAT3 center;
	DirectX::XMFLOAT4 rotation;
	float radius;
	float length; // 양 끝 반구를 뺀 원기둥 길이
};

// 볼록 도형의 면. 바깥쪽 법선 normal과 원점에서의 거리 distance로, normal·점 ≤ distance 쪽이 안쪽이다.
struct TerrainPlane {
	DirectX::XMFLOAT3 normal;
	float distance;
};

struct TerrainConvex {
	std::vector<TerrainPlane> planes;
	std::vector<DirectX::XMFLOAT3> vertices; // 메시 기준 꼭짓점. 캡슐 겹침 검사에 쓴다.
};

// 삼각형 상자 나무의 노드. count가 0이면 안쪽 노드로 왼쪽 자식은 바로 다음 칸, 오른쪽 자식은 first 칸이다.
// count가 0보다 크면 끝 노드로 first번째 삼각형부터 count개를 가진다.
struct TerrainBvhNode {
	DirectX::XMFLOAT3 min;
	DirectX::XMFLOAT3 max;
	std::uint32_t first;
	std::uint32_t count;
};

// 충돌 모양 원본. 정적 메시는 메시마다 하나이고, 같은 메시를 쓰는 배치가 공유한다. 좌표는 메시 기준(cm)이다.
// 삼각형으로 판정하는 메시는 triangles만, 단순 도형으로 판정하는 메시는 나머지를 쓴다.
struct TerrainSource {
	std::string name; // 메시 이름(브러시는 액터 이름)
	std::vector<TerrainSphere> spheres;
	std::vector<TerrainBox> boxes;
	std::vector<TerrainCapsule> capsules;
	std::vector<TerrainConvex> convexes;
	std::vector<DirectX::XMFLOAT3> vertices;
	std::vector<std::uint32_t> triangles; // 삼각형마다 꼭짓점 번호 3개. 상자 나무의 끝 노드 순서로 정렬해 둔다.
	std::vector<TerrainBvhNode> nodes;
	DirectX::XMFLOAT3 local_min; // 상자·볼록 도형·삼각형을 감싸는 메시 기준 상자
	DirectX::XMFLOAT3 local_max;
	bool has_local_shapes = false;
	// 양면이 아니면 삼각형은 꼭짓점 순서(a→b→c)의 법선 방향으로 들어오는 선분만 막는다. 언리얼 삼각형 충돌과 같다.
	bool is_double_sided = false;
	// 이 원본을 쓰는 맵 배치는 배치 목록의 first_object번부터 object_count개다. 맵 배치는 원본 순서로 줄 세워 둔다.
	std::uint32_t first_object = 0;
	std::uint32_t object_count = 0;
};

// 월드 좌표 캡슐. 구는 양 끝이 같은 캡슐로 둔다.
struct TerrainWorldCapsule {
	DirectX::XMFLOAT3 start;
	DirectX::XMFLOAT3 end;
	float radius;
};

// 배치 하나. 메시 기준 ↔ 월드 변환과 월드 경계 상자를 미리 계산해 둔다.
struct TerrainObject {
	DirectX::XMFLOAT4X4 local_to_world;
	DirectX::XMFLOAT4X4 world_to_local;
	DirectX::XMFLOAT3 world_min;
	DirectX::XMFLOAT3 world_max;
	std::uint32_t source;
	std::uint32_t first_capsule;
	std::uint32_t capsule_count;
	std::uint8_t flags;
};

// 랜드스케이프 충돌 조각 하나. 꼭짓점 (i, j)의 조각 기준 위치는 (i·칸 배율, j·칸 배율, (높이 − 32768) / 128)이다.
// 칸 (i, j)는 (i, j)–(i+1, j+1) 대각선으로 나눈 삼각형 2개로, 언리얼과 같다.
struct TerrainHeightfield {
	DirectX::XMFLOAT4X4 local_to_world;
	DirectX::XMFLOAT4X4 world_to_local;
	DirectX::XMFLOAT3 world_min;
	DirectX::XMFLOAT3 world_max;
	std::uint32_t size; // 한 변의 칸 수
	float collision_scale;
	std::vector<std::uint16_t> heights; // (size + 1)²개, j 행 우선
	std::vector<std::uint8_t> quad_flags; // size²개
};
