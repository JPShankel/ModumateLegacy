// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Materials/Material.h"
#include "ModumateCore/ModumateUnits.h"
#include "BIMKernel/BIMKey.h"

#include "ModumateArchitecturalMesh.generated.h"

/*
Door handles, window frames and such
*/
USTRUCT()
struct FArchitecturalMesh
{
	GENERATED_BODY()

	FBIMKey Key;

	FSoftObjectPath AssetPath;
	TWeakObjectPtr<UStaticMesh> EngineMesh = nullptr;

	FVector NativeSize = FVector::ZeroVector;
	FBox NineSliceBox = FBox(ForceInit);

	FBIMKey UniqueKey() const { return Key; }

	TMap<FString, Modumate::Units::FUnitValue> NamedDimensions;

	void ReadNamedDimensions(const FString& InNamedDimensions);
};