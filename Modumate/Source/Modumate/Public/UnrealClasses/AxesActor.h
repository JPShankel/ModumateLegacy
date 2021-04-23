// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AxesActor.generated.h"

UCLASS()
class MODUMATE_API AAxesActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAxesActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
