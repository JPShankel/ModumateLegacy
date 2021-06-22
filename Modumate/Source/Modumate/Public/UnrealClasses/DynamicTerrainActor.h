// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ModumateCore/ModumateGeometryStatics.h"

#include "DynamicTerrainActor.generated.h"

class FDraftingComposite;

UCLASS()
class MODUMATE_API ADynamicTerrainActor : public AActor
{
	GENERATED_BODY()

private:

public:
	// Sets default values for this actor's properties
	ADynamicTerrainActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void GetGridTriangulationPoints(FVector2D PointA, FVector2D PointB, float GridSize, TArray<FVector2D>& OutPoints) const;

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
	int32 UVSize = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* TerrainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* GrassStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GrassMeshOffset = FVector(0.f, 0.f, -10.f);

	//UFUNCTION(BlueprintCallable)
	//void SetupTerrainGeometry(const TArray<FVector>& PerimeterPoints, const TArray<FVector>& HeightPoints, bool bRecreateMesh, bool bCreateCollision = true);

	void SetupTerrainGeometry(int32 SectionID, float GridSize, const TArray<FVector2D>& PerimeterPoints, const TArray<FVector>& HeightPoints,
		const TArray<FPolyHole2D>& HolePoints, bool bAddGraphPoints, bool bCreateCollision = true);
	void UpdateEnableCollision(bool bEnableCollision);
	void ClearAllMeshSections() { Mesh->ClearAllMeshSections(); }

	// TODO: Update instances for triangles in mesh sections
#if 0
	void UpdateInstancedMeshes(bool bRecreateMesh);
	bool GetRandomPointsOnTriangleSurface(int32 Tri1, int32 Tri2, int32 Tri3, int32 NumOfOutPoints, TArray<FVector>& OutPoints);
#endif

	bool GetCutPlaneDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const;
	void GetFarDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FBox2D& BoundingBox) const;
};
