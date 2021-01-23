// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOIGroupActor.generated.h"

class AModumateObjectInstance;

UCLASS()
class MODUMATE_API AMOIGroupActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMOIGroupActor();

	UPROPERTY()
	AModumateObjectInstance *MOI;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetMemberLocations();




};
