// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/DynamicTerrainActor.h"
#include "IntpThinPlateSpline2.h"
#include "ConstrainedDelaunay2.h"

// Sets default values
ADynamicTerrainActor::ADynamicTerrainActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = Mesh;

	Mesh->bUseAsyncCooking = true;
	Mesh->SetMobility(EComponentMobility::Movable);
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
		UV0.Add(FVector2D(Vertices2D[i].X, Vertices2D[i].Y) / VertSize);
		Tangents.Add(FProcMeshTangent(0, 1, 0));
		VertexColors.Add(FLinearColor::White);
	}
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bCreateCollision);
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

	// Setup procedural mesh
	for (int32 i = 0; i < Vertices2D.Num(); i++)
	{
		Vertices.Add(FVector(Vertices2D[i].X, Vertices2D[i].Y, zOffsets[i]));
	}

	Mesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UV0, VertexColors, Tangents);
}
