// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/DynamicMeshActor.h"

#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Algo/Transform.h"
#include "Algo/Accumulate.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

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
}

// Setup Blueprint for access in cpp
ADynamicCursor::ADynamicCursor()
{

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

void ADynamicMeshActor::CacheTriangleNormals(const TArray<FVector> &points, int32 cp1, int32 cp2, int32 cp3)
{
	const FVector &p1 = points[cp1];
	const FVector &p2 = points[cp2];
	const FVector &p3 = points[cp3];
	int32 numPoints = points.Num();

	FVector centroid = (p1 + p2 + p3) / 3.0f;
	FPlane trianglePlane(p1, p2, p3);

	FVector cpPositions[] = { p1, p2, p3 };
	int32 cpIndices[] = { cp1, cp2, cp3 };
	for (int i = 0; i < 3; ++i)
	{
		int32 cpA = cpIndices[i];
		int32 cpB = cpIndices[(i + 1) % 3];
		const FVector &cpAPos = cpPositions[i];
		const FVector &cpBPos = cpPositions[(i + 1) % 3];

		TPair<int32, int32> edgeIndicesPair(FMath::Min(cpA, cpB), FMath::Max(cpA, cpB));

		if ((cpA == ((cpB + 1) % numPoints)) || (cpB == ((cpA + 1) % numPoints)))
		{
			FVector edgeNormal = trianglePlane ^ (cpBPos - cpAPos).GetSafeNormal();
			if ((edgeNormal | (centroid - cpAPos).GetSafeNormal()) < 0.0f)
			{
				edgeNormal *= -1.0f;
			}
			exteriorTriangleNormals.Add(edgeIndicesPair, edgeNormal);
		}
	}
}

void ADynamicMeshActor::SetupRailGeometry(const TArray<FVector> &points, float height)
{
	if (points.Num() == 0)
	{
		return;
	}
	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	vertexColors.Reset();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();

	SetActorLocation(centroid);

	for (size_t i = 0; i < points.Num() - 1; i += 2)
	{
		MakeTriangle(points[i] - centroid, points[i + 1] - centroid, points[i + 1] + FVector(0, 0, height) - centroid, 0, 0);
		MakeTriangle(points[i] - centroid, points[i + 1] + FVector(0, 0, height) - centroid, points[i + 1] - centroid, 0, 0);

		MakeTriangle(points[i] - centroid, points[i + 1] + FVector(0, 0, height) - centroid, points[i] + FVector(0, 0, height) - centroid, 0, 1);
		MakeTriangle(points[i] - centroid, points[i] + FVector(0, 0, height) - centroid, points[i + 1] + FVector(0, 0, height) - centroid, 0, 1);
	}

	UMaterialInterface *greenMaterial = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->GreenMaterial;

	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);
	Mesh->SetMaterial(0, greenMaterial);
}

void ADynamicMeshActor::UpdateRailGeometry(const TArray<FVector> &points, float height)
{
	SetupRailGeometry(points, height);
}

void ADynamicMeshActor::UpdateHolesFromActors()
{
	Holes3D.Reset();
	FVector actorLocation = GetActorLocation();

	for (auto *holeActor : HoleActors)
	{
		FPolyHole3D &hole = Holes3D.AddDefaulted_GetRef();
		hole.Points = UModumateObjectStatics::GetMoiActorHoleVertsWorldLocations(holeActor);

		// Hole points are calculate in world coordinates, but for consistency with cached layer
		// definitions and vertices, they should be relative to the actor location (centroid).
		for (FVector &holePoint : hole.Points)
		{
			holePoint -= actorLocation;
		}
	}
}

