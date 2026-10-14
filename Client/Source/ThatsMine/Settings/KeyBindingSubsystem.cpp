#include "KeyBindingSubsystem.h"

void UKeyBindingSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	// 저장된 키가 없거나 동작 수가 바뀌었으면 빠진 동작을 기본 키로 채운다.
	const int32 saved_count = keys_.Num();
	keys_.SetNum(static_cast<int32>(EKeyAction::COUNT));
	for (int32 action = 0; action < keys_.Num(); ++action)
	{
		if (action >= saved_count || !keys_[action].IsValid())
		{
			keys_[action] = GetDefaultKey(static_cast<EKeyAction>(action));
		}
	}
}

FKey UKeyBindingSubsystem::GetDefaultKey(EKeyAction action)
{
	switch (action)
	{
	case EKeyAction::MOVE_FORWARD: return EKeys::W;
	case EKeyAction::MOVE_BACKWARD: return EKeys::S;
	case EKeyAction::MOVE_LEFT: return EKeys::A;
	case EKeyAction::MOVE_RIGHT: return EKeys::D;
	case EKeyAction::JUMP: return EKeys::SpaceBar;
	case EKeyAction::INTERACT: return EKeys::F;
	case EKeyAction::PICKUP: return EKeys::E;
	case EKeyAction::RELOAD: return EKeys::R;
	case EKeyAction::WEAPON_SNIPER: return EKeys::One;
	case EKeyAction::WEAPON_PISTOL: return EKeys::Two;
	case EKeyAction::WEAPON_PICKAXE: return EKeys::Three;
	case EKeyAction::WEAPON_MAGNIFIER: return EKeys::Four;
	case EKeyAction::INVENTORY: return EKeys::Tab;
	case EKeyAction::MAP: return EKeys::M;
	case EKeyAction::HELP: return EKeys::F1;
	case EKeyAction::PING: return EKeys::MiddleMouseButton;
	default: return EKeys::Invalid;
	}
}

FText UKeyBindingSubsystem::GetActionName(EKeyAction action)
{
	switch (action)
	{
	case EKeyAction::MOVE_FORWARD: return NSLOCTEXT("KeyBinding", "MoveForward", "앞으로 이동");
	case EKeyAction::MOVE_BACKWARD: return NSLOCTEXT("KeyBinding", "MoveBackward", "뒤로 이동");
	case EKeyAction::MOVE_LEFT: return NSLOCTEXT("KeyBinding", "MoveLeft", "왼쪽 이동");
	case EKeyAction::MOVE_RIGHT: return NSLOCTEXT("KeyBinding", "MoveRight", "오른쪽 이동");
	case EKeyAction::JUMP: return NSLOCTEXT("KeyBinding", "Jump", "점프");
	case EKeyAction::INTERACT: return NSLOCTEXT("KeyBinding", "Interact", "상호작용");
	case EKeyAction::PICKUP: return NSLOCTEXT("KeyBinding", "Pickup", "줍기");
	case EKeyAction::RELOAD: return NSLOCTEXT("KeyBinding", "Reload", "재장전");
	case EKeyAction::WEAPON_SNIPER: return NSLOCTEXT("KeyBinding", "WeaponSniper", "저격총");
	case EKeyAction::WEAPON_PISTOL: return NSLOCTEXT("KeyBinding", "WeaponPistol", "권총");
	case EKeyAction::WEAPON_PICKAXE: return NSLOCTEXT("KeyBinding", "WeaponPickaxe", "곡괭이");
	case EKeyAction::WEAPON_MAGNIFIER: return NSLOCTEXT("KeyBinding", "WeaponMagnifier", "돋보기");
	case EKeyAction::INVENTORY: return NSLOCTEXT("KeyBinding", "Inventory", "인벤토리");
	case EKeyAction::MAP: return NSLOCTEXT("KeyBinding", "Map", "지도");
	case EKeyAction::HELP: return NSLOCTEXT("KeyBinding", "Help", "도움말");
	case EKeyAction::PING: return NSLOCTEXT("KeyBinding", "Ping", "핑 찍기");
	default: return FText::GetEmpty();
	}
}

void UKeyBindingSubsystem::SetKeys(const TArray<FKey>& keys)
{
	if (keys.Num() != keys_.Num())
	{
		return;
	}
	keys_ = keys;
	SaveConfig();
	on_keys_changed_.Broadcast();
}
