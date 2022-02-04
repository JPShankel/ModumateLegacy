// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/DynamicMeshActor.h"

#include "UnrealClasses/EditModelGameMode.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Algo/Transform.h"
#include "Algo/Accumulate.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerState.h"

// Sets default values
ADynamicMeshActor::ADynamicMeshActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = Mesh;

	Mesh->bUseAsyncCooking = true;
	Mesh->SetMobility(bIsDynamic ? EComponentMobility::Movable : EComponentMobility::Static);

	Height = 0;

	// Cap for cutplane culling
	MeshCap = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMeshCap"));
	MeshCap->bUseAsyncCooking = true;
	MeshCap->SetMobility(bIsDynamic ? EComponentMobility::Movable : EComponentMobility::Static);
	MeshCap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshCap->SetCastShadow(false);
}

// Called when the game starts or when spawned
void ADynamicMeshActor::BeginPlay()
{
	Super::BeginPlay();

	if (WallMaterial)
	{
		Mesh->SetMaterial(0, WallMaterial);
	}
}

// Called every frame
void ADynamicMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ADynamicMeshActor::PostActorCreated()
{
	Super::PostActorCreated();
}

// This is called when actor is already in level and map is opened
void ADynamicMeshActor::PostLoad()
{
	Super::PostLoad();
}


void ADynamicMeshActor::MakeTriangleUVs(const FVector &p1, const FVector &p2, const FVector &p3, int32 dir, int32 facedir, float uvRot)
{
	static const float uvFactor = 0.01f;

	FVector dp1 = p2 - p1;
	FVector dp2 = p3 - p1;

	switch (facedir)
	{
	case -1:
	{
		FPlane triPlane(p1, p2, p3);
		FVector uvX = FVector::ForwardVector;
		FVector uvY = FVector::RightVector;
		if (!FVector::Parallel(triPlane, FVector::UpVector))
		{
			uvX = FVector::UpVector ^ triPlane;
			uvY = triPlane ^ uvX;
		}

		uv0.Add(uvFactor * FVector2D(p1 | uvX, p1 | uvY).GetRotated(uvRot));
		uv0.Add(uvFactor * FVector2D(p2 | uvX, p2 | uvY).GetRotated(uvRot));
		uv0.Add(uvFactor * FVector2D(p3 | uvX, p3 | uvY).GetRotated(uvRot));
	}
	break;
	case 0:
	{
		if (dir == 0)
		{
			uv0.Add(FVector2D(dp1.Size() * uvFactor, dp2.Size() * uvFactor));
			uv0.Add(FVector2D(0, dp2.Size() * uvFactor));
			uv0.Add(FVector2D(dp1.Size() * uvFactor, 0));
		}
		else
		{
			uv0.Add(FVector2D(0, 0));
			uv0.Add(FVector2D(dp1.Size() * uvFactor, 0));
			uv0.Add(FVector2D(0, dp2.Size() * uvFactor));
		}
	}
	break;
	case 1:
	{
		if (dir == 0)
		{
			uv0.Add(FVector2D(dp2.Size() * uvFactor, dp1.Size() * uvFactor));
			uv0.Add(FVector2D(dp2.Size() * uvFactor, 0));
			uv0.Add(FVector2D(0, dp1.Size() * uvFactor));
		}
		else
		{
			uv0.Add(FVector2D(0, 0));
			uv0.Add(FVector2D(0, dp1.Size() * uvFactor));
			uv0.Add(FVector2D(dp2.Size() * uvFactor, 0));
		}
	}
	break;
	case 2:
	{
		if (dir == 0)
		{
			uv0.Add(FVector2D(dp1.Size() * uvFactor, dp2.Size() * uvFactor));
			uv0.Add(FVector2D(0, dp2.Size() * uvFactor));
			uv0.Add(FVector2D(dp1.Size() * uvFactor, 0));
		}
		else
		{
			uv0.Add(FVector2D(0, 0));
			uv0.Add(FVector2D(dp1.Size() * uvFactor, 0));
			uv0.Add(FVector2D(0, dp2.Size() * uvFactor));
		}
	}
	break;
	case 3:
	{
		if (dir == 0)
		{
			uv0.Add(FVector2D(0, 0));
			uv0.Add(FVector2D(0, dp1.Size() * uvFactor));
			uv0.Add(FVector2D(dp2.Size() * uvFactor, 0));
		}
		else
		{
			uv0.Add(FVector2D(dp2.Size() * uvFactor, dp1.Size() * uvFactor));
			uv0.Add(FVector2D(dp2.Size() * uvFactor, 0));
			uv0.Add(FVector2D(0, dp1.Size() * uvFactor));
		}
	}
	break;
	default:
	{
		if (dir == 0)
		{
			uv0.Add(FVector2D(0, 0));
			uv0.Add(FVector2D(dp1.Size() * uvFactor, 0));
			uv0.Add(FVector2D(0, dp2.Size() * uvFactor));
		}
		else
		{
			uv0.Add(FVector2D(dp1.Size() * uvFactor, dp2.Size() * uvFactor));
			uv0.Add(FVector2D(0, dp2.Size() * uvFactor));
			uv0.Add(FVector2D(dp1.Size() * uvFactor, 0));
		}
	}
	break;
	}
}