bool ADynamicMeshActor::CreateBasicLayerDefs(const TArray<FVector> &PlanePoints, const FVector &PlaneNormal,
	const FModumateObjectAssembly &InAssembly, float PlaneOffsetPCT,
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

	FVector centroid = Algo::Accumulate(fixedPlanePoints, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / numPoints;
	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	FVector layersNormal = PlaneNormal.IsZero() ? FVector(planeGeom) : PlaneNormal;
	Assembly = InAssembly;
	LayerGeometries.SetNum(numLayers);
	float totalThickness = Assembly.CalculateThickness().AsWorldCentimeters();
	float totalOffset = totalThickness * -FMath::Clamp(PlaneOffsetPCT, 0.0f, 1.0f);
	UpdateHolesFromActors();

	FVector commonAxisX = AxisX;
	float accumThickness = 0.0f;
	for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
	{
		const FModumateObjectAssemblyLayer &layer = Assembly.Layers[layerIdx];
		float layerThickness = layer.Thickness.AsWorldCentimeters();

		layerPointsA.Reset(numPoints);
		layerPointsB.Reset(numPoints);
		for (const FVector &planePoint : fixedPlanePoints)
		{
			layerPointsA.Add(planePoint + (totalOffset + accumThickness) * layersNormal - centroid);
			layerPointsB.Add(planePoint + (totalOffset + accumThickness + layerThickness) * layersNormal - centroid);
		}

		FLayerGeomDef &layerGeomDef = LayerGeometries[layerIdx];
		layerGeomDef.Init(layerPointsA, layerPointsB, layersNormal, commonAxisX, &Holes3D);
		if (!commonAxisX.IsNormalized())
		{
			commonAxisX = layerGeomDef.AxisX;
		}

		accumThickness += layerThickness;
	}

	return true;
}

bool ADynamicMeshActor::UpdatePlaneHostedMesh(bool bRecreateMesh, bool bUpdateCollision, bool bEnableCollision, const FVector &InUVAnchor, float UVRotOffset)
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

	// Only update collision for the mesh if there aren't any preview hole actors,
	// since otherwise the holes will interfere with the tool's ability to raycast against where the hole might be.
	// TODO: if we want to potentially select plane hosted objects by both their convex geometry and their complex geometry,
	// then we may want to explicitly create simple collision convex shapes, or create additional collision meshes
	// for the spaces where the holes would be.
	if (PreviewHoleActors.Num() != 0)
	{
		bUpdateCollision = false;
	}

	UVAnchor = InUVAnchor;

	for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
	{
		const FLayerGeomDef &layerGeomDef = LayerGeometries[layerIdx];

		vertices.Reset();
		normals.Reset();
		triangles.Reset();
		uv0.Reset();
		tangents.Reset();
		vertexColors.Reset();

		if (layerGeomDef.TriangulateMesh(vertices, triangles, normals, uv0, tangents, UVAnchor, UVRotOffset))
		{
			UProceduralMeshComponent *procMeshComp = ProceduralSubLayers[layerIdx];

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
		}
	}

	UpdateLayerMaterialsFromAssembly();

	return true;
}

