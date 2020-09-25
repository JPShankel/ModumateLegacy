// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "UI/DimensionActor.h"

#include "PendingAngleActor.generated.h"

UCLASS()
class MODUMATE_API APendingAngleActor : public ADimensionActor
{
	GENERATED_BODY()

public:
	APendingAngleActor(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

public:
	FVector WorldPosition;
	FVector WorldDirection;
	float CurrentAngle;
};