void ADynamicMeshActor::MakeTriangle(const FVector &p1, const FVector &p2, const FVector &p3, int32 dir, int32 facedir, float uvRot)
{
	int32 startIndex = vertices.Num();
	vertices.Add(p1);
	vertices.Add(p2);
	vertices.Add(p3);

	triangles.Add(startIndex);
	triangles.Add(startIndex + 1);
	triangles.Add(startIndex + 2);

	FVector dp1 = (p2 - p1).GetSafeNormal();
	FVector dp2 = (p3 - p1).GetSafeNormal();

	FVector norm = FVector::CrossProduct(dp2, dp1).GetSafeNormal();
	normals.Add(norm);
	normals.Add(norm);
	normals.Add(norm);

	MakeTriangleUVs(p1, p2, p3, dir, facedir, uvRot);

	tangents.Add(FProcMeshTangent(0, 1, 0));
	tangents.Add(FProcMeshTangent(0, 1, 0));
	tangents.Add(FProcMeshTangent(0, 1, 0));

	vertexColors.Add(FLinearColor(1, 1, 1, 1.0));
	vertexColors.Add(FLinearColor(1, 1, 1, 1.0));
	vertexColors.Add(FLinearColor(1, 1, 1, 1.0));
}

bool ADynamicMeshActor::CreateBasicLayerDefs(const TArray<FVector> &PlanePoints, const FVector &PlaneNormal,
	const TArray<FPolyHole3D>& Holes, const FBIMAssemblySpec &InAssembly, float PlaneOffsetPCT,
	const FVector &AxisX, float UVRotOffset, bool bToleratePlanarErrors)
{
	TArray<FVector> layerPointsA, layerPointsB, fixedPlanePoints;
	int32 numLayers = InAssembly.Layers.Num();
	int32 numPoints = PlanePoints.Num();

	if (!ensureAlways((numLayers > 0) && (numPoints > 0)))
	{
		return false;
	}

	// Ensure that the supplied points are at least planar, and then either the normal vector will be derived,
	// or supplied as a vector that's parallel to the points' plane's normal.
	FPlane planeGeom;
	bool bPlanarSuccess = UModumateGeometryStatics::GetPlaneFromPoints(PlanePoints, planeGeom) &&
		(PlaneNormal.IsZero() || FVector::Parallel(PlaneNormal, planeGeom));

	// If we don't allow planar errors, then require that the supplied points are planar,
	// and whose normal is parallel to the supplied normal (if one was provided)
	if (!bToleratePlanarErrors && !ensureAlways(bPlanarSuccess))
	{
		return false;
	}

	// Otherwise, set an error on the object so that we know the planar points were invalid,
	// but we want to try to create a mesh for the object anyway.
	static const FName planarErrorName(TEXT("LayerPointsNonPlanar"));
	SetPlacementError(planarErrorName, !bPlanarSuccess);
	if (bPlanarSuccess)
	{
		fixedPlanePoints = PlanePoints;
	}
	else
	{
		Algo::Transform(PlanePoints, fixedPlanePoints,
			[planeGeom](const FVector &point) { return FVector::PointPlaneProject(point, planeGeom); });
	}

	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FQuat::Identity);

	FVector layersNormal = PlaneNormal.IsZero() ? FVector(planeGeom) : PlaneNormal;
	Assembly = InAssembly;
	LayerGeometries.SetNum(numLayers);
	float totalThickness = Assembly.CalculateThickness().AsWorldCentimeters();
	float totalOffset = totalThickness * -FMath::Clamp(PlaneOffsetPCT, 0.0f, 1.0f);

	FVector commonAxisX = AxisX;
	float accumThickness = 0.0f;
	for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
	{
		const FBIMLayerSpec &layer = Assembly.Layers[layerIdx];
		float layerThickness = layer.ThicknessCentimeters;

		layerPointsA.Reset(numPoints);
		layerPointsB.Reset(numPoints);
		for (const FVector &planePoint : fixedPlanePoints)
		{
			layerPointsA.Add(planePoint + (totalOffset + accumThickness) * layersNormal);
			layerPointsB.Add(planePoint + (totalOffset + accumThickness + layerThickness) * layersNormal);
		}

		FLayerGeomDef &layerGeomDef = LayerGeometries[layerIdx];
		layerGeomDef.Init(layerPointsA, layerPointsB, layersNormal, commonAxisX, &Holes);
		if (!commonAxisX.IsNormalized())
		{
			commonAxisX = layerGeomDef.AxisX;
		}

		accumThickness += layerThickness;
	}

	return true;
}

bool ADynamicMeshActor::UpdatePlaneHostedMesh(bool bRecreateMesh, bool bUpdateCollision, bool bEnableCollision, const FVector &InUVAnchor, const FVector2D& InUVFlip, float UVRotOffset)
{
	int32 numLayers = LayerGeometries.Num();
	if (!ensureAlways(numLayers > 0))
	{
		return false;
	}

	if (bRecreateMesh || (numLayers != ProceduralSubLayers.Num()))
	{
		SetupProceduralLayers(numLayers);
	}

	UVAnchor = InUVAnchor;

	for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
	{
		UProceduralMeshComponent *procMeshComp = ProceduralSubLayers[layerIdx];
		const FLayerGeomDef &layerGeomDef = LayerGeometries[layerIdx];

		vertices.Reset();
		normals.Reset();
		triangles.Reset();
		uv0.Reset();
		tangents.Reset();
		vertexColors.Reset();

		bool bLayerVisible = layerGeomDef.bValid && (layerGeomDef.Thickness > 0.0f);

		if (bLayerVisible && layerGeomDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents, UVAnchor, InUVFlip, UVRotOffset))
		{
			// TODO: enable iterative mesh section updates when we can know that
			// the order of vertices did not change as a result of re-triangulation
			//if (bRecreateMesh || (procMeshComp->GetNumSections() == 0) || (numHoles > 0))
			{
				procMeshComp->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bUpdateCollision);
			}
			/*else
			{
				procMeshComp->UpdateMeshSection_LinearColor(0, vertices, normals, uv0, vertexColors, tangents);
			}*/

			procMeshComp->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			procMeshComp->SetVisibility(true);
		}
		else
		{
			procMeshComp->SetVisibility(false);
		}
	}

	UpdateLayerMaterialsFromAssembly();

	return true;
}

