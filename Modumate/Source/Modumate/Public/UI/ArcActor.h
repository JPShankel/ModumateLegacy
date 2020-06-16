// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

#include "ArcActor.generated.h"

UCLASS()
class MODUMATE_API AArcActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArcActor(const FObjectInitializer& ObjectInitializer);

public:
	// TODO: the mesh that renders the arc should either be
	// referenced here or the function used be the blueprint for 
	// setting the bounds of the arc need to be implemented here instead
	/*
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMeshComponent *DegreePlane;
	//*/
};
