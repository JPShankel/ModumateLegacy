#pragma once

#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMSlot.generated.h"


USTRUCT()
struct FBIMSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FString ID;

	UPROPERTY()
	FString SupportedNCPs;

	UPROPERTY()
	FString RequiredInputParamaters;

	UPROPERTY()
	FString LocationX;

	UPROPERTY()
	FString LocationY;

	UPROPERTY()
	FString LocationZ;

	UPROPERTY()
	FString SizeX;

	UPROPERTY()
	FString SizeY;

	UPROPERTY()
	FString SizeZ;

	UPROPERTY()
	FString RotationX;

	UPROPERTY()
	FString RotationY;

	UPROPERTY()
	FString RotationZ;

	UPROPERTY()
	bool FlipX;

	UPROPERTY()
	bool FlipY;

	UPROPERTY()
	bool FlipZ;
};