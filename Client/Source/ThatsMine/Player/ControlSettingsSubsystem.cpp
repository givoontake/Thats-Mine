#include "ControlSettingsSubsystem.h"

void UControlSettingsSubsystem::SetMouseSensitivity(float sensitivity)
{
	mouse_sensitivity_ = FMath::Clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY);
}

void UControlSettingsSubsystem::SetZoomSensitivity(float sensitivity)
{
	zoom_sensitivity_ = FMath::Clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY);
}
