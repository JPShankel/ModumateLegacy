// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Input/Events.h"
#include "UI/DimensionActor.h"

#include "GraphDimensionActor.generated.h"

namespace Modumate 
{
	class FGraph3D;
}

UCLASS()
class MODUMATE_API AGraphDimensionActor : public ADimensionActor
{
	GENERATED_BODY()

public:
	AGraphDimensionActor(const FObjectInitializer& ObjectInitializer);

	void SetTarget(int32 InTargetEdgeID, int32 InTargetObjID, bool bIsEditable);

	virtual ALineActor* GetLineActor() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

public:
	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

private:
	int32 TargetEdgeID;
	int32 TargetObjID;

	const Modumate::FGraph3D *Graph;
};
