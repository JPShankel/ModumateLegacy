// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Materials/Material.h"

#include "ModumateArchitecturalMesh.generated.h"

/*
Door handles, window frames and such
*/
USTRUCT()
struct FArchitecturalMesh
{
	GENERATED_BODY()

	FName Key = FName();

	FSoftObjectPath AssetPath;
	TWeakObjectPtr<UStaticMesh> EngineMesh = nullptr;

	FVector NativeSize = FVector::ZeroVector;
	FBox NineSliceBox = FBox(ForceInit);

	FName UniqueKey() const { return Key; }
};