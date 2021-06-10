// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/DynamicTerrainActor.h"
#include "IntpThinPlateSpline2.h"
#include "ConstrainedDelaunay2.h"
#include "KismetProceduralMeshLibrary.h"
#include "ModumateCore//ModumateFunctionLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Drafting/ModumateDraftingElements.h"

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

#if 0
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
#endif

void ADynamicTerrainActor::SetupTerrainGeometryGTE(int32 SectionID, float GridSize, const TArray<FVector2D>& PerimeterPoints, const TArray<FVector>& HeightPoints, const TArray<FPolyHole2D>& HolePoints, bool bAddGraphPoints, bool bCreateCollision /*= true*/)
{
	TArray<FVector> vertices;
	TArray<FVector2D> vertices2D;
	TArray<FVector> normals;
	TArray<int32> triangles;
	TArray<FVector2D> uv0;
	TArray<FProcMeshTangent> tangents;
	TArray<FLinearColor> vertexColors;

	// Add grid points if required
	FBox2D box2D(PerimeterPoints);
	FDynamicGraph2<float> inGridPoints;
	if (bAddGraphPoints)
	{
		int32 numX = ((box2D.Max.X - box2D.Min.X) / GridSize) + 1;
		int32 numY = ((box2D.Max.Y - box2D.Min.Y) / GridSize) + 1;
		for (int32 xId = 0; xId < numX; xId++)
		{
			for (int32 yId = 0; yId < numY; yId++)
			{
				float xV = (xId * GridSize) + box2D.Min.X;
				float yV = (yId * GridSize) + box2D.Min.Y;
				inGridPoints.AppendVertex(FVector2f(xV, yV));
			}
		}
	}

	// Add vertices between perimeter points to match grid vertices
	TArray<FVector2D> dividedPerimeterPoints;
	for (int32 i = 0; i < PerimeterPoints.Num(); i++)
	{
		FVector2D p1 = PerimeterPoints[i];
		FVector2D p2 = PerimeterPoints[(i + 1) % PerimeterPoints.Num()];
		int32 totalPoints = int32((p1 - p2).Size() / GridSize + 0.5f);
		for (int32 idPoint = 0; idPoint < totalPoints; idPoint++)
		{
			float pct = float(idPoint) / float(totalPoints);
			FVector2D curP = p1 + pct * (p2 - p1);
			dividedPerimeterPoints.Add(curP);
		}
	}

	for (const FPolyHole2D& hole: HolePoints)
	{
		for (int32 point = 0; point < hole.Points.Num(); ++point)
		{
			FVector2D p1 = hole.Points[point];
			FVector2D p2 = hole.Points[(point + 1) % hole.Points.Num()];
			int32 totalPoints = int32((p1 - p2).Size() / GridSize + 0.5f);
			for (int32 idPoint = 0; idPoint < totalPoints; idPoint++)
			{
				float pct = float(idPoint) / float(totalPoints);
				FVector2D curP = p1 + pct * (p2 - p1);
				inGridPoints.AppendVertex(curP);
			}

		}
	}

	// GTE
	TArray<FVector2D> gteCombinedVertices;
	if (!UModumateGeometryStatics::TriangulateVerticesGTE(dividedPerimeterPoints, HolePoints, triangles, &gteCombinedVertices, true, &vertices2D, &inGridPoints))
	{
		return;
	}

	// tps
	FThinPlateSpline2 tps;
	TArray<float> zOffsets;
	if (!tps.Calculate(HeightPoints, vertices2D, 0, zOffsets))
	{
		return;
	}

	// Setup procedural mesh
	for (int32 i = 0; i < vertices2D.Num(); i++)
	{
		vertices.Add(FVector(vertices2D[i].X, vertices2D[i].Y, zOffsets[i]));
		uv0.Add(FVector2D(vertices2D[i].X, vertices2D[i].Y) / UVSize);
		vertexColors.Add(FLinearColor::Black);
	}
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(vertices, triangles, uv0, normals, tangents);

	Mesh->CreateMeshSection_LinearColor(SectionID, vertices, triangles, normals, uv0, vertexColors, tangents, bCreateCollision);
	//Mesh->SetMaterial(SectionID, TerrainMaterial);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	//GrassMesh->SetStaticMesh(GrassStaticMesh);
	//UpdateInstancedMeshes(true);

	return;
}

