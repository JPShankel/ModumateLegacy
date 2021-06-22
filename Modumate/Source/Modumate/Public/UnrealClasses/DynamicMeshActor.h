// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "GameFramework/Actor.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateTypes.h"
#include "Objects/DimensionOffset.h"
#include "ProceduralMeshComponent.h"

#include "DynamicMeshActor.generated.h"


class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FWallAssemblyLayerControlPoints
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FVector> LayerControlPoints;
};

USTRUCT(BlueprintType)
struct FFloorAssemblyLayerControlPoints
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FVector> TopLayerControlPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FVector> BottomLayerControlPoints;
};

USTRUCT(BlueprintType)
struct FProceduralMeshParams
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh Parameters")
	TArray<FVector> Vertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh Parameters")
	TArray<int32> Triangles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh Parameters")
	TArray<FVector> Normals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh Parameters")
	TArray<FVector2D> UVs;
};

UCLASS()
class MODUMATE_API ADynamicMeshActor : public AActor
{
	GENERATED_BODY()

private:

	TArray<FVector> vertices,normals;
	TArray<int32> triangles;
	TArray<FVector2D> uv0;
	TArray<FProcMeshTangent> tangents;
	TArray<FLinearColor> vertexColors;
	float baseArea = 0.0f;
	float meshVolume = 0.0f;

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;

	void MakeTriangle(const FVector &p1, const FVector &p2, const FVector &p3, int32 dir, int32 facedir, const float uvRot = 0.0f);
	void MakeTriangleUVs(const FVector &p1, const FVector &p2, const FVector &p3, int32 dir, int32 facedir, float uvRot = 0.0f);


public:
	// Sets default values for this actor's properties
	ADynamicMeshActor();

	void SetupProceduralLayers(int32 numProceduralLayers);
	void ClearProceduralLayers();
	void ClearProceduralMesh() { Mesh->ClearAllMeshSections(); }

	void SetupPlaneGeometry(const TArray<FVector> &points, const FArchitecturalMaterial &material, bool bRecreateMesh, bool bCreateCollision = true);
	void SetupMetaPlaneGeometry(const TArray<FVector> &points, 
		const FArchitecturalMaterial &material, 
		float alpha, 
		bool bRecreateMesh, 
		const TArray<FPolyHole3D> *holes = nullptr, 
		bool bCreateCollision = true);

	void SetupRoomGeometry(const TArray<TArray<FVector>> &Polygons, const FArchitecturalMaterial &Material);

	bool SetupStairPolys(const FVector &StairOrigin,
		const TArray<TArray<FVector>> &TreadPolys, const TArray<TArray<FVector>> &RiserPolys, const TArray<FVector> &RiserNormals,
		float StepThickness, const FArchitecturalMaterial &Material);

	// Layered version
	bool SetupStairPolys(const FVector &StairOrigin,
		const TArray<TArray<FVector>> &TreadPolys, const TArray<TArray<FVector>> &RiserPolys, const TArray<FVector> &RiserNormals,
		const TArray<FLayerGeomDef>& TreadLayers, const TArray<FLayerGeomDef>& RiserLayers, const FBIMAssemblySpec& Assembly);

	void DynamicMeshActorMoved(const FVector& newPosition);
	void DynamicMeshActorRotated(const FRotator& newRotation);

	bool CreateBasicLayerDefs(const TArray<FVector> &PlanePoints, const FVector &PlaneNormal,
		const TArray<FPolyHole3D>& Holes, const FBIMAssemblySpec &InAssembly, float PlaneOffsetPCT,
		const FVector &AxisX = FVector::ZeroVector, float UVRotOffset = 0.0f, bool bToleratePlanarErrors = false);
	bool UpdatePlaneHostedMesh(bool bRecreateMesh, bool bUpdateCollision, bool bEnableCollision,
		const FVector &InUVAnchor = FVector::ZeroVector, const FVector2D& InUVFlip = FVector2D(1.0f, 1.0f), float UVRotOffset = 0.0f);

	void SetupPrismGeometry(const TArray<FVector>& BasePpoints, const FVector& ExtrusionDelta, const FArchitecturalMaterial& MaterialData, bool bUpdateCollision, bool bEnableCollision,
		const FVector2D& UVFlip = FVector2D::UnitVector, float UVRotOffset = 0.0f);
	void SetupCabinetGeometry(const TArray<FVector>& BasePoints, const FVector& ExtrusionDelta, const FArchitecturalMaterial& MaterialData, bool bUpdateCollision, bool bEnableCollision,
		const FVector2D& ToeKickDimensions = FVector2D::ZeroVector, float FaceInsetDist = 0.0f, int32 FrontIdxStart = INDEX_NONE,
		const FVector2D& UVFlip = FVector2D(1.0f, 1.0f), float UVRotOffset = 0.0f);

	bool SetupExtrudedPolyGeometry(const FBIMAssemblySpec& InAssembly, const FVector& InStartPoint, const FVector& InEndPoint,
		const FVector& ObjNormal, const FVector& ObjUp, const FDimensionOffset& OffsetNormal, const FDimensionOffset& OffsetUp,
		const FVector2D& Extensions = FVector2D::ZeroVector, const FVector& InFlipSigns = FVector::OneVector, bool bRecreateSection = true, bool bCreateCollision = true);

	void SetupMasksGeometry(const TArray<TArray<FVector>> &Polygons, const FPlane &Plane, const FVector &Origin, const FVector &AxisX, const FVector &AxisY);

	void UpdateLayerMaterialsFromAssembly();

	bool HasPlacementError() const { return PlacementErrors.Num() > 0; }
	bool SetPlacementError(FName errorTag, bool bIsError);
	void ClearPlacementErrors() { PlacementErrors.Reset(); }
	const TSet<FName> &GetPlacementErrors() const { return PlacementErrors; }

	const FBIMAssemblySpec &GetAssembly() { return Assembly; }
	FBIMAssemblySpec Assembly;

	float Height; // Height of actor, from base

	// TODO: add support for these for non-prism shapes
	float GetBaseArea() const { return baseArea; }
	float GetMeshVolume() const { return meshVolume; }

	void SetIsDynamic(bool DynamicStatus);
	bool GetIsDynamic() const { return bIsDynamic; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// The current set of error tags that are making this actor invalid.
	TSet<FName> PlacementErrors;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	UProceduralMeshComponent *Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> OldControlPoints;

	// (For now, only for plane-hosted layered assemblies:) the world-space geometric definitions
	// of each of the layers that will be used for layered assemblies, and will be triangulated into ProcMeshComponents.
	UPROPERTY()
	TArray<FLayerGeomDef> LayerGeometries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector UVAnchor = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> OldFloorCP1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> OldFloorCP2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> UVFloorAnchors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TopUVFloorAnchor = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator TopUVFloorRot = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AdjustHandleSide = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UProceduralMeshComponent*> ProceduralSubLayers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UMaterialInstanceDynamic*> CachedMIDs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWallAssemblyLayerControlPoints> AssemblyLayerControlPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FFloorAssemblyLayerControlPoints> FloorAssemblyLayerControlPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface *PlacementWarningMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface *WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface *FloorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface *StaircaseMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UMaterialInterface*> TilingMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface *MasterPBRMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsDynamic = true;
};
