// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "Input/Events.h"

#include "DimensionActor.generated.h"

class UDimensionWidget;

namespace Modumate 
{
	class FGraph3D;
}

UCLASS()
class MODUMATE_API ADimensionActor : public AActor
{
	GENERATED_BODY()

public:
	ADimensionActor(const FObjectInitializer& ObjectInitializer);

	void CreateWidget();
	void ReleaseWidget();

	void SetTarget(int32 InTargetEdgeID, int32 InTargetObjID, bool bIsEditable);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

public:
	UPROPERTY()
	UDimensionWidget* DimensionText;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

private:
	int32 TargetEdgeID;
	int32 TargetObjID;

	const Modumate::FGraph3D *Graph;
};
