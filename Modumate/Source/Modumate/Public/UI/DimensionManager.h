// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DimensionManager.generated.h"

class ADimensionActor;
class AGraphDimensionActor;
class APendingSegmentActor;

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
	void ClearGraphDimensionStrings();

	template <typename DimensionActorT = ADimensionActor>
	ADimensionActor *AddDimensionActor(TSubclassOf<ADimensionActor> ActorClass)
	{
		auto actor = GetWorld()->SpawnActor<ADimensionActor>(ActorClass.Get());
		actor->ID = NextID++;
		DimensionActors.Add(actor->ID, actor);
		return actor;
	}

	ADimensionActor *GetDimensionActor(int32 id);
	void ReleaseDimensionActor(int32 id);

private:

	int32 LastSelectedObjID;
	// if there is one selected object, save the ID.  If it's a graph object, dimension strings
	// will be displayed
	TArray<int32> LastSelectedVertexIDs;
	TArray<int32> CurrentGraphDimensionStrings;

	TMap<int32, ADimensionActor*> DimensionActors;
	int32 NextID = 1;
};