void ADynamicMeshActor::AddPrismGeometry(const TArray<FVector> &points, const FVector &centroid, float height, const FVector ExtrusionDir,
	float uvRotOffset, bool bMakeBottom, bool bMakeTop, bool bMakeSides, const TArray<FPolyHole3D> *holes)
{
	int32 numPoints = points.Num();
	if (numPoints < 3)
	{
		return;
	}

	FVector extrusionDelta = height * ExtrusionDir;

	TArray<FVector> perimeterPoints;

	Algo::Transform(points,perimeterPoints,[centroid](const FVector &p) {return p - centroid; });

	TArray<int32> tris, perimeterVertexHoleIndices;
	TArray<FVector> bottomPoints;
	if (holes)
	{
		auto projectPoint2D = [](const FVector &point) { return FVector2D(point); };

		TArray<FVector2D> triangulatedPoints2D;
		TArray<FVector2D> points2D;
		TArray<FPolyHole2D> holes2D;

		Algo::Transform(points, points2D, projectPoint2D);
		Algo::Transform(*holes, holes2D, [projectPoint2D](const FPolyHole3D &hole3D) {
			FPolyHole2D hole2D;
			Algo::Transform(hole3D.Points, hole2D.Points, projectPoint2D);
			return hole2D;
		});

		TArray<FVector2D> perimeter2D;
		TArray<bool> mergedHoles;
		if (UModumateGeometryStatics::TriangulateVerticesPoly2Tri(points2D, holes2D,
			triangulatedPoints2D, tris, perimeter2D, mergedHoles, perimeterVertexHoleIndices))
		{
			Algo::Transform(
				triangulatedPoints2D, 
				bottomPoints,
				[centroid](const FVector2D &triangulatedPoint2D)
				{ 
					return FVector(triangulatedPoint2D, centroid.Z) - centroid; 
				});
		}
	}
	else
	{
		bottomPoints = perimeterPoints;
		UModumateFunctionLibrary::CalculatePolygonTriangle(bottomPoints, tris);
	}

	TArray<FVector> topPoints;
	Algo::Transform(bottomPoints, topPoints, [extrusionDelta](const FVector &p) {return p + extrusionDelta; });

	// Approximate the direction of the polygon's OBB by finding the longest edge, so we can rotate UVs appropriately.
	// Ideally we would find the convex polygon and then use rotating calipers to find the minimum OBB, but that's fancier than we need.
	float endFacesUVRot = 0.0f;
	float longestEdgeLength = 0.0f;
	FVector longestEdgeDir = FVector::ZeroVector;
	for (int32 i1 = 0; i1 < numPoints; ++i1)
	{
		int32 i2 = (i1 + 1) % numPoints;
		const FVector p1 = points[i1];
		const FVector p2 = points[i2];
		float edgeLength = FVector::Dist(p1, p2);
		if (edgeLength > longestEdgeLength)
		{
			longestEdgeLength = edgeLength;
			longestEdgeDir = (p2 - p1) / edgeLength;
		}
	}

	if (longestEdgeLength > 0.0f)
	{
		endFacesUVRot = 90.0f + uvRotOffset + FMath::RadiansToDegrees(FMath::Atan2(longestEdgeDir.X, longestEdgeDir.Y));
	}

	for (int32 i = 0; i < tris.Num(); i += 3)
	{
		CacheTriangleNormals(topPoints, tris[i], tris[i + 1], tris[i + 2]);

		if (bMakeTop)
		{
			MakeTriangle(topPoints[tris[i]], topPoints[tris[i + 1]], topPoints[tris[i + 2]], 0, -1, endFacesUVRot);
			MakeTriangle(topPoints[tris[i + 2]], topPoints[tris[i + 1]], topPoints[tris[i]], 0, -1, endFacesUVRot);
		}

		if (bMakeBottom)
		{
			MakeTriangle(bottomPoints[tris[i]], bottomPoints[tris[i + 1]], bottomPoints[tris[i + 2]], 0, -1, endFacesUVRot);
			MakeTriangle(bottomPoints[tris[i + 2]], bottomPoints[tris[i + 1]], bottomPoints[tris[i]], 0, -1, endFacesUVRot);
		}

		const auto &p1 = bottomPoints[tris[i]], &p2 = bottomPoints[tris[i + 1]], &p3 = bottomPoints[tris[i + 2]];
		FVector triBaseDelta = p2 - p1;
		float triBaseLen = triBaseDelta.Size();
		float triHeight = FMath::PointDistToLine(p3, triBaseDelta, p1);
		float triArea = 0.5f * triBaseLen * triHeight;
		baseArea += triArea;

		float triPrismVolume = triArea * height;
		meshVolume += triPrismVolume;
	}

	if (bMakeSides)
	{
		float sideTriUVRot = 90.0f + uvRotOffset;

		auto makeSideTriangles = [this, sideTriUVRot, extrusionDelta](const FVector &p1, const FVector &p2)
		{
			FVector p3 = p1 + extrusionDelta;
			FVector p4 = p2 + extrusionDelta;

			MakeTriangle(p1, p2, p4, 0, -1, sideTriUVRot);
			MakeTriangle(p1, p4, p3, 0, -1, sideTriUVRot);

			MakeTriangle(p2, p1, p4, 0, -1, sideTriUVRot);
			MakeTriangle(p4, p1, p3, 0, -1, sideTriUVRot);
		};

		for (int32 pointIdx = 0; pointIdx < numPoints; pointIdx++)
		{
			FVector &p1 = perimeterPoints[pointIdx];
			FVector &p2 = perimeterPoints[(pointIdx + 1) % numPoints];

			makeSideTriangles(p1, p2);
		}

		if (holes)
		{
			for (int32 holeIdx = 0; holeIdx < holes->Num(); ++holeIdx)
			{
				const TArray<FVector> &hole = (*holes)[holeIdx].Points;
				int32 numHolePoints = hole.Num();
				for (int32 pointIdx = 0; pointIdx < numHolePoints; ++pointIdx)
				{
					FVector p1 = hole[pointIdx] - centroid;
					FVector p2 = hole[(pointIdx + 1) % numHolePoints] - centroid;

					makeSideTriangles(p1, p2);
				}
			}
		}
	}
}

void ADynamicMeshActor::SetupPrismGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &materialData,
	float uvRotOffset, bool bMakeSides, const TArray<FPolyHole3D> *holes)
{
	int32 numPoints = points.Num();
	if (numPoints < 3)
	{
		return;
	}

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	vertexColors.Reset();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	SetActorLocation(centroid);

	// reset area and volume calculations
	baseArea = meshVolume = 0.0f;

	FPlane pointsPlane(points[0], points[1], points[2]);
	FVector extrusionDir = pointsPlane;

	AddPrismGeometry(points, centroid, height, extrusionDir, uvRotOffset, true, true, bMakeSides, holes);

	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);

	CachedMIDs.SetNumZeroed(1);
	UModumateFunctionLibrary::SetMeshMaterial(Mesh, materialData, 0, &CachedMIDs[0]);
}

void ADynamicMeshActor::UpdatePrismGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &materialData,
	float uvRotOffset, bool bMakeSides, const TArray<FPolyHole3D> *holes)
{
	return SetupPrismGeometry(points, height, materialData, uvRotOffset, bMakeSides, holes);
}

