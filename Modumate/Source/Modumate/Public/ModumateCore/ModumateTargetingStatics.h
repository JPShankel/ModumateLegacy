// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ModumateTargetingStatics.generated.h"

class FModumateObjectInstance;

UCLASS(BlueprintType)
class MODUMATE_API UModumateTargetingStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static int32 GetFaceIndexFromTargetHit(const FModumateObjectInstance* HitObject, const FVector& HitLocation, const FVector& HitNormal);

	static void GetConnectedSurfaceGraphs(const FModumateObjectInstance* HitObject, const FVector& HitLocation, TArray<const FModumateObjectInstance*>& OutSurfaceGraphs);
};
