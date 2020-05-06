// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ModumateTypes.h"
#include "ModumateShoppingItem.h"
#include "LineActor.generated.h"

class AEditModelPlayerState_CPP;
class AEditModelGameMode_CPP;
class AEditModelPlayerHUD_CPP;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class AEditModelGameState_CPP;

UCLASS()
class MODUMATE_API ALineActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALineActor(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FString OldKey;
	FVector OldPoint1;
	FVector OldPoint2;

public:
	UPROPERTY()
	UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY()
	AEditModelPlayerState_CPP* EMPlayerState;

	UPROPERTY()
	AEditModelGameMode_CPP* EMGameMode;

	UPROPERTY()
	AEditModelGameState_CPP* EMGameState;

	UPROPERTY()
	AEditModelPlayerHUD_CPP* EMPlayerHUD;

	UPROPERTY()
	TArray<FShoppingItem> AssemblyShoppingItems;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> DynamicMaterials;


	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	FVector Point1 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	FVector Point2 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	FColor Color = FColor(0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	float Thickness = 1.f;

	void UpdateMetaEdgeVisuals(bool bConnected, float ThicknessMultiplier = 1.0f);
};
