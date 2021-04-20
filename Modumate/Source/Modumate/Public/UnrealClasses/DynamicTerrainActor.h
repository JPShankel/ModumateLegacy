// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

#include "DynamicTerrainActor.generated.h"

UCLASS()
class MODUMATE_API ADynamicTerrainActor : public AActor
{
	GENERATED_BODY()

private:

	TArray<FVector2D> GridPoints;
	TArray<FVector> Vertices;
	TArray<FVector2D> Vertices2D;
	TArray<FVector> Normals;
	TArray<int32> Triangles;
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;

public:
	// Sets default values for this actor's properties
	ADynamicTerrainActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UProceduralMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VertSize = 100.f;

	UFUNCTION(BlueprintCallable)
	void SetupTerrainGeometry(const TArray<FVector>& PerimeterPoints, const TArray<FVector>& HeightPoints, bool bRecreateMesh, bool bCreateCollision = true);

	UFUNCTION(BlueprintCallable)
	void UpdateTerrainGeometryFromPoints(const TArray<FVector>& HeightPoints);
};
