// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DimensionManager.generated.h"

class ADimensionActor;

UCLASS()
class MODUMATE_API UDimensionManager : public UObject
{
	GENERATED_BODY()

	UDimensionManager(const FObjectInitializer& ObjectInitializer);

public:
	void Init();
	void Reset();
	void Shutdown();

	void UpdateGraphDimensionStrings(int32 selectedGraphObjID);

private:
	UPROPERTY()
	TArray<ADimensionActor*> DimensionActors;

	int32 LastSelectedObjID;
	// if there is one selected object, save the ID.  If it's a graph object, dimension strings
	// will be displayed
	TArray<int32> LastSelectedVertexIDs;
};