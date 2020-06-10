// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Input/Events.h"
#include "UI/DimensionActor.h"

#include "PendingSegmentActor.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API APendingSegmentActor : public ADimensionActor
{
	GENERATED_BODY()

public:
	APendingSegmentActor(const FObjectInitializer& ObjectInitializer);

public:
	virtual ALineActor* GetLineActor() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	// TODO: when input handling is refactored, the input can be sent here.  
	// Possibly, the tool that uses this actor should implement this function instead and 
	// add it to DimensionText
	//UFUNCTION()
	//void OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

public:
	UPROPERTY()
	ALineActor* PendingSegment;
};