void ADynamicMeshActor::SetupCabinetGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &materialData, const FVector2D &toeKickDimensions, int32 frontIdx1, float uvRotOffset)
{
	int32 numPoints = points.Num();
	if (numPoints < 3)
	{
		return;
	}

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	vertexColors.Reset();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	SetActorLocation(centroid);

	FPlane pointsPlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(points, pointsPlane))
	{
		// TODO: this function should return a boolean
		return;// false
	}
	FVector extrusionDir = pointsPlane;
	if (FVector::Parallel(pointsPlane, FVector::UpVector))
	{
		extrusionDir = FVector::UpVector;
	}

	// reset area and volume calculations
	baseArea = meshVolume = 0.0f;

	// if there's no toe kick, then just make a prism
	if ((toeKickDimensions.X == 0.0f) || (toeKickDimensions.Y == 0.0f) || (numPoints == 3) || (frontIdx1 < 0))
	{
		AddPrismGeometry(points, centroid, height, extrusionDir, uvRotOffset, true, true);
	}
	// otherwise, we need to make two prisms
	else
	{
		// first, make the upper box on top of the base
		float boxHeight = height - toeKickDimensions.Y;

		bool bCoincident = FVector::Coincident(FVector(pointsPlane), FVector::UpVector);
		FVector baseHeightDelta = extrusionDir * toeKickDimensions.Y;

		TArray<FVector> boxBottomPoints;
		Algo::Transform(points, boxBottomPoints, [baseHeightDelta](const FVector &p) {return p + baseHeightDelta; });
		AddPrismGeometry(boxBottomPoints, centroid, boxHeight, extrusionDir, uvRotOffset, true, true);

		// now, make the base of the cabinet
		TArray<FVector> basePoints = points;

		// figure out the edge for the chosen front face of the cabinet
		int32 frontIdx2 = (frontIdx1 + 1) % numPoints;
		int32 nextIdx1 = (frontIdx1 + numPoints - 1) % numPoints;
		int32 nextIdx2 = (frontIdx2 + 1) % numPoints;
		FVector &frontEdgeP1 = basePoints[frontIdx1];
		FVector &frontEdgeP2 = basePoints[frontIdx2];
		const FVector &frontNextP1 = basePoints[nextIdx1];
		const FVector &frontNextP2 = basePoints[nextIdx2];

		FVector frontEdgeDelta = frontEdgeP2 - frontEdgeP1;
		float frontEdgeLen = frontEdgeDelta.Size();

		if (!FMath::IsNearlyZero(frontEdgeLen))
		{
			FVector frontEdgeDir = frontEdgeDelta / frontEdgeLen;
			FVector frontFaceNormal = extrusionDir ^ frontEdgeDir * (bCoincident ? 1.0f : -1.0f);
			FVector depthOffset = toeKickDimensions.X * frontFaceNormal;
			FVector nextEdgeDir1 = (frontEdgeP1 - frontNextP1).GetSafeNormal();
			FVector nextEdgeDir2 = (frontEdgeP2 - frontNextP2).GetSafeNormal();

			// retract the front face vertices based on the intersections of the retracted edge against its neighboring edges
			FVector frontEdgeP1Cut = FMath::RayPlaneIntersection(frontEdgeP2 + depthOffset, -frontEdgeDir,
				FPlane(frontNextP1, extrusionDir ^ nextEdgeDir1));
			FVector frontEdgeP2Cut = FMath::RayPlaneIntersection(frontEdgeP1 + depthOffset, frontEdgeDir,
				FPlane(frontNextP2, extrusionDir ^ nextEdgeDir2));

			frontEdgeP1 = frontEdgeP1Cut;
			frontEdgeP2 = frontEdgeP2Cut;

			AddPrismGeometry(basePoints, centroid, toeKickDimensions.Y, extrusionDir, uvRotOffset, true, false);
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, true);

	CachedMIDs.SetNumZeroed(1);
	UModumateFunctionLibrary::SetMeshMaterial(Mesh, materialData, 0, &CachedMIDs[0]);
}

void ADynamicMeshActor::UpdateCabinetGeometry(const TArray<FVector> &points, float height, const FArchitecturalMaterial &materialData, const FVector2D &toeKickDimensions, int32 frontIndexStart, float uvRotOffset)
{
	return SetupCabinetGeometry(points, height, materialData, toeKickDimensions, frontIndexStart, uvRotOffset);
}

