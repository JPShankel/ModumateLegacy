// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"

class FBIMAssemblySpec;

// Used by MOIs to calculate the layout of parts on a host plane
class MODUMATE_API FBIMPartLayout
{
private:
	bool TryGetValueForPart(const FBIMAssemblySpec& InAssemblySpec, int32 InPartIndex, const FString& InVar, float& OutVal) const;

public:
	struct FPartSlotInstance
	{
		FVector FlipVector = FVector::OneVector;
		TMap<FString, float> VariableValues;
		FVector Location, Rotation, Size;
	};

	TArray<FPartSlotInstance> PartSlotInstances;
	EBIMResult FromAssembly(const FBIMAssemblySpec& InAssemblySpec, const FVector& InScale);

	float CabinetPanelAssemblyConceptualSizeY = 0.0f;

	static const FString ScaledSizeX;
	static const FString ScaledSizeY;
	static const FString ScaledSizeZ;

	static const FString NativeSizeX;
	static const FString NativeSizeY;
	static const FString NativeSizeZ;

	static const FString LocationX;
	static const FString LocationY;
	static const FString LocationZ;

	static const FString RotationX;
	static const FString RotationY;
	static const FString RotationZ;

	static const FString Self;
	static const FString Parent;
};