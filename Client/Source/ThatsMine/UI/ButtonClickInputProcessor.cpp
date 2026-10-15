#include "ButtonClickInputProcessor.h"

FButtonClickInputProcessor::FButtonClickInputProcessor(TFunction<void(const FPointerEvent&)> on_mouse_button_down)
	: on_mouse_button_down_(MoveTemp(on_mouse_button_down))
{
}

bool FButtonClickInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& slate_app, const FPointerEvent& event)
{
	on_mouse_button_down_(event);
	return false;
}