void ADynamicMeshActor::SetupFlatPolyGeometry(const TArray<FVector> &points, const FModumateObjectAssembly &assembly)
{
	Assembly = assembly;
	TArray<FModumateObjectAssemblyLayer> orderedFAL;
	for (int32 i = 0; i < Assembly.Layers.Num(); ++i)
	{
		orderedFAL.Add(Assembly.Layers[i]);
	}

	float thickness = Algo::TransformAccumulate(orderedFAL,[](const FModumateObjectAssemblyLayer &layer){return layer.Thickness.AsWorldCentimeters();},0.0f);

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	vertices.Reset();
	Algo::Transform(points, vertices, [centroid](const FVector &p) { return p - centroid; });
	TArray<FVector> originalVertices = vertices;

	SetActorLocation(centroid);

	// Begin constructing sublayers for floor
	UModumateFunctionLibrary::GetFloorAssemblyLayerControlPoints(originalVertices, Assembly.Layers, FloorAssemblyLayerControlPoints);
	TArray<FVector> topControlPoints, bottomControlPoints, layerVerts, layerNormals;
	TArray<int32> layerTris;
	TArray<int32> layerTopTris;
	TArray<FVector2D> layerUVs;

	// If layers are more than procedural mesh components, add more procedural meshes
	SetupProceduralLayers(FloorAssemblyLayerControlPoints.Num());

	vertexColors.Reset();
	tangents.Reset();

	// Generate procedural mesh based on parameters from FloorAssemblyLayerControlPoints
	int32 numLayers = FloorAssemblyLayerControlPoints.Num();
	CachedMIDs.SetNumZeroed(numLayers);
	for (int32 i = 0; i < numLayers; i++)
	{
		topControlPoints = FloorAssemblyLayerControlPoints[i].TopLayerControlPoints;
		bottomControlPoints = FloorAssemblyLayerControlPoints[i].BottomLayerControlPoints;
		UModumateFunctionLibrary::CalculateFloorParam(centroid, UVFloorAnchors, TopUVFloorAnchor, TopUVFloorRot, topControlPoints, bottomControlPoints, layerVerts, layerTris, layerNormals, layerUVs, layerTopTris);

		// Create Mesh Sections
		UProceduralMeshComponent *procMeshComp = ProceduralSubLayers[i];
		procMeshComp->CreateMeshSection_LinearColor(0, layerVerts, layerTris, layerNormals, layerUVs, vertexColors, tangents, true);

		UModumateFunctionLibrary::SetMeshMaterial(procMeshComp, orderedFAL[i].Material, 0, &CachedMIDs[i]);
	}

	// Setup UV Anchor
	TArray<FVector> newCPNext;
	for (int32 i = 0; i < points.Num(); i++)
	{
		int32 nextId = UModumateFunctionLibrary::LoopArrayGetNextIndex(i, points.Num());
		newCPNext.Add(points[nextId]);
	}

	for (int32 i = 0; i < AdjustHandleFloor.Num(); i++)
	{
		int32 currentHandleID = AdjustHandleFloor[i];
		int32 nextHandleID = UModumateFunctionLibrary::LoopArrayGetPreviousIndex(currentHandleID, UVFloorAnchors.Num());
		UVFloorAnchors[currentHandleID] = UModumateFunctionLibrary::WallUpdateUVAnchor(UVFloorAnchors[currentHandleID], 0, points[currentHandleID], newCPNext[currentHandleID], OldFloorCP1[currentHandleID], OldFloorCP2[currentHandleID]);
		UVFloorAnchors[nextHandleID] = UModumateFunctionLibrary::WallUpdateUVAnchor(UVFloorAnchors[nextHandleID], 1, points[nextHandleID], newCPNext[nextHandleID], OldFloorCP1[nextHandleID], OldFloorCP2[nextHandleID]);
	}
	OldFloorCP1 = points;
	OldFloorCP2 = newCPNext;

	UpdateLayerMaterialsFromAssembly();
}

void ADynamicMeshActor::UpdateFlatPolyGeometry(const TArray<FVector> &points, const FModumateObjectAssembly &assembly)
{
	SetupFlatPolyGeometry(points, assembly);
	return;
}

void ADynamicMeshActor::SetupPlaneGeometry(const TArray<FVector> &points, const FArchitecturalMaterial &material, bool bRecreateMesh, bool bCreateCollision)
{
	FPlane pointsPlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(points, pointsPlane))
	{
		return;
	}

	FVector planeNormal(pointsPlane);
	Assembly = FModumateObjectAssembly();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset();
	Holes3D.Reset();


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

void ADynamicMeshActor::SetupMetaPlaneGeometry(const TArray<FVector> &points, const FArchitecturalMaterial &material, float alpha, bool bRecreateMesh, bool bCreateCollision)
{
	FPlane pointsPlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(points, pointsPlane))
	{
		return;
	}

	FVector planeNormal(pointsPlane);
	Assembly = FModumateObjectAssembly();

	FVector centroid = Algo::Accumulate(points, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / points.Num();
	SetActorLocation(centroid);
	SetActorRotation(FQuat::Identity);

	LayerGeometries.Reset();
	Holes3D.Reset();


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

void ADynamicMeshActor::SetupRoomGeometry(const TArray<TArray<FVector>> &Polygons, const FArchitecturalMaterial &Material)
{
	Assembly = FModumateObjectAssembly();

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
	Holes3D.Reset();

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
	Holes3D.Reset();
	Assembly = FModumateObjectAssembly();

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
		}
	}
	// Clear mesh of all procedural mesh
	for (UProceduralMeshComponent* proceduralSubLayer : ProceduralSubLayers)
	{
		proceduralSubLayer->ClearAllMeshSections();
	}
}

