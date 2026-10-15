#pragma once

#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"

// 게임 화면 글자에 쓰는 글꼴(Black Han Sans). 굵기는 하나뿐이라 크기만 정한다. 이 글꼴에 없는 글자는 엔진 기본 글꼴의 대체 글꼴로 그린다.
inline FSlateFontInfo GetGameFont(int32 size)
{
	return FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Game/Thats_Mine/UI/Shared/Fonts/F_BlackHanSans.F_BlackHanSans")), size);
}
