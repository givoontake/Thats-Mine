#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

// 마우스 누름을 Slate 위젯보다 먼저 받아 넘겨준다. 누름은 소비하지 않아 위젯도 그대로 받는다.
class FButtonClickInputProcessor : public IInputProcessor
{
public:
	explicit FButtonClickInputProcessor(TFunction<void(const FPointerEvent&)> on_mouse_button_down);
	virtual void Tick(const float delta_time, FSlateApplication& slate_app, TSharedRef<ICursor> cursor) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& slate_app, const FPointerEvent& event) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("ButtonClick"); }

private:
	TFunction<void(const FPointerEvent&)> on_mouse_button_down_;
};
