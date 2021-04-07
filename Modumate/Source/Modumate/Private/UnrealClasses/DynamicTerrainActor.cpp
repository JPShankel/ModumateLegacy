// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/DynamicTerrainActor.h"
#include "IntpThinPlateSpline2.h"

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

void ADynamicTerrainActor::SetupTerrainGeometry(const TArray<FVector>& points, bool bRecreateMesh, bool bCreateCollision /*= true*/)
{
	if (!ensureAlways(points.Num() > 2))
	{
		return;
	}

	GridPoints.Empty();
	Vertices.Empty();
	Triangles.Empty();
	UV0.Empty();
	Normals.Empty();
	VertexColors.Empty();
	Tangents.Empty();

	// Mesh parameters
	for (int32 xId = 0; xId < Xsize; xId++)
	{
		for (int32 yId = 0; yId < Ysize; yId++)
		{
			int32 xV = xId * VertSize;
			int32 yV = yId * VertSize;
			GridPoints.Add(FVector2D(xV, yV));
		}
	}

	FThinPlateSpline2 cdt;
	TArray<float> zOffsets;
	if (!cdt.Calculate(points, GridPoints, 0, zOffsets))
	{
		return;
	}

	for (int32 i = 0; i < GridPoints.Num(); i++)
	{
		Vertices.Add(FVector(GridPoints[i].X, GridPoints[i].Y, zOffsets[i]));
		Normals.Add(FVector::UpVector);
		UV0.Add(FVector2D(GridPoints[i].X, GridPoints[i].Y) / VertSize);
		Tangents.Add(FProcMeshTangent(0, 1, 0));
		VertexColors.Add(FLinearColor::White);
	}

	// Triangulation
	for (int32 xId = 0; xId < Xsize - 1; xId++)
	{
		for (int32 yId = 0; yId < Ysize - 1; yId++)
		{
			// Triangle1
			Triangles.Add(xId + (yId * Xsize) + Xsize + 1);
			Triangles.Add(xId + (yId * Xsize) + Xsize);
			Triangles.Add(xId + (yId * Xsize));
			// Triangle2
			Triangles.Add(xId + (yId * Xsize) + 1);
			Triangles.Add(xId + (yId * Xsize) + Xsize + 1);
			Triangles.Add(xId + (yId * Xsize));
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bCreateCollision);
}

void ADynamicTerrainActor::UpdateTerrainGeometryFromPoints(const TArray<FVector>& points)
{
	Vertices.Reset();

	FThinPlateSpline2 cdt;
	TArray<float> zOffsets;
	if (!cdt.Calculate(points, GridPoints, 0, zOffsets))
	{
		return;
	}

	for (int32 i = 0; i < GridPoints.Num(); i++)
	{
		Vertices.Add(FVector(GridPoints[i].X, GridPoints[i].Y, zOffsets[i]));
	}

	Mesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UV0, VertexColors, Tangents);
}
