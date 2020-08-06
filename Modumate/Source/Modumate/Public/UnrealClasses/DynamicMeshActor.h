// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ModumateCore/ModumateTypes.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "BIMKernel/BIMAssemblySpec.h"
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
	TMap<TPair<int32, int32>, FVector> exteriorTriangleNormals;
	float baseArea = 0.0f;
	float meshVolume = 0.0f;

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;

	void MakeTriangle(const FVector &p1, const FVector &p2, const FVector &p3, int32 dir, int32 facedir, const float uvRot = 0.0f);
	void MakeTriangleUVs(const FVector &p1, const FVector &p2, const FVector &p3, int32 dir, int32 facedir, float uvRot = 0.0f);
	void CacheTriangleNormals(const TArray<FVector> &points, int32 cp1, int32 cp2, int32 cp3);


public:
	// Sets default values for this actor's properties
	ADynamicMeshActor();

	void SetupProceduralLayers(int32 numProceduralLayers);
	void ClearProceduralLayers();

	void SetupFlatPolyGeometry(const TArray<FVector> &points, const FBIMAssemblySpec &assembly);
	void UpdateFlatPolyGeometry(const TArray<FVector> &points, const FBIMAssemblySpec &assembly);

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

	void DynamicMeshActorMoved(const FVector& newPosition);
	void DynamicMeshActorRotated(const FRotator& newRotation);

	void SetupRailGeometry(const TArray<FVector> &points, float height);
	void UpdateRailGeometry(const TArray<FVector> &points, float height);

	const TArray<FPolyHole3D> &GetHoles3D() const { return Holes3D; }
	bool CreateBasicLayerDefs(const TArray<FVector> &PlanePoints, const FVector &PlaneNormal,
		const FBIMAssemblySpec &InAssembly, float PlaneOffsetPCT,
		const FVector &AxisX = FVector::ZeroVector, float UVRotOffset = 0.0f, bool bToleratePlanarErrors = false);
	bool UpdatePlaneHostedMesh(bool bRecreateMesh, bool bUpdateCollision, bool bEnableCollision,
		const FVector &InUVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f);

	void AddPrismGeometry(const TArray<FVector> &points, const FVector &centroid, float height, const FVector ExtrusionDir,
		float uvRotOffset = 0.0f, bool bMakeBottom = true, bool bMakeTop = true, bool bMakeSides = true, const TArray<FPolyHole3D> *holes = nullptr);

	void SetupPrismGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &material,
		float uvRotOffset = 0.0f, bool bMakeSides = true, const TArray<FPolyHole3D> *holes = nullptr);
	void UpdatePrismGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &material,
		float uvRotOffset = 0.0f, bool bMakeSides = true, const TArray<FPolyHole3D> *holes = nullptr);

	void SetupCabinetGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &material, const FVector2D &toeKickDimensions = FVector2D::ZeroVector, int32 frontIndexStart = 0, float uvRotOffset = 0.0f);
	void UpdateCabinetGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &material, const FVector2D &toeKickDimensions = FVector2D::ZeroVector, int32 frontIndexStart = 0, float uvRotOffset = 0.0f);

	void SetupExtrudedPolyGeometry(const FBIMAssemblySpec &assembly, const FVector &p1, const FVector &p2, const FVector &objNormal, const FVector &objUp,
		const FVector2D &upperExtensions = FVector2D::ZeroVector, const FVector2D &outerExtensions = FVector2D::ZeroVector, const FVector &scale = FVector::OneVector, bool bRecreateSection = true, bool bCreateCollision = true);
	void UpdateExtrudedPolyGeometry(const FBIMAssemblySpec &assembly, const FVector &p1, const FVector &p2, const FVector &objNormal, const FVector &objUp,
		const FVector2D &upperExtensions = FVector2D::ZeroVector, const FVector2D &outerExtensions = FVector2D::ZeroVector, const FVector &scale = FVector::OneVector, bool bCreateCollision = true);

	void SetupMasksGeometry(const TArray<TArray<FVector>> &Polygons, const FPlane &Plane, const FVector &Origin, const FVector &AxisX, const FVector &AxisY);

	void UpdateLayerMaterialsFromAssembly();

	bool GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const;

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

	// The most recent list of hole sections used to bore holes in this geometry.
	TArray<FPolyHole3D> Holes3D;

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

class UTexture;

UCLASS()
class MODUMATE_API ADynamicCursor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADynamicCursor();
	//TSubclassOf<class ADynamicCursor> C_DynamicCursor_BP;
};

//TSubclassOf<class ADynamicCursor> C_DynamicCursor_BP;