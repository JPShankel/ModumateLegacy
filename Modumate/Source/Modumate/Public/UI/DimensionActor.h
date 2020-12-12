// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"

#include "DimensionActor.generated.h"

class ALineActor;
class UDimensionWidget;

UCLASS()
class MODUMATE_API ADimensionActor : public AActor
{
	GENERATED_BODY()

public:
	ADimensionActor(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void CreateWidget();
	void ReleaseWidget();

public:
	virtual ALineActor* GetLineActor();

public:
	UPROPERTY()
	UDimensionWidget* DimensionText;

	UPROPERTY()
	class UModumateDocument* Document;

	int32 ID;
};
