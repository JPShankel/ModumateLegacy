// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Input/Events.h"
#include "UI/DimensionActor.h"

#include "AngleDimensionActor.generated.h"

class AArcActor;

UCLASS()
class MODUMATE_API AAngleDimensionActor : public ADimensionActor
{
	GENERATED_BODY()

public:
	AAngleDimensionActor(const FObjectInitializer& ObjectInitializer);

	void SetTarget(int32 EdgeID, TPair<int32, int32> FaceIDs);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY()
	AArcActor *PendingArc;

	int32 AnchorEdgeID;
	int32 StartFaceID;
	int32 EndFaceID;
};
