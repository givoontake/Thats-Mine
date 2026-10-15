#pragma once
#include <cstdint>

// 힌트 물체 목록 파일. 서버 힌트 물체 트리와 같은 점(바위·나무·대나무·색 데이지·색 버섯의 바닥 위치)을 담아, 클라이언트 돋보기가 주변 개수를 서버 힌트와 같게 센다.
// 서버를 --export-hint-objects <파일>로 실행해 만든다. 앞의 uint32 개수 뒤에 HintObjectRecord가 개수만큼 이어진다.
#pragma pack(push, 1)
struct HintObjectRecord {
	float location[3]; // 물체 바닥 위치(cm)
	std::uint8_t kind; // HintObjectKind 값
};
#pragma pack(pop)
