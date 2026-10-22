#pragma once
#include "CoreMinimal.h"

// 서버 맵 파일의 이름 칸 크기(UTF-8, 남는 칸은 0)
constexpr int32 MAP_NAME_SIZE = 64;

// 서버 맵 파일 형식으로 값을 차례로 붙여 쓰는 버퍼
struct FMapFileWriter
{
	TArray<uint8> bytes;

	template <typename T>
	void Write(const T& value)
	{
		bytes.Append(reinterpret_cast<const uint8*>(&value), sizeof(T));
	}
	void WriteName(const FString& name)
	{
		char buffer[MAP_NAME_SIZE] = {};
		const FTCHARToUTF8 utf8(*name);
		FMemory::Memcpy(buffer, utf8.Get(), FMath::Min(utf8.Length(), MAP_NAME_SIZE - 1));
		bytes.Append(reinterpret_cast<const uint8*>(buffer), MAP_NAME_SIZE);
	}
	void WriteVector(const FVector& value)
	{
		Write(static_cast<float>(value.X));
		Write(static_cast<float>(value.Y));
		Write(static_cast<float>(value.Z));
	}
	void WriteQuat(const FQuat& value)
	{
		Write(static_cast<float>(value.X));
		Write(static_cast<float>(value.Y));
		Write(static_cast<float>(value.Z));
		Write(static_cast<float>(value.W));
	}
	void WriteTransform(const FTransform& value)
	{
		WriteVector(value.GetLocation());
		WriteQuat(value.GetRotation());
		WriteVector(value.GetScale3D());
	}
};
