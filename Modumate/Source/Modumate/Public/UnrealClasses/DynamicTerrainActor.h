// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ModumateCore/ModumateGeometryStatics.h"

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

	// Uncomment to test SimpleDynamicMesh
	//class USimpleDynamicMeshComponent* SimpleDynamicMesh = nullptr;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UProceduralMeshComponent* Mesh;

	UPROPERTY()
	class UHierarchicalInstancedStaticMeshComponent* GrassMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticesDensityPerRow = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinVertSize = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxVertSize = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UVSize = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* TerrainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* GrassStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GrassMeshOffset = FVector(0.f, 0.f, -10.f);

	//UFUNCTION(BlueprintCallable)
	//void SetupTerrainGeometry(const TArray<FVector>& PerimeterPoints, const TArray<FVector>& HeightPoints, bool bRecreateMesh, bool bCreateCollision = true);

	UFUNCTION(BlueprintCallable)
	void UpdateTerrainGeometryFromPoints(const TArray<FVector>& HeightPoints);

	UFUNCTION(BlueprintCallable)
	void UpdateVertexColorByLocation(const FVector& Location, const FLinearColor& NewColor, float Radius, float Alpha);


	void TestSetupTerrainGeometryGTE(const TArray<FVector2D>& PerimeterPoints, const TArray<FVector>& HeightPoints, const TArray<FPolyHole2D>& HolePoints, bool bAddGraphPoints, bool bCreateCollision = true);

	void UpdateInstancedMeshes(bool bRecreateMesh);
	bool GetRandomPointsOnTriangleSurface(int32 Tri1, int32 Tri2, int32 Tri3, int32 NumOfOutPoints, TArray<FVector>& OutPoints);
};