void ADynamicMeshActor::ClearProceduralLayers()
{
	for (int32 layerIdx = 0; layerIdx < ProceduralSubLayers.Num(); layerIdx++)
	{
		ProceduralSubLayers[layerIdx]->DestroyComponent();
	}

	ProceduralSubLayers.Reset();
}

void ADynamicMeshActor::SetupExtrudedPolyGeometry(const FModumateObjectAssembly &inAssembly, const FVector &inP1, const FVector &inP2, const FVector &objNormal, const FVector &objUp,
	const FVector2D &upperExtensions, const FVector2D &outerExtensions, const FVector &scale, bool bRecreateSection, bool bCreateCollision)
{
	if (!ensureAlways(inAssembly.Layers.Num() >= 1))
	{
		return;
	}

	Assembly = inAssembly;

	const FSimplePolygon *polyProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&Assembly, polyProfile))
	{
		return;
	}

	FVector midPoint = 0.5f * (inP1 + inP2);
	FVector baseStartPoint = inP1 - midPoint;
	FVector baseEndPoint = inP2 - midPoint;
	FVector baseExtrusionDelta = baseEndPoint - baseStartPoint;
	float baseExtrusionLength = baseExtrusionDelta.Size();
	if (!ensureAlways(!FMath::IsNearlyZero(baseExtrusionLength)))
	{
		return;
	}
	FVector extrusionDir = baseExtrusionDelta / baseExtrusionLength;
	SetActorLocation(midPoint);
	SetActorRotation(FQuat::Identity);

	const TArray<FVector2D> &profilePoints = polyProfile->Points;
	const TArray<int32> &profileTris = polyProfile->Triangles;

	vertices.Reset();
	normals.Reset();
	triangles.Reset();
	uv0.Reset();
	vertexColors.Reset();

	// TODO: scale should come in as an FVector2D
	FVector2D scale2D(scale);

	const FBox2D &profileExtents = polyProfile->Extents;
	FVector2D profileExtentsMin = profileExtents.Min * scale2D;
	FVector2D profileExtentsSize = profileExtents.GetSize() * scale2D;

	auto offsetPoint = [extrusionDir, objNormal, objUp, profileExtentsMin, profileExtentsSize, upperExtensions, outerExtensions]
	(const FVector &worldPoint, const FVector2D &polyPoint, bool bAtStart)
	{
		FVector2D pointRelative = polyPoint - profileExtentsMin;
		FVector2D pointPCT = (profileExtentsSize.GetMin() > 0.0f) ? (pointRelative / profileExtentsSize) : FVector2D::ZeroVector;
		float lengthExtension = 0.0f;

		if (bAtStart)
		{
			lengthExtension = (-pointPCT.X * upperExtensions.X) + (-pointPCT.Y * outerExtensions.X);
		}
		else
		{
			lengthExtension = (pointPCT.X * upperExtensions.Y) + (pointPCT.Y * outerExtensions.Y);
		}

		return worldPoint + (lengthExtension * extrusionDir) + (polyPoint.Y * objNormal) + (polyPoint.X * objUp);
	};

	static const float uvFactor = 0.01f;
	auto fixExtrudedTriUVs = [this](const FVector2D &uv1, const FVector2D &uv2, const FVector2D &uv3)
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
		FVector2D p1 = profilePoints[p1Idx] * scale2D;
		FVector2D p2 = profilePoints[p2Idx] * scale2D;
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

			FVector2D polyPointA = profilePoints[triIdxA]*scale2D;
			FVector2D polyPointB = profilePoints[triIdxB]*scale2D;
			FVector2D polyPointC = profilePoints[triIdxC]*scale2D;

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
	UModumateFunctionLibrary::SetMeshMaterial(Mesh, Assembly.Layers[0].Material, 0, &CachedMIDs[0]);
}

