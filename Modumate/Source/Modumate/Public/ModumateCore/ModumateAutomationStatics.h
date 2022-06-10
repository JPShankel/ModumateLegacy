// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Graph/Graph3DDelta.h"
#include "Graph/Graph3D.h"

#include "ModumateAutomationStatics.generated.h"


// Helper functions for automation operations;
UCLASS()
class MODUMATE_API UModumateAutomationStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static TArray<TArray<FVector>> GenerateProceduralGeometry(UWorld* World, int32 NextAvailableID, FGraph3D* Graph, int32 XIterations, int32 YIterations, TSharedRef<FGraph3DDelta> outGraphDelta, float XDim = 100.0f, float YDim = 100.0f, FVector PlaneOrigin = FVector::ZeroVector, FVector PlaneDirection = FVector(1,0,0));

};
