// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Materials/Material.h"
#include "ModumateCore/ModumateUnits.h"
#include "BIMKernel/Core/BIMKey.h"

#include "ModumateArchitecturalMesh.generated.h"

/*
Door handles, window frames and such
*/
USTRUCT()
struct FArchitecturalMesh
{
	GENERATED_BODY()

	FGuid Key;

	FSoftObjectPath AssetPath;
	TWeakObjectPtr<UStaticMesh> EngineMesh = nullptr;

	FVector NativeSize = FVector::ZeroVector;
	FBox NineSliceBox = FBox(ForceInit);

	FGuid UniqueKey() const { return Key; }

	TMap<FString, Modumate::Units::FUnitValue> NamedDimensions;

	void ReadNamedDimensions(const FString& InNamedDimensions);
};