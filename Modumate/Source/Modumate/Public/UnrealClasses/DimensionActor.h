// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"

#include "DimensionActor.generated.h"

class UDimensionWidget;
class ALineActor;

UCLASS()
class MODUMATE_API ADimensionActor : public AActor
{
	GENERATED_BODY()

public:
	ADimensionActor(const FObjectInitializer& ObjectInitializer);

	void CreateWidget();
	void ReleaseWidget();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY()
	UDimensionWidget* DimensionText;
};
