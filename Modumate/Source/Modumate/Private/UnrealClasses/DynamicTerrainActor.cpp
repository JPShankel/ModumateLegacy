// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/DynamicTerrainActor.h"
#include "IntpThinPlateSpline2.h"
#include "ConstrainedDelaunay2.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

// To use SimpleDynamicMeshComponent:
// Add "ModelingComponents" and "DynamicMesh" to Build.cs
// Enable ModelingToolsEditorMode.uplugin
//#include "SimpleDynamicMeshComponent.h"

// Sets default values
ADynamicTerrainActor::ADynamicTerrainActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = Mesh;
	Mesh->bUseAsyncCooking = true;
	Mesh->SetMobility(EComponentMobility::Movable);

	GrassMesh = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GrassMesh"));
	GrassMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
	GrassMesh->SetMobility(EComponentMobility::Movable);
	GrassMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrassMesh->bUseAsOccluder = false;

	// Uncomment to test SimpleDynamicMesh
	//SimpleDynamicMesh = CreateDefaultSubobject<USimpleDynamicMeshComponent>(TEXT("GeneratedMesh"));
	//RootComponent = SimpleDynamicMesh;
	//SimpleDynamicMesh->TangentsType = EDynamicMeshTangentCalcType::AutoCalculated;
}

// Called when the game starts or when spawned
void ADynamicTerrainActor::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ADynamicTerrainActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADynamicTerrainActor::SetupTerrainGeometry(const TArray<FVector>& PerimeterPoints, const TArray<FVector>& HeightPoints, bool bRecreateMesh, bool bCreateCollision /*= true*/)
{
	if (!ensureAlways(PerimeterPoints.Num() > 2 && HeightPoints.Num() > 2))
	{
		return;
	}

	GridPoints.Empty();
	Vertices.Empty();
	Vertices2D.Empty();
	Triangles.Empty();
	UV0.Empty();
	Normals.Empty();
	VertexColors.Empty();
	Tangents.Empty();

	// Perimeter vertices
	TArray<FVector2f> perimeterVerts;
	for (auto& curPoint : PerimeterPoints)
	{
		perimeterVerts.Add(FVector2f(curPoint.X, curPoint.Y));
	}
	FPolygon2f perimeterPoly(perimeterVerts);

	// Make GridPoints from min and max of perimeter
	FVector2f min = perimeterPoly.Bounds().Min;
	FVector2f max = perimeterPoly.Bounds().Max;
	int32 numX = (max.X - min.X) / VertSize;
	int32 numY = (max.Y - min.Y) / VertSize;
	for (int32 xId = 0; xId < numX; xId++)
	{
		for (int32 yId = 0; yId < numY; yId++)
		{
			float xV = (xId * VertSize) + min.X;
			float yV = (yId * VertSize) + min.Y;
			GridPoints.Add(FVector2D(xV, yV));
		}
	}
	FDynamicGraph2<float> gridPointF;
	for (auto& curPoint : GridPoints)
	{
		gridPointF.AppendVertex(FVector2f(curPoint.X, curPoint.Y));
	}

	// Setup cdt
	TConstrainedDelaunay2<float> cdt;
	cdt.bOutputCCW = false;
	cdt.Add(perimeterPoly);
	cdt.Add(gridPointF);
	if (!cdt.Triangulate())
	{
		return;
	}

	// Translate cdt triangles
	for (auto& triangle : cdt.Triangles)
	{
		Triangles.Add(triangle.A);
		Triangles.Add(triangle.B);
		Triangles.Add(triangle.C);
	}

	// Convert cdt.Vertices for use in tps
	for (auto& curVert : cdt.Vertices)
	{
		Vertices2D.Add(FVector2D(curVert.X, curVert.Y));
	}

	// tps
	FThinPlateSpline2 tps;
	TArray<float> zOffsets;
	if (!tps.Calculate(HeightPoints, Vertices2D, 0, zOffsets))
	{
		return;
	}

	// Setup procedural mesh
	for (int32 i = 0; i < Vertices2D.Num(); i++)
	{
		Vertices.Add(FVector(Vertices2D[i].X, Vertices2D[i].Y, zOffsets[i]));
		Normals.Add(FVector::UpVector);
		UV0.Add(FVector2D(Vertices2D[i].X, Vertices2D[i].Y) / UVSize);
		Tangents.Add(FProcMeshTangent(0, 1, 0));
		VertexColors.Add(FLinearColor::Black);
	}

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bCreateCollision);
	Mesh->SetMaterial(0, TerrainMaterial);

	// Uncomment to test SimpleDynamicMesh
	/*
	SimpleDynamicMesh->GetMesh()->EnableVertexColors(FLinearColor::Black);
	SimpleDynamicMesh->GetMesh()->EnableVertexUVs(FVector2f::Zero());
	for (int32 i = 0; i < Vertices2D.Num(); i++)
	{
		FVector3d position = FVector(Vertices2D[i].X, Vertices2D[i].Y, zOffsets[i]);
		FVector3f normal = FVector::UpVector;
		FVector3f color = FLinearColor::White;
		FVector2f uv = (FVector2f(Vertices2D[i].X, Vertices2D[i].Y) / UVSize) * 100.f;

		FVertexInfo newVert(position, normal, color, uv);
		SimpleDynamicMesh->GetMesh()->AppendVertex(newVert);
	}

	for (auto& curTri : cdt.Triangles)
	{
		SimpleDynamicMesh->GetMesh()->AppendTriangle(FIndex3i(curTri.A, curTri.B, curTri.C));
	}

	SimpleDynamicMesh->NotifyMeshUpdated();
	SimpleDynamicMesh->SetMaterial(0, TerrainMaterial);
	*/
}

