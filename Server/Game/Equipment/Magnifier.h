#pragma once
#include "Equipment.h"

// 돋보기. 서버가 따로 들 상태가 없어 장비 공통 동작만 쓴다.
class Magnifier : public Equipment
{
public:
	explicit Magnifier(const EquipmentSettings& settings);

	void Reset() override;
};
