// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "ModumateCore/ModumateTypes.h"
#include "ProceduralMeshComponent.h"
#include "LineActor.generated.h"

class AEditModelPlayerState;
class AEditModelGameMode;
class AEditModelPlayerHUD;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class AEditModelGameState;

UCLASS()
class MODUMATE_API ALineActor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALineActor();

	bool MakeGeometry();
	bool UpdateTransform();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void UpdateColor();

public:
	UPROPERTY()
	AEditModelPlayerState* EMPlayerState;

	UPROPERTY()
	AEditModelGameMode* EMGameMode;

	UPROPERTY()
	AEditModelPlayerHUD* EMPlayerHUD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	FVector Point1 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	FVector Point2 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	FColor Color = FColor(0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drawing")
	float Thickness = 1.f;

	void SetIsHUD(bool bRenderScreenSpace);
	void SetVisibilityInApp(bool bVisible);

	void UpdateLineVisuals(bool bConnected, float ThicknessMultiplier = 1.0f, FColor NewColor = FColor::Black);

private:
	bool bIsHUD = true;
	bool bVisibleInApp = true;
};