#if 0
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
				FRotator(0.f, (curLoc.X + curLoc.Y) * 10.0f, 0.f),
				curLoc + GrassMeshOffset,
				FVector::OneVector) * GetTransform());
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
	static constexpr float gridSize = 50.0f;
	if (!ensureAlways(Vertices.IsValidIndex(TriA) && Vertices.IsValidIndex(TriB) && Vertices.IsValidIndex(TriC)))
	{
		return false;
	}

	FVector2D p1(Vertices[TriA]);
	FVector2D p2(Vertices[TriB]);
	FVector2D p3(Vertices[TriC]);
	FBox2D box(ForceInitToZero);
	box += p1;
	box += p2;
	box += p3;

	int32 minX = (int32) FMath::Floor(box.Min.X / gridSize);
	int32 minY = (int32) FMath::Floor(box.Min.Y / gridSize);
	int32 maxX = FMath::CeilToInt(box.Max.X / gridSize);
	int32 maxY = FMath::CeilToInt(box.Max.Y / gridSize);
	for (int32 y = minY; y < maxY; ++y)
	{
		for (int32 x = minX; x < maxX; ++x)
		{
			// Hash grid position to two floating-point offsets within the grid.
			// TODO: factor out into cleaner function, or replace entirely with UE4 procedural texture.
			int32 hashSrc[2] = { x, y };
			uint64 rand = CityHash64((char*)&hashSrc, sizeof(hashSrc));
			static constexpr float maxUint32 = float(((uint64)1 << 32) - 1);
			float randX = (rand >> 32u) / maxUint32;
			float randY = (rand & 0xffffffff) / maxUint32;

			FVector2D randPoint((x + randX) * gridSize, (y + randY) * gridSize);
			FVector3f bary = VectorUtil::BarycentricCoords(FVector2f(randPoint), FVector2f(p1), FVector2f(p2), FVector2f(p3));
			if (bary.X >= 0.0f && bary.X <= 1.0f && bary.Y >= 0.0f && bary.Y <= 1.0f && bary.Z >= 0.0f)
			{
				OutPoints.Add(bary.X * Vertices[TriA] + bary.Y * Vertices[TriB] + bary.Z * Vertices[TriC]);
			}
		}
	}

	return true;
}
#endif

bool ADynamicTerrainActor::GetCutPlaneDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage,
	const FPlane& Plane, const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	TArray<TPair<FVector, FVector>> OutEdges;

	if (!FMath::PlaneAABBIntersection(Plane, Mesh->Bounds.GetBox()))
	{
		return true;
	}

	auto gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateGeometryStatics::ConvertProcMeshToLinesOnPlane(Mesh, Origin, Plane, OutEdges);

	for (auto& edge: OutEdges)
	{
		FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Key);
		FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Value);

		FVector2D clippedStart, clippedEnd;
		if (UModumateFunctionLibrary::ClipLine2DToRectangle(start, end, BoundingBox, clippedStart, clippedEnd))
		{

			TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(clippedStart),
				FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
				ModumateUnitParams::FThickness::Points(0.3f), FMColor::Gray64);
			line->SetLayerTypeRecursive(FModumateLayerType::kTerrainCut);
			ParentPage->Children.Add(line);
		}
	}

	return OutEdges.Num() != 0;
}

void ADynamicTerrainActor::GetFarDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage,
	const FPlane& Plane, const FBox2D& BoundingBox) const
{
	const FTransform localToWorld = Mesh->GetRelativeTransform();

	TArray<FEdge> terrainEdges;

	const FVector viewNormal = Plane;

	const int32 numSections = Mesh->GetNumSections();
	for (int32 section = 0; section < numSections; ++section)
	{
		const FProcMeshSection* meshSection = Mesh->GetProcMeshSection(section);
		if (meshSection)
		{
			const auto& sectionVertices = meshSection->ProcVertexBuffer;
			const auto& sectionTriangles = meshSection->ProcIndexBuffer;
			ensureAlways(sectionTriangles.Num() % 3 == 0);

			TArray <FVector> vertices;
			for (const auto& v : meshSection->ProcVertexBuffer)
			{
				vertices.Emplace(localToWorld.TransformPosition(v.Position));
			}

			UModumateGeometryStatics::GetSilhouetteEdges(vertices, sectionTriangles, viewNormal, terrainEdges, 0.1, 0.0);
		}
	}

	TArray<FEdge> clippedLines;
	for (const auto& edge: terrainEdges)
	{
		clippedLines.Append(ParentPage->lineClipping->ClipWorldLineToView(edge));
	}

	FVector2D boxClipped0;
	FVector2D boxClipped1;
	for (const auto& clippedLine: clippedLines)
	{
		FVector2D vert0(clippedLine.Vertex[0]);
		FVector2D vert1(clippedLine.Vertex[1]);

		if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
		{

			TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
				FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
				ModumateUnitParams::FThickness::Points(0.15f), FMColor::Gray96);
			ParentPage->Children.Add(line);
			line->SetLayerTypeRecursive(FModumateLayerType::kTerrainBeyond);
		}
	}
}
