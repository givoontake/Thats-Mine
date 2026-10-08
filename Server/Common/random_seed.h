#pragma once
#include <cstdint>
#include <random>

// 운영체제 난수로 새 시드를 만든다. 판마다 정하는 배치(발판·탑·상자·보물)는 모두 부를 때마다 이 시드로 난수를 시작한다.
inline std::uint32_t CreateRandomSeed()
{
	return std::random_device{}();
}
