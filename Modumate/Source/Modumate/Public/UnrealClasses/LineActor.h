// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ModumateCore/ModumateTypes.h"
#include "Database/ModumateShoppingItem.h"
#include "LineActor.generated.h"

class AEditModelPlayerState_CPP;
class AEditModelGameMode_CPP;
class AEditModelPlayerHUD;
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

	bool MakeGeometry();
	bool UpdateGeometry();
	bool CalculateVertices();

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> ScreenVertices;
	TArray<FVector2D> Uvs;
	TArray<int32> Tris;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> Colors;

	FString OldKey;
	FVector OldPoint1;
	FVector OldPoint2;

	bool bCreate = false;

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
	AEditModelPlayerHUD* EMPlayerHUD;

	UPROPERTY()
	TArray<FShoppingItem> AssemblyShoppingItems;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> DynamicMaterials;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UMaterialInstanceDynamic*> CachedMIDs;

	APlayerCameraManager* CameraManager;

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

	void SetIsHUD(bool bRenderScreenSpace);
	void SetVisibilityInApp(bool bVisible);

	void UpdateMetaEdgeVisuals(bool bConnected, float ThicknessMultiplier = 1.0f);

private:
	bool bIsHUD = true;
	bool bVisibleInApp = true;
	bool bLastRenderValid = true;
};
