// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Input/Events.h"
#include "UI/DimensionActor.h"

#include "CabinetDimensionActor.generated.h"

UCLASS()
class MODUMATE_API ACabinetDimensionActor : public ADimensionActor
{
	GENERATED_BODY()

public:
	ACabinetDimensionActor(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	int32 CabinetID;
};