void ADynamicMeshActor::UpdateExtrudedPolyGeometry(const FModumateObjectAssembly &inAssembly, const FVector &p1, const FVector &p2, const FVector &objNormal, const FVector &objUp,
	const FVector2D &upperExtensions, const FVector2D &outerExtensions, const FVector &scale, bool bCreateCollision)
{
	SetupExtrudedPolyGeometry(inAssembly, p1, p2, objNormal, objUp, upperExtensions, outerExtensions, scale, false, bCreateCollision);
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

		TArray<FVector2D> vertices2D, OutPerimeter2D;
		TArray<int32> tris2D;
		TArray<int32> perimeterVertexHoleIndices2D; // empty
		TArray<FPolyHole2D> validHoles; // empty
		TArray<bool> outholes; // empty

		UModumateGeometryStatics::TriangulateVerticesPoly2Tri(maskVertices2D, validHoles,vertices2D,tris2D,OutPerimeter2D,outholes,perimeterVertexHoleIndices2D);
		
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

bool ADynamicMeshActor::AddRoofFace(const TArray<FVector> &polyVerts, const FVector &extrusionDir, float extrusionOffset, float layerThickness, const FArchitecturalMaterial &material, float uvRotOffset)
{
	if (polyVerts.Num() < 3)
	{
		return false;
	}

	FPlane polyPlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(polyVerts, polyPlane))
	{
		return false;
	}

	FVector polyNormal = polyPlane;
	FVector polyTangentX = (polyVerts[1] - polyVerts[0]).GetSafeNormal();
	FVector polyTangentY = polyNormal ^ polyTangentX;
	const FVector &polyOrigin = polyVerts[0];

	auto vert3DTo2D = [polyTangentX, polyTangentY, polyOrigin](const FVector &polyVert) -> FVector2D
	{
		FVector vertRelative = polyVert - polyOrigin;
		return FVector2D(vertRelative | polyTangentX, vertRelative | polyTangentY);
	};

	auto vert2DTo3D = [polyTangentX, polyTangentY, polyOrigin](const FVector2D &vert2D) -> FVector
	{
		return polyOrigin + (polyTangentX * vert2D.X) + (polyTangentY * vert2D.Y);
	};

	TArray<FVector2D> polyVerts2D;
	Algo::Transform(polyVerts, polyVerts2D, vert3DTo2D);

	TArray<FPolyHole2D> holes2D;
	TArray<FVector2D> triVerts2D, polyPerimeter2D;
	TArray<int32> polyTris, perimeterVertexHoleIndices;
	TArray<bool> mergedHoles;
	if (!UModumateGeometryStatics::TriangulateVerticesPoly2Tri(polyVerts2D, holes2D,
		triVerts2D, polyTris, polyPerimeter2D, mergedHoles, perimeterVertexHoleIndices))
	{
		return false;
	}

	TArray<FVector> triVerts3D;
	Algo::Transform(triVerts2D, triVerts3D, vert2DTo3D);
	TArray<FVector> polyPerimeter3D;
	Algo::Transform(polyPerimeter2D, polyPerimeter3D, vert2DTo3D);

	FVector bottomOffset = (extrusionDir * extrusionOffset);
	FVector topOffset = (extrusionDir * (extrusionOffset + layerThickness));

	for (int32 triIdx = 0; triIdx < polyTris.Num(); triIdx += 3)
	{
		int32 triVertIdxA = polyTris[triIdx + 0];
		int32 triVertIdxB = polyTris[triIdx + 1];
		int32 triVertIdxC = polyTris[triIdx + 2];

		const FVector &triVertA = triVerts3D[triVertIdxA];
		const FVector &triVertB = triVerts3D[triVertIdxB];
		const FVector &triVertC = triVerts3D[triVertIdxC];

		// Make triangle(s) for the bottom of the roof face
		FVector bottomTriVertA = triVertA + bottomOffset;
		FVector bottomTriVertB = triVertB + bottomOffset;
		FVector bottomTriVertC = triVertC + bottomOffset;
		MakeTriangle(bottomTriVertA, bottomTriVertB, bottomTriVertC, 0, -1, uvRotOffset);
		MakeTriangle(bottomTriVertC, bottomTriVertB, bottomTriVertA, 0, -1, uvRotOffset);

		// Make triangle(s) for the top of the roof face
		FVector topTriVertA = triVertA + topOffset;
		FVector topTriVertB = triVertB + topOffset;
		FVector topTriVertC = triVertC + topOffset;
		MakeTriangle(topTriVertA, topTriVertB, topTriVertC, 0, -1, uvRotOffset);
		MakeTriangle(topTriVertC, topTriVertB, topTriVertA, 0, -1, uvRotOffset);
	}

	int32 numPerimVerts = polyPerimeter3D.Num();
	for (int32 perimIdxA = 0; perimIdxA < numPerimVerts; ++perimIdxA)
	{
		int32 perimIdxB = (perimIdxA + 1) % numPerimVerts;

		const FVector &perimVertA = polyPerimeter3D[perimIdxA];
		const FVector &perimVertB = polyPerimeter3D[perimIdxB];

		FVector bottomPerimVertA = perimVertA + bottomOffset;
		FVector bottomPerimVertB = perimVertB + bottomOffset;
		FVector topPerimVertA = perimVertA + topOffset;
		FVector topPerimVertB = perimVertB + topOffset;

		MakeTriangle(bottomPerimVertA, bottomPerimVertB, topPerimVertA, 0, -1, uvRotOffset);
		MakeTriangle(topPerimVertA, bottomPerimVertB, bottomPerimVertA, 0, -1, uvRotOffset);
		MakeTriangle(bottomPerimVertB, topPerimVertB, topPerimVertA, 0, -1, uvRotOffset);
		MakeTriangle(topPerimVertA, topPerimVertB, bottomPerimVertB, 0, -1, uvRotOffset);
	}

	return true;
}