void ADynamicMeshActor::SetupPrismGeometry(const TArray<FVector> &BasePoints, const FVector& ExtrusionDelta, const FArchitecturalMaterial &MaterialData,
	bool bUpdateCollision, bool bEnableCollision, const FVector2D& UVFlip, float UVRotOffset)
{
	int32 numPoints = BasePoints.Num();
	float extrusionDist = ExtrusionDelta.Size();

	if (!ensure((numPoints >= 3) && !FMath::IsNearlyZero(extrusionDist)))
	{
		return;
	}

	// Ensure that the supplied points are planar, and consistent with the extrusion direction.
	FVector extrusionNormal = ExtrusionDelta / extrusionDist;
	FPlane basePlane;
	bool bPlanarSuccess = UModumateGeometryStatics::GetPlaneFromPoints(BasePoints, basePlane);
	if (!ensure(bPlanarSuccess && FVector::Parallel(extrusionNormal, basePlane)))
	{
		return;
	}

	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset(1);
	FLayerGeomDef& prismDef = LayerGeometries.Add_GetRef(FLayerGeomDef(BasePoints, extrusionDist, extrusionNormal));
	if (!prismDef.bValid)
	{
		return;
	}

	// Now, triangulate the prism geometry data
	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	tangents.Reset();
	vertexColors.Reset();

	if (prismDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents, UVAnchor, UVFlip, UVRotOffset))
	{
		Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bUpdateCollision);
		Mesh->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(true);

		CachedMIDs.SetNumZeroed(1);
		UModumateFunctionLibrary::SetMeshMaterial(Mesh, MaterialData, 0, &CachedMIDs[0]);
	}
	else
	{
		Mesh->SetVisibility(false);
	}
}

void ADynamicMeshActor::SetupCabinetGeometry(const TArray<FVector>& BasePoints, const FVector& ExtrusionDelta, const FArchitecturalMaterial& MaterialData, bool bUpdateCollision, bool bEnableCollision,
	const FVector2D& ToeKickDimensions, float FaceInsetDist, int32 FrontIdxStart, const FVector2D& UVFlip, float UVRotOffset)
{
	int32 numPoints = BasePoints.Num();
	float extrusionDist = ExtrusionDelta.Size();

	if (!ensure((numPoints >= 3) && !FMath::IsNearlyZero(extrusionDist) && FrontIdxStart < numPoints))
	{
		return;
	}

	// Ensure that the supplied points are planar, and consistent with the extrusion direction.
	FVector extrusionNormal = ExtrusionDelta / extrusionDist;
	FPlane basePlane;
	bool bPlanarSuccess = UModumateGeometryStatics::GetPlaneFromPoints(BasePoints, basePlane);
	if (!ensure(bPlanarSuccess && FVector::Parallel(extrusionNormal, basePlane)))
	{
		return;
	}
	FVector planeNormal = FMath::Sign(extrusionNormal | basePlane) * extrusionNormal;
	FVector centroid = Algo::Accumulate(BasePoints, FVector::ZeroVector) / numPoints;

	LayerGeometries.Reset();
	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	TArray<FVector> recenteredBasePoints;
	for (const FVector& basePoint : BasePoints)
	{
		recenteredBasePoints.Add(basePoint - centroid);
	}

	TArray<FVector> upperBoxBottomPoints(recenteredBasePoints);
	float upperBoxHeight = extrusionDist;

	// If there's a front face, then we may need to:
	// - offset the front edge of the lower cabinet box by the toe kick depth
	// - offset the front edge of the upper cabinet box by the face inset distance
	// - offset the upper cabinet box points by the toe kick height
	bool bHaveFrontFace = (FrontIdxStart != INDEX_NONE) && (FrontIdxStart <= numPoints);
	if (bHaveFrontFace)
	{
		int32 frontIdxEnd = (FrontIdxStart + 1) % numPoints;

		if (!FMath::IsNearlyZero(FaceInsetDist))
		{
			if (!UModumateGeometryStatics::TranslatePolygonEdge(upperBoxBottomPoints, planeNormal, FrontIdxStart, -FaceInsetDist,
				upperBoxBottomPoints[FrontIdxStart], upperBoxBottomPoints[frontIdxEnd]))
			{
				return;
			}
		}

		if ((ToeKickDimensions.X > 0.0f) && (ToeKickDimensions.Y > 0.0f) && (ToeKickDimensions.Y < extrusionDist))
		{
			TArray<FVector> lowerBoxBottomPoints(recenteredBasePoints);
			if (UModumateGeometryStatics::TranslatePolygonEdge(lowerBoxBottomPoints, planeNormal, FrontIdxStart, -ToeKickDimensions.X,
				lowerBoxBottomPoints[FrontIdxStart], lowerBoxBottomPoints[frontIdxEnd]))
			{
				LayerGeometries.Add(FLayerGeomDef(lowerBoxBottomPoints, ToeKickDimensions.Y, extrusionNormal));

				FVector toeKickDelta = ToeKickDimensions.Y * extrusionNormal;
				for (FVector& mainBoxBottomPoint : upperBoxBottomPoints)
				{
					mainBoxBottomPoint += toeKickDelta;
				}

				upperBoxHeight -= ToeKickDimensions.Y;
			}
		}
	}

	// Now, triangulate the intermediate cabinet box geometry data
	FLayerGeomDef& cabinetMainPrism = LayerGeometries.Add_GetRef(FLayerGeomDef(upperBoxBottomPoints, upperBoxHeight, extrusionNormal));
	int32 numLayers = LayerGeometries.Num();
	SetupProceduralLayers(numLayers);
	CachedMIDs.SetNumZeroed(1);

	for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
	{
		UProceduralMeshComponent* procMeshComp = ProceduralSubLayers[layerIdx];
		const FLayerGeomDef& layerGeomDef = LayerGeometries[layerIdx];

		vertices.Reset();
		normals.Reset();
		triangles.Reset();
		uv0.Reset();
		tangents.Reset();
		vertexColors.Reset();

		bool bLayerVisible = layerGeomDef.bValid && (layerGeomDef.Thickness > 0.0f);

		if (bLayerVisible && layerGeomDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents, UVAnchor, UVFlip, UVRotOffset))
		{
			procMeshComp->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bUpdateCollision);
			procMeshComp->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			procMeshComp->SetVisibility(true);

			UModumateFunctionLibrary::SetMeshMaterial(procMeshComp, MaterialData, 0, &CachedMIDs[0]);
		}
		else
		{
			procMeshComp->SetVisibility(false);
		}
	}
}

void ADynamicMeshActor::SetupPlaneGeometry(const TArray<FVector> &points, const FArchitecturalMaterial &material, bool bRecreateMesh, bool bCreateCollision)
{
	FPlane pointsPlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(points, pointsPlane))
	{
		return;
	}

	FVector planeNormal(pointsPlane);
	Assembly = FBIMAssemblySpec();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset();


	TArray<FVector> relativePoints;
	Algo::Transform(points, relativePoints, [centroid](const FVector &worldPoint) { return worldPoint - centroid; });

	FLayerGeomDef &layerGeomDef = LayerGeometries.AddDefaulted_GetRef();
	layerGeomDef.Init(relativePoints, relativePoints, planeNormal);

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	tangents.Reset();
	vertexColors.Reset();
	if (layerGeomDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents))
	{
		// TODO: enable iterative mesh section updates when we can know that
		// the order of vertices did not change as a result of re-triangulation
		Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bCreateCollision);
	}
	Mesh->SetMaterial(0, material.EngineMaterial.Get());
}

void ADynamicMeshActor::SetupMetaPlaneGeometry(const TArray<FVector> &points, const FArchitecturalMaterial &material, float alpha, bool bRecreateMesh, const TArray<FPolyHole3D> *holes, bool bCreateCollision)
{
	FPlane pointsPlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(points, pointsPlane))
	{
		return;
	}

	FVector planeNormal(pointsPlane);
	Assembly = FBIMAssemblySpec();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset();


	TArray<FVector> relativePoints;
	Algo::Transform(points, relativePoints, [centroid](const FVector &worldPoint) { return worldPoint - centroid; });

	TArray<FVector> holeRelativePoints;
	TArray<FPolyHole3D> relativeHoles;
	if (holes != nullptr)
	{
		for (auto& hole : *holes)
		{
			holeRelativePoints.Reset();
			Algo::Transform(hole.Points, holeRelativePoints, [centroid](const FVector &worldPoint) { return worldPoint - centroid; });
			relativeHoles.Add(FPolyHole3D(holeRelativePoints));
		}
	}

	FLayerGeomDef &layerGeomDef = LayerGeometries.AddDefaulted_GetRef();
	layerGeomDef.Init(relativePoints, relativePoints, planeNormal, FVector::ZeroVector, &relativeHoles);

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	tangents.Reset();
	vertexColors.Reset();
	if (layerGeomDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents))
	{
		// TODO: enable iterative mesh section updates when we can know that
		// the order of vertices did not change as a result of re-triangulation
		Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bCreateCollision);
	}

	Mesh->SetMaterial(0, material.EngineMaterial.Get());
}

void ADynamicMeshActor::SetupRoomGeometry(const TArray<TArray<FVector>> &Polygons, const FArchitecturalMaterial &Material)
{
	Assembly = FBIMAssemblySpec();

	// TODO: calculate a point inside of the merged polygon formed from projecting the input 3D polygons to a 2D surface,
	// if none of them overlap.
	FVector centroid = FVector::ZeroVector;
	int32 totalNumPoints = 0;
	for (const TArray<FVector> &polyPoints : Polygons)
	{
		centroid += Algo::Accumulate(polyPoints, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; });
		totalNumPoints += polyPoints.Num();
	}
	centroid /= totalNumPoints;

	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset();

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	tangents.Reset();
	vertexColors.Reset();

	for (const TArray<FVector> &polyPoints : Polygons)
	{
		FPlane pointsPlane;
		if (!UModumateGeometryStatics::GetPlaneFromPoints(polyPoints, pointsPlane))
		{
			continue;
		}

		FVector planeNormal(pointsPlane);

		TArray<FVector> relativePoints;
		Algo::Transform(polyPoints, relativePoints, [centroid](const FVector &worldPoint) { return worldPoint - centroid; });

		FLayerGeomDef layerGeomDef(relativePoints, relativePoints, planeNormal);

		if (layerGeomDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents))
		{
			LayerGeometries.Add(MoveTemp(layerGeomDef));
		}
	}

	baseArea = 0.0f;
	// area of rooms is measured along xy plane
	FPlane unitXY = FPlane(FVector::ZeroVector, FVector::DownVector);
	for (int32 i = 0; i < triangles.Num(); i += 3)
	{
		const auto &p1 = vertices[triangles[i]], &p2 = vertices[triangles[i + 1]], &p3 = vertices[triangles[i + 2]];
		FVector p12D = FVector::PointPlaneProject(p1, unitXY);
		FVector p22D = FVector::PointPlaneProject(p2, unitXY);
		FVector p32D = FVector::PointPlaneProject(p3, unitXY);

		FVector triBaseDelta = p22D - p12D;
		float triBaseLen = triBaseDelta.Size();
		float triHeight = FMath::PointDistToLine(p32D, triBaseDelta, p12D);
		float triArea = 0.5f * triBaseLen * triHeight;
		baseArea += triArea;
	}

	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);

	CachedMIDs.SetNumZeroed(1);
	UModumateFunctionLibrary::SetMeshMaterial(Mesh, Material, 0, &CachedMIDs[0]);
}

bool ADynamicMeshActor::SetupStairPolys(const FVector &StairOrigin,
	const TArray<TArray<FVector>> &TreadPolys, const TArray<TArray<FVector>> &RiserPolys, const TArray<FVector> &RiserNormals,
	float StepThickness, const FArchitecturalMaterial &Material)
{
	SetActorLocation(StairOrigin);
	SetActorRotation(FQuat::Identity);

	vertices.Reset();
	triangles.Reset();
	normals.Reset();
	uv0.Reset();
	tangents.Reset();
	vertexColors.Reset();

	LayerGeometries.Reset();
	Assembly = FBIMAssemblySpec();

	TArray<FVector> layerPointsB;

	// Create a FLayerGeomDef for each tread, since it's just an extruded polygon
	for (const TArray<FVector> &treadPoly : TreadPolys)
	{
		FLayerGeomDef &treadLayerGeom = LayerGeometries.AddDefaulted_GetRef();

		layerPointsB.Reset(treadPoly.Num());
		Algo::Transform(treadPoly, layerPointsB, [StepThickness](const FVector &pointA) { return pointA - (StepThickness * FVector::UpVector); });

		treadLayerGeom.Init(treadPoly, layerPointsB, -FVector::UpVector);
		treadLayerGeom.TriangulateMesh(vertices, triangles, normals, uv0, tangents);
	}

	int32 numRisers = RiserPolys.Num();
	if (!ensure(numRisers == RiserNormals.Num()))
	{
		return false;
	}

	// Create a FLayerGeomDef for each riser, with the specified normals
	for (int32 riserIdx = 0; riserIdx < numRisers; ++riserIdx)
	{
		const TArray<FVector> &riserPoly = RiserPolys[riserIdx];
		const FVector &riserNormal = RiserNormals[riserIdx];

		FLayerGeomDef &riserLayerGeom = LayerGeometries.AddDefaulted_GetRef();

		layerPointsB.Reset(riserPoly.Num());
		Algo::Transform(riserPoly, layerPointsB, [StepThickness, riserNormal](const FVector &pointA) { return pointA + (StepThickness * riserNormal); });

		riserLayerGeom.Init(riserPoly, layerPointsB, riserNormal);
		riserLayerGeom.TriangulateMesh(vertices, triangles, normals, uv0, tangents);
	}

	// Finalize the mesh with a single material
	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);

	// TODO: respect an actual assembly with layers and multiple materials
	CachedMIDs.SetNumZeroed(1);
	UModumateFunctionLibrary::SetMeshMaterial(Mesh, Material, 0, &CachedMIDs[0]);

	return true;
}

bool ADynamicMeshActor::SetupStairPolys(const FVector& StairOrigin, const TArray<TArray<FVector>>& TreadPolys,
	const TArray<TArray<FVector>>& RiserPolys, const TArray<FVector>& RiserNormals,
	const TArray<FLayerGeomDef>& TreadLayers, const TArray<FLayerGeomDef>& RiserLayers, const FBIMAssemblySpec& AssemblySpec)
{
	Mesh->ClearAllMeshSections();
	MeshCap->ClearAllMeshSections();

	SetActorLocation(StairOrigin);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset();
	CachedMIDs.SetNum(TreadLayers.Num() + RiserLayers.Num());

	using FGeomLayers = TArray<FLayerGeomDef>;

	Assembly = AssemblySpec;

	// Create a FLayerGeomDef for each tread & riser layer
	int32 sectionIndex = 0;
	const int32 numTreadLayers = TreadLayers.Num();
	bool bProcessingRisers = false;
	TArray<FVector> stairPoints;
	for (const auto* stairElement: { &TreadLayers, &RiserLayers })
	{
		int32 materialIndex = 0;
		const TArray<TArray<FVector>>&  polys = bProcessingRisers ? RiserPolys : TreadPolys;
		for (const auto& inLayer: *stairElement)
		{
			vertices.Reset();
			triangles.Reset();
			normals.Reset();
			uv0.Reset();
			tangents.Reset();
			vertexColors.Reset();

			for (const TArray<FVector>& elementPoly: polys)
			{
				FVector stepDelta(elementPoly[0] - polys[0][0]);
				stairPoints = inLayer.OriginalPointsA;
				for (auto& point: stairPoints)
				{
					point += stepDelta;
				}

				FLayerGeomDef& newLayerGeom = LayerGeometries.Emplace_GetRef(stairPoints, inLayer.Thickness, inLayer.Normal);
				newLayerGeom.TriangulateMesh(vertices, triangles, normals, uv0, tangents);
			}

			Mesh->CreateMeshSection_LinearColor(sectionIndex, vertices, triangles, normals, uv0, vertexColors, tangents, true);

			const FArchitecturalMaterial* material = nullptr;
			if (bProcessingRisers)
			{
				if (ensureAlways(Assembly.RiserLayers[materialIndex].Modules.Num() > 0))
				{
					material =  &Assembly.RiserLayers[materialIndex].Modules[0].Material;
				}
			}
			else
			{
				if (ensureAlways(Assembly.TreadLayers[materialIndex].Modules.Num() > 0))
				{
					material = &Assembly.TreadLayers[materialIndex].Modules[0].Material;
				}
			}

			++materialIndex;

			if (ensureAlways(material != nullptr))
			{
				UModumateFunctionLibrary::SetMeshMaterial(Mesh, *material, sectionIndex, &CachedMIDs[sectionIndex]);
			}
			++sectionIndex;
		}

		bProcessingRisers = true;
	}

	return true;
}

void ADynamicMeshActor::DynamicMeshActorMoved(const FVector& newPosition)
{
}

void ADynamicMeshActor::DynamicMeshActorRotated(const FRotator& newRotation)
{
}

void ADynamicMeshActor::SetupProceduralLayers(int32 numProceduralLayers)
{
	const EComponentMobility::Type newMobility = bIsDynamic ? EComponentMobility::Movable : EComponentMobility::Static;

	// Potentially create more procedural meshes for the desired number of layers
	if (numProceduralLayers > ProceduralSubLayers.Num())
	{
		int32 numSubLayerToAdd = numProceduralLayers - ProceduralSubLayers.Num();
		for (int32 i = 1; i < numSubLayerToAdd + 1; i++)
		{
			UProceduralMeshComponent* newProceduralMesh = NewObject<UProceduralMeshComponent>(this);
			newProceduralMesh->bUseAsyncCooking = true;
			newProceduralMesh->SetCollisionObjectType(Mesh->GetCollisionObjectType());
			newProceduralMesh->SetupAttachment(this->GetRootComponent());
			newProceduralMesh->SetMobility(newMobility);
			newProceduralMesh->RegisterComponent();
			ProceduralSubLayers.Add(newProceduralMesh);

			UProceduralMeshComponent* newProceduralMeshCap = NewObject<UProceduralMeshComponent>(this);
			newProceduralMeshCap->bUseAsyncCooking = true;
			newProceduralMeshCap->SetCollisionObjectType(Mesh->GetCollisionObjectType());
			newProceduralMeshCap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			newProceduralMeshCap->SetCastShadow(false);
			newProceduralMeshCap->SetupAttachment(this->GetRootComponent());
			newProceduralMeshCap->SetMobility(newMobility);
			newProceduralMeshCap->RegisterComponent();
			ProceduralSubLayerCaps.Add(newProceduralMeshCap);
		}
	}
	// Clear mesh of all procedural mesh
	for (UProceduralMeshComponent* proceduralSubLayer : ProceduralSubLayers)
	{
		proceduralSubLayer->ClearAllMeshSections();
	}
	for (UProceduralMeshComponent* proceduralSubLayerCap : ProceduralSubLayerCaps)
	{
		proceduralSubLayerCap->ClearAllMeshSections();
	}
}

void ADynamicMeshActor::ClearProceduralLayers()
{
	for (int32 layerIdx = 0; layerIdx < ProceduralSubLayers.Num(); layerIdx++)
	{
		ProceduralSubLayers[layerIdx]->DestroyComponent();
	}
	ProceduralSubLayers.Reset();

	for (int32 layerIdx = 0; layerIdx < ProceduralSubLayerCaps.Num(); layerIdx++)
	{
		ProceduralSubLayerCaps[layerIdx]->DestroyComponent();
	}
	ProceduralSubLayerCaps.Reset();
}

void ADynamicMeshActor::ClearProceduralMesh()
{
	Mesh->ClearAllMeshSections();
	MeshCap->ClearAllMeshSections();
}

bool ADynamicMeshActor::SetupExtrudedPolyGeometry(const FBIMAssemblySpec& InAssembly, const FVector& InStartPoint, const FVector& InEndPoint,
	const FVector& ObjNormal, const FVector& ObjUp, const FDimensionOffset& OffsetNormal, const FDimensionOffset& OffsetUp,
	const FVector2D& Extensions, const FVector& InFlipSigns, bool bRecreateSection, bool bCreateCollision)
{
	const FSimplePolygon *polyProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&InAssembly, polyProfile))
	{
		return false;
	}

	Assembly = InAssembly;

	FVector midPoint = 0.5f * (InStartPoint + InEndPoint);
	FVector baseStartPoint = InStartPoint - midPoint;
	FVector baseEndPoint = InEndPoint - midPoint;
	FVector baseExtrusionDelta = baseEndPoint - baseStartPoint;
	float baseExtrusionLength = baseExtrusionDelta.Size();
	if (!ensure(!FMath::IsNearlyZero(baseExtrusionLength)))
	{
		return false;
	}
	FVector extrusionDir = baseExtrusionDelta / baseExtrusionLength;
	SetActorLocation(midPoint);
	SetActorRotation(FQuat::Identity);

	TArray<FVector2D> profilePoints;
	FBox2D profileExtents;
	FVector2D profileFlip(InFlipSigns.X, InFlipSigns.Y);
	if (!UModumateObjectStatics::GetExtrusionProfilePoints(InAssembly, OffsetNormal, OffsetUp, profileFlip, profilePoints, profileExtents))
	{
		return false;
	}

	const TArray<int32>& profileTris = polyProfile->Triangles;

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	vertexColors.Reset();

	// TODO: determine the best format for providing an arbitrary angle to use to offset each end of the mesh,
	// or potentially specify entire custom end cap geometry.
	auto offsetPoint = [extrusionDir, ObjNormal, ObjUp, profileExtents, Extensions]
	(const FVector &worldPoint, const FVector2D &polyPoint, bool bAtStart)
	{
		FVector2D profileExtentsSize = profileExtents.GetSize();
		FVector2D pointRelative = polyPoint - profileExtents.Min;
		FVector2D pointPCT = (profileExtentsSize.GetMin() > 0.0f) ? (pointRelative / profileExtentsSize) : FVector2D::ZeroVector;
		float lengthExtension = bAtStart ? -Extensions.X : Extensions.Y;

		return worldPoint + (lengthExtension * extrusionDir) + (polyPoint.X * ObjNormal) + (polyPoint.Y * ObjUp);
	};

	static constexpr float uvScale = 0.01f;
	const FVector2D uvFactor(uvScale * FMath::Sign(InFlipSigns.Z), uvScale);
	auto fixExtrudedTriUVs = [this, uvFactor](const FVector2D &uv1, const FVector2D &uv2, const FVector2D &uv3)
	{
		int32 numUVs = uv0.Num();
		uv0[numUVs - 3] = uv1 * uvFactor;
		uv0[numUVs - 2] = uv2 * uvFactor;
		uv0[numUVs - 1] = uv3 * uvFactor;
	};

	// For each edge along the profile, create triangles that run along the length of the trim,
	// based on the extents of inP1 and inP2.
	int32 numPoints = profilePoints.Num();
	FVector2D uvOffset(ForceInitToZero);
	float polyPerimeter = 0.0f;
	for (int32 p1Idx = 0; p1Idx < numPoints; ++p1Idx)
	{
		int32 p2Idx = (p1Idx + 1) % numPoints;
		FVector2D p1 = profilePoints[p1Idx];
		FVector2D p2 = profilePoints[p2Idx];
		FVector2D edgeDelta = p2 - p1;
		float edgeLength = edgeDelta.Size();

		FVector quadP1 = offsetPoint(baseStartPoint, p1, true);
		FVector quadP2 = offsetPoint(baseEndPoint, p1, false);
		FVector quadP3 = offsetPoint(baseStartPoint, p2, true);
		FVector quadP4 = offsetPoint(baseEndPoint, p2, false);

		FVector2D uv1(uvOffset.X, uvOffset.Y + polyPerimeter);
		FVector2D uv2(uvOffset.X + baseExtrusionLength, uvOffset.Y + polyPerimeter);
		FVector2D uv3(uvOffset.X, uvOffset.Y + polyPerimeter + edgeLength);
		FVector2D uv4(uvOffset.X + baseExtrusionLength, uvOffset.Y + polyPerimeter + edgeLength);

		MakeTriangle(quadP1, quadP2, quadP3, 0, -1, 0.0f);
		fixExtrudedTriUVs(uv1, uv2, uv3);
		MakeTriangle(quadP1, quadP3, quadP2, 0, -1, 0.0f);
		fixExtrudedTriUVs(uv1, uv3, uv2);
		MakeTriangle(quadP2, quadP4, quadP3, 0, -1, 0.0f);
		fixExtrudedTriUVs(uv2, uv4, uv3);
		MakeTriangle(quadP2, quadP3, quadP4, 0, -1, 0.0f);
		fixExtrudedTriUVs(uv2, uv3, uv4);

		polyPerimeter += edgeLength;
	}

	int32 numTriIndices = profileTris.Num();
	if ((numTriIndices > 0) && ((numTriIndices % 3) == 0))
	{
		int32 numTris = numTriIndices / 3;
		for (int32 triIdx = 0; triIdx < numTris; ++triIdx)
		{
			int32 triIdxA = profileTris[(3 * triIdx) + 0];
			int32 triIdxB = profileTris[(3 * triIdx) + 1];
			int32 triIdxC = profileTris[(3 * triIdx) + 2];

			FVector2D polyPointA = profilePoints[triIdxA];
			FVector2D polyPointB = profilePoints[triIdxB];
			FVector2D polyPointC = profilePoints[triIdxC];

			FVector meshPointStartA = offsetPoint(baseStartPoint, polyPointA, true);
			FVector meshPointStartB = offsetPoint(baseStartPoint, polyPointB, true);
			FVector meshPointStartC = offsetPoint(baseStartPoint, polyPointC, true);
			MakeTriangle(meshPointStartA, meshPointStartB, meshPointStartC, 0, -1, 0.0f);
			MakeTriangle(meshPointStartA, meshPointStartC, meshPointStartB, 0, -1, 0.0f);

			FVector meshPointEndA = offsetPoint(baseEndPoint, polyPointA, false);
			FVector meshPointEndB = offsetPoint(baseEndPoint, polyPointB, false);
			FVector meshPointEndC = offsetPoint(baseEndPoint, polyPointC, false);
			MakeTriangle(meshPointEndA, meshPointEndB, meshPointEndC, 0, -1, 0.0f);
			MakeTriangle(meshPointEndA, meshPointEndC, meshPointEndB, 0, -1, 0.0f);
		}
	}

	if (bRecreateSection)
	{
		Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bCreateCollision);
	}
	else
	{
		Mesh->UpdateMeshSection_LinearColor(0, vertices, normals, uv0, vertexColors, tangents);
	}

	// Update the material (and its parameters, if any)
	CachedMIDs.SetNumZeroed(1);
	UModumateFunctionLibrary::SetMeshMaterial(Mesh, Assembly.Extrusions[0].Material, 0, &CachedMIDs[0]);

	return true;
}

