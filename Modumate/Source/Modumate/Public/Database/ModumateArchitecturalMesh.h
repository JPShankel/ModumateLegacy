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

	UPROPERTY()
	FGuid Key;

	UPROPERTY()
	FSoftObjectPath AssetPath;
	
	TWeakObjectPtr<UStaticMesh> EngineMesh = nullptr;

	UPROPERTY()
	FVector NativeSize = FVector::ZeroVector;
	
	UPROPERTY()
	FBox NineSliceBox = FBox(ForceInit);

	FGuid UniqueKey() const { return Key; }

	TMap<FString, FModumateUnitValue> NamedDimensions;

	void ReadNamedDimensions(const FString& InNamedDimensions);
};

template<>
struct TStructOpsTypeTraits<FArchitecturalMesh> : public TStructOpsTypeTraitsBase2<FArchitecturalMesh>
{
	enum
	{
		// Override default traits here
	};
};