bool ADynamicMeshActor::AddRoofLayer(const TArray<FVector> &combinedPolyVerts, const TArray<int32> &combinedVertIndices, const TArray<bool> &edgesHaveFaces, const FVector &extrusionDir, float extrusionOffset, float layerThickness, const FArchitecturalMaterial &material, float uvRotOffset)
{
	if (!ensure(combinedVertIndices.Num() == edgesHaveFaces.Num()))
	{
		return false;
	}

	int32 numEdges = combinedVertIndices.Num();
	const FVector *combinedPolyVertsPtr = combinedPolyVerts.GetData();
	int32 vertIdxStart = 0, vertIdxEnd = 0;
	bool bAddedAnyFaces = false;

	for (int32 i = 0; i < numEdges; ++i)
	{
		vertIdxEnd = combinedVertIndices[i];

		if (edgesHaveFaces[i])
		{
			int32 numPolyVerts = (vertIdxEnd - vertIdxStart) + 1;
			TArray<FVector> polyVerts(combinedPolyVertsPtr + vertIdxStart, numPolyVerts);

			if (AddRoofFace(polyVerts, extrusionDir, extrusionOffset, layerThickness, material, uvRotOffset))
			{
				bAddedAnyFaces = true;
			}
		}

		vertIdxStart = vertIdxEnd + 1;
	}

	return bAddedAnyFaces;
}

bool ADynamicMeshActor::SetupRoofGeometry(const FModumateObjectAssembly &assembly, const TArray<FVector> &combinedPolyVerts, const TArray<int32> &combinedVertIndices, const TArray<bool> &edgesHaveFaces, float uvRotOffset, bool bCreateCollision)
{
	auto clearMeshData = [this]()
	{
		vertices.Reset();
		triangles.Reset();
		normals.Reset();
		uv0.Reset();
		vertexColors.Reset();
		tangents.Reset();
	};
	clearMeshData();

	Assembly = assembly;

	int32 numVerts = combinedPolyVerts.Num();
	if (numVerts < 3)
	{
		return false;
	}

	FVector centroid = Algo::Accumulate(combinedPolyVerts, FVector::ZeroVector, [](const FVector &c, const FVector &p) { return c + p; }) / numVerts;
	SetActorLocation(centroid);

	TArray<FVector> relativePolyVerts;
	Algo::Transform(combinedPolyVerts, relativePolyVerts, [centroid](const FVector &p) { return p - centroid; });

	FVector extrusionDir = FVector::UpVector;
	float curThickness = 0.0f;
	bool bCreatedAnyLayers = false;

	int32 numLayers = assembly.Layers.Num();

	SetupProceduralLayers(numLayers);
	CachedMIDs.SetNumZeroed(numLayers);

	for (int32 layerIdx = numLayers - 1; layerIdx >= 0; --layerIdx)
	{
		const FModumateObjectAssemblyLayer &layer = assembly.Layers[layerIdx];
		float layerThickness = layer.Thickness.AsWorldCentimeters();

		clearMeshData();
		if (AddRoofLayer(relativePolyVerts, combinedVertIndices, edgesHaveFaces, extrusionDir, curThickness, layerThickness, layer.Material, uvRotOffset))
		{
			UProceduralMeshComponent *layerMesh = ProceduralSubLayers[layerIdx];
			layerMesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, vertexColors, tangents, bCreateCollision);

			UModumateFunctionLibrary::SetMeshMaterial(layerMesh, layer.Material, 0, &CachedMIDs[layerIdx]);
			bCreatedAnyLayers = true;
		}

		curThickness += layerThickness;
	}

	UpdateLayerMaterialsFromAssembly();

	return bCreatedAnyLayers;
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

bool ADynamicMeshActor::GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const
{
	int32 lowerCP = FMath::Min(cp1, cp2);
	int32 higherCP = FMath::Max(cp1, cp2);
	TPair<int32, int32> edgeIndicesPair(lowerCP, higherCP);
	if (const FVector *interalNormalPtr = exteriorTriangleNormals.Find(edgeIndicesPair))
	{
		outNormal = *interalNormalPtr;
		return true;
	}

	return false;
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
				const FModumateObjectAssemblyLayer &layerData = Assembly.Layers[i];
				UModumateFunctionLibrary::SetMeshMaterial(procMeshComp, layerData.Material, 0, &CachedMIDs[i]);
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