void ADynamicMeshActor::SetupMasksGeometry(const TArray<TArray<FVector>> &Polygons, const FPlane &Plane, const FVector &Origin, const FVector &AxisX, const FVector &AxisY)
{
	SetupProceduralLayers(Polygons.Num());

	SetActorLocation(FVector::ZeroVector);
	int32 idx = 0;
	for (auto& perimeter : Polygons)
	{
		TArray<FVector2D> maskVertices2D;
		for (auto& vertex : perimeter)
		{
			// 1cm off of plane face
			FVector planeOffset = FVector(Plane).GetSafeNormal();
			maskVertices2D.Add(UModumateGeometryStatics::ProjectPoint2D(vertex, AxisX, AxisY, Origin + planeOffset));
		}

		TArray<FVector2D> vertices2D;
		TArray<int32> tris2D;
		TArray<FPolyHole2D> validHoles; // empty

		UModumateGeometryStatics::TriangulateVerticesGTE(maskVertices2D, validHoles, tris2D, &vertices2D);

		TArray<FVector> maskVertices;
		TArray<FVector> maskNormals;
		TArray<FVector2D> maskUVs;
		TArray<FLinearColor> maskVertexColors;
		TArray<FProcMeshTangent> maskTangents;

		for (auto& vertex : vertices2D)
		{
			FVector vertex3D = Origin + (AxisX * vertex.X) + (AxisY * vertex.Y);
			maskVertices.Add(vertex3D);

			normals.Add(FVector(Plane));
			maskUVs.Add(FVector2D());

			maskTangents.Add(FProcMeshTangent(0, 1, 0));

			maskVertexColors.Add(FLinearColor(1, 1, 1, 1.0));
		}

		ProceduralSubLayers[idx]->CreateMeshSection_LinearColor(0, maskVertices, tris2D, maskNormals, maskUVs, maskVertexColors, maskTangents, false);

		ProceduralSubLayers[idx]->bRenderCustomDepth = true;

		// TODO: aggregate all of the uses of CustomDepthStencilValue in one place so that it is obvious what values are already being used
		// also, this hard-coded value is duplicated in the shader
		ProceduralSubLayers[idx]->CustomDepthStencilValue = 4;

		idx++;
	}
}

void ADynamicMeshActor::SetupCapGeometry()
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller)
	{
		return;
	}
	ClearCapGeometry();

	TArray<UStaticMeshComponent*> emptyStaticMeshes;
	if (ProceduralSubLayers.Num() > 0)
	{
		for (int32 layerIdx = 0; layerIdx < ProceduralSubLayers.Num(); ++layerIdx)
		{
			UModumateGeometryStatics::CreateProcMeshCapFromPlane(ProceduralSubLayerCaps[layerIdx], 
				TArray<UProceduralMeshComponent*>{ ProceduralSubLayers[layerIdx] }, emptyStaticMeshes,
				controller->GetCurrentCullingPlane(), 0, ProceduralSubLayers[layerIdx]->GetMaterial(0));
		}
	}
	else
	{
		UModumateGeometryStatics::CreateProcMeshCapFromPlane(MeshCap, 
			TArray<UProceduralMeshComponent*>{ Mesh }, emptyStaticMeshes,
			controller->GetCurrentCullingPlane(), 0, Mesh->GetMaterial(0));
	}

}

void ADynamicMeshActor::ClearCapGeometry()
{
	for (UProceduralMeshComponent* proceduralSubLayerCap : ProceduralSubLayerCaps)
	{
		proceduralSubLayerCap->ClearAllMeshSections();
	}
	MeshCap->ClearAllMeshSections();
}

void ADynamicMeshActor::UpdateLayerMaterialsFromAssembly()
{
	// Only update relevant procedural layers
	TArray<UProceduralMeshComponent*> updateLayers;
	for (int32 layerIdx = 0; layerIdx < Assembly.Layers.Num(); ++layerIdx)
	{
		if (ensureAlways(ProceduralSubLayers[layerIdx]))
		{
			updateLayers.Add(ProceduralSubLayers[layerIdx]);
		}
	}
	UModumateFunctionLibrary::UpdateMaterialsFromAssembly(updateLayers, Assembly, TilingMaterials, MasterPBRMaterial,
		&CachedMIDs);
}

bool ADynamicMeshActor::SetPlacementError(FName errorTag, bool bIsError)
{
	bool bChanged = false;

	if (bIsError)
	{
		PlacementErrors.Add(errorTag, &bChanged);
	}
	else
	{
		bChanged = (PlacementErrors.Remove(errorTag) > 0);
	}

	if (bChanged)
	{
		// Update the materials of the mesh, in case they don't get updated by whatever is changing the error state
		int32 numLayers = FMath::Min(Assembly.Layers.Num(), ProceduralSubLayers.Num());
		CachedMIDs.SetNumZeroed(numLayers);
		for (int32 i = 0; i < numLayers; i++)
		{
			UProceduralMeshComponent *procMeshComp = ProceduralSubLayers[i];

			if (PlacementErrors.Num() > 0)
			{
				procMeshComp->SetMaterial(0, PlacementWarningMaterial);
			}
			else
			{
				const FBIMLayerSpec& layerData = Assembly.Layers[i];
				if (ensureAlways(layerData.Modules.Num() > 0))
				{
					UModumateFunctionLibrary::SetMeshMaterial(procMeshComp, layerData.Modules[0].Material, 0, &CachedMIDs[i]);
				}
			}
		}
	}

	return bChanged;
}

void ADynamicMeshActor::SetIsDynamic(bool DynamicStatus)
{
	if (bIsDynamic != DynamicStatus)
	{
		bIsDynamic = DynamicStatus;
		EComponentMobility::Type newMobility = DynamicStatus ? EComponentMobility::Movable : EComponentMobility::Static;
		if (Mesh)
		{
			Mesh->SetMobility(newMobility);
		}

		for (UProceduralMeshComponent* mesh : ProceduralSubLayers)
		{
			mesh->SetMobility(newMobility);
		}
	}
}