void ADynamicTerrainActor::UpdateTerrainGeometryFromPoints(const TArray<FVector>& HeightPoints)
{
	if (!ensureAlways(HeightPoints.Num() > 2))
	{
		return;
	}

	Vertices.Reset();

	// tps
	FThinPlateSpline2 tps;
	TArray<float> zOffsets;
	if (!tps.Calculate(HeightPoints, Vertices2D, 0, zOffsets))
	{
		return;
	}

	// Update vertices
	for (int32 i = 0; i < Vertices2D.Num(); i++)
	{
		Vertices.Add(FVector(Vertices2D[i].X, Vertices2D[i].Y, zOffsets[i]));
	}
	// Uncomment below to update normals and tangents (expensive)
	//UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UV0, Normals, Tangents);

	Mesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UV0, VertexColors, Tangents);
	UpdateInstancedMeshes(false);
}

void ADynamicTerrainActor::UpdateVertexColorByLocation(const FVector& Location, const FLinearColor& NewColor, float Radius, float Alpha)
{
	FVector2D origin2D = FVector2D(Location.X, Location.Y);
	for (int32 i = 0; i < Vertices2D.Num(); i++)
	{
		if ((Vertices2D[i] - origin2D).Size() < Radius)
		{
			FLinearColor newColor = (Alpha * (NewColor - VertexColors[i])) + VertexColors[i];
			VertexColors[i] = newColor;
		}
	}

	Mesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UV0, VertexColors, Tangents);
}

void ADynamicTerrainActor::TestSetupTerrainGeometryGTE(const TArray<FVector2D>& PerimeterPoints, const TArray<FVector>& HeightPoints, const TArray<FVector2D>& HolePoints, bool bCreateCollision /*= true*/)
{
	Vertices.Empty();
	Vertices2D.Empty();
	Triangles.Empty();
	UV0.Empty();
	Normals.Empty();
	VertexColors.Empty();
	Tangents.Empty();

	TArray<FPolyHole2D> inHoles = { FPolyHole2D(HolePoints) };
	FBox2D box2D(PerimeterPoints);

	// GTE
	int32 numX = (box2D.Max.X - box2D.Min.X) / VertSize;
	int32 numY = (box2D.Max.Y - box2D.Min.Y) / VertSize;
	FDynamicGraph2<float> inGridPoints;
	for (int32 xId = 0; xId < numX; xId++)
	{
		for (int32 yId = 0; yId < numY; yId++)
		{
			float xV = (xId * VertSize) + box2D.Min.X;
			float yV = (yId * VertSize) + box2D.Min.Y;
			inGridPoints.AppendVertex(FVector2f(xV, yV));
		}
	}

	TArray<FVector2D> gteCombinedVertices;
	if (!UModumateGeometryStatics::TriangulateVerticesGTE(PerimeterPoints, inHoles, Triangles, &gteCombinedVertices, true, &Vertices2D, &inGridPoints))
	{
		return;
	}

	// tps
	FThinPlateSpline2 tps;
	TArray<float> zOffsets;
	if (!tps.Calculate(HeightPoints, Vertices2D, 0, zOffsets))
	{
		return;
	}

	// Setup procedural mesh
	for (int32 i = 0; i < Vertices2D.Num(); i++)
	{
		Vertices.Add(FVector(Vertices2D[i].X, Vertices2D[i].Y, zOffsets[i]));
		UV0.Add(FVector2D(Vertices2D[i].X, Vertices2D[i].Y) / UVSize);
		VertexColors.Add(FLinearColor::Black);
	}
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UV0, Normals, Tangents);

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bCreateCollision);
	Mesh->SetMaterial(0, TerrainMaterial);
	GrassMesh->SetStaticMesh(GrassStaticMesh);

	UpdateInstancedMeshes(true);

	return;
}

void ADynamicTerrainActor::UpdateInstancedMeshes(bool bRecreateMesh)
{
	if (bRecreateMesh)
	{
		GrassMesh->ClearInstances();
	}

	int32 numInstMeshes = 5;
	TArray<FTransform> randTransforms;

	for (int32 i = 0; i < Triangles.Num(); i += 3)
	{
		TArray<FVector> randLocs;
		GetRandomPointsOnTriangleSurface(Triangles[i], Triangles[i + 1], Triangles[i + 2], numInstMeshes, randLocs);
		for (FVector& curLoc : randLocs)
		{
			randTransforms.Add(FTransform(
				FRotator(0.f, FMath::FRand() * 360.f, 0.f),
				curLoc + GrassMeshOffset,
				FVector::OneVector));
		}
	}

	if (bRecreateMesh)
	{
		for (FTransform& curTransform : randTransforms)
		{
			GrassMesh->AddInstanceWorldSpace(curTransform);
		}
	}
	else
	{
		GrassMesh->BatchUpdateInstancesTransforms(0, randTransforms, true, false, true);
	}
}

bool ADynamicTerrainActor::GetRandomPointsOnTriangleSurface(int32 TriA, int32 TriB, int32 TriC, int32 NumOfOutPoints, TArray<FVector>& OutPoints)
{
	if (!ensureAlways(Vertices.IsValidIndex(TriA) && Vertices.IsValidIndex(TriB) && Vertices.IsValidIndex(TriC)))
	{
		return false;
	}

	for (int32 i = 0; i < NumOfOutPoints; ++i)
	{
		FVector lerpAB = Vertices[TriA] + FMath::FRand() * (Vertices[TriB] - Vertices[TriA]);
		FVector lerpABC = lerpAB + FMath::FRand() * (Vertices[TriC] - lerpAB);
		OutPoints.Add(lerpABC);
	}

	return true;
}
