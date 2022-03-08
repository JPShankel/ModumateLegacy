// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerMeshCache.h"

#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "TransformTypes.h"
#include "KismetProceduralMeshLibrary.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"


UDrawingDesignerMeshCache::UDrawingDesignerMeshCache()
{ }

void UDrawingDesignerMeshCache::Shutdown()
{ }

bool UDrawingDesignerMeshCache::GetDesignerLines(const FBIMAssemblySpec& ObAsm, const FVector& Scale, bool bLateralInvert, TArray<FDrawingDesignerLined>& OutLines)
{
	TArray<CacheItem>* cachedList = Cache.Find(ObAsm.PresetGUID);
	static constexpr float scaleTolerance = 1e-5f;

	if (cachedList)
	{
		for (auto& cacheItem : *cachedList)
		{
			if (Scale.Equals(cacheItem.Scale, scaleTolerance))
			{
				OutLines.Append(cacheItem.Mesh);
				return true;
			}
		}
	}

	TArray<FDrawingDesignerLined> newLines;
	if (ensure(GetLinesForAssembly(ObAsm, Scale, newLines)))
	{
		OutLines.Append(newLines);
		auto& itemList = Cache.FindOrAdd(ObAsm.PresetGUID);
		CacheItem newItem = { Scale, MoveTemp(newLines) };
		itemList.Emplace(MoveTemp(newItem));
	}
	else
	{
		return false;
	}

	return true;
}

bool UDrawingDesignerMeshCache::GetDesignerLines(const FBIMAssemblySpec& ObAsm, const FVector& Scale, bool bLateralInvert,
	const FVector& ViewDirection, TArray<FDrawingDesignerLined>& OutLines, float MinLength /*= 0.0f*/)
{
	static constexpr double angleThreshold = 0.866;  // 30 deg
	TArray <FDrawingDesignerLined> allLines;
	const FVector3d viewDirection(ViewDirection);

	if (ensure(GetDesignerLines(ObAsm, Scale, bLateralInvert, allLines)))
	{
		for (auto& line : allLines)
		{
			if (line.Length() < MinLength)
			{
				line.bValid = false;
			}
			else
			{
				const bool bFrontFacing = viewDirection.Dot(line.N) < 0.0;
				if (!bFrontFacing || (viewDirection.Dot(line.AdjacentN) < 0.0 && line.N.Dot(line.AdjacentN) >= angleThreshold))
				{
					line.bValid = false;
				}
			}

			if (line)
			{
				OutLines.Add(line);
			}
		}

		return true;
	}

	return false;
}

bool UDrawingDesignerMeshCache::GetLinesForAssembly(const FBIMAssemblySpec& Assembly, const FVector& Scale, TArray<FDrawingDesignerLined>& OutLines)
{
	ACompoundMeshActor* actor = GetWorld()->SpawnActor<ACompoundMeshActor>();
	if (!ensure(actor))
	{
		return false;
	}

	static constexpr bool bUseLowLOD = false;  // TBD

	actor->MakeFromAssembly(Assembly, Scale, false, false);

	TArray<FDrawingDesignerLined> lines;
	const auto& staticMeshComps = actor->StaticMeshComps;
	const int32 numComponents = staticMeshComps.Num();
	for (int32 component = 0; component < numComponents; ++component)
	{
		const UStaticMeshComponent* staticMeshComponent = staticMeshComps[component];
		if (staticMeshComponent == nullptr)
		{
			continue;
		}

		if (actor->UseSlicedMesh[component])
		{
			// Use nine-sliced mesh component:
			const int32 sliceStart = 9 * component;
			const int32 sliceEnd = 9 * (component + 1);
			for (int32 slice = sliceStart; slice < sliceEnd; ++slice)
			{
				UProceduralMeshComponent* mesh9Component = bUseLowLOD ? actor->NineSliceLowLODComps[slice] : actor->NineSliceComps[slice];

				if (mesh9Component == nullptr)
				{
					continue;
				}

				const FTransform3d componentTransform(mesh9Component->GetComponentTransform());
				const int32 numSections = mesh9Component->GetNumSections();
				for (int32 section = 0; section < numSections; ++section)
				{
					const FProcMeshSection* meshSection = mesh9Component->GetProcMeshSection(section);
					if (meshSection == nullptr)
					{
						continue;
					}

					const TArray<FProcMeshVertex>& sectionVertices = meshSection->ProcVertexBuffer;
					const TArray<uint32>& sectionIndices = meshSection->ProcIndexBuffer;
					const int32 numVertices = sectionVertices.Num();
					const int32 numIndices = sectionIndices.Num();
					ensure(numIndices % 3 == 0);

					const int32 numTriangles = numIndices / 3;
					for (int32 t = 0; t < numTriangles; ++t)
					{
						FVector3d p0(componentTransform.TransformPosition(sectionVertices[sectionIndices[3 * t]].Position));
						FVector3d p1(componentTransform.TransformPosition(sectionVertices[sectionIndices[3 * t + 1]].Position));
						FVector3d p2(componentTransform.TransformPosition(sectionVertices[sectionIndices[3 * t + 2]].Position));

						FVector3d N((p2 - p0).Cross(p1 - p0));
						double area2 = N.SquaredLength();
						N.Normalize();
						// Check for various degenerate cases.
						static constexpr double minSideLen = 0.001;
						if (area2 > 0.005)
						{
							if (p0.DistanceSquared(p1) >= minSideLen)
							{
								lines.Emplace(p0, p1, N);
							}
							if (p1.DistanceSquared(p2) >= minSideLen)
							{
								lines.Emplace(p1, p2, N);
							}
							if (p2.DistanceSquared(p0) >= minSideLen)
							{
								lines.Emplace(p2, p0, N);
							}
						}
					}
				}
			}
		}
		else
		{
			// Use static mesh:
			UStaticMesh* staticMesh = staticMeshComponent->GetStaticMesh();
			if (staticMesh == nullptr)
			{
				continue;
			}

			TArray<FVector2D> UVs;		// Unused
			TArray<FProcMeshTangent> tangents;	// Unused

			const FTransform3d componentTransform(staticMeshComponent->GetComponentTransform());
			const int lodNumber = bUseLowLOD ? staticMesh->GetNumLODs() - 1 : 0;
			const int32 numSections = staticMesh->GetNumSections(lodNumber);
			for (int32 section = 0; section < numSections; ++section)
			{
				TArray<FVector> sectionVertices;
				TArray<int32> sectionIndices;
				TArray<FVector> sectionNormals;
				UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, lodNumber, section,
					sectionVertices, sectionIndices, sectionNormals, UVs, tangents);
				ensure(sectionIndices.Num() % 3 == 0);

				const int32 numTriangles = sectionIndices.Num() / 3;
				for (int32 t = 0; t < numTriangles; ++t)
				{
					FVector3d p0(componentTransform.TransformPosition(sectionVertices[sectionIndices[3 * t]]));
					FVector3d p1(componentTransform.TransformPosition(sectionVertices[sectionIndices[3 * t + 1]]));
					FVector3d p2(componentTransform.TransformPosition(sectionVertices[sectionIndices[3 * t + 2]]));

					// Wind order is CW(?)
					FVector3d N(((p2 - p0).Cross(p1 - p0)).Normalized());
					lines.Emplace(p0, p1, N);
					lines.Emplace(p1, p2, N);
					lines.Emplace(p2, p0, N);
				}
			}
		}
	}

	actor->Destroy();

	for (auto& line: lines)
	{
		line.Canonicalize();
	}

	static constexpr double planarityThreshold = 0.9962;  // 5 degrees
	static constexpr double distanceThreshold = 0.2;  // 2 mm
	static constexpr double distanceThreshold2 = distanceThreshold * distanceThreshold;

	int numDroppedLines = 0;

	const int32 numLines = lines.Num();
	for (int32 l1 = 0; l1 < numLines; ++l1)
	{
		auto& line1 = lines[l1];
		if (line1)
		{
			for (int32 l2 = l1 + 1; l2 < numLines; ++l2)
			{
				auto& line2 = lines[l2];
				if (line2 &&
					((line1.P1.DistanceSquared(line2.P1) < distanceThreshold2 && line1.P2.DistanceSquared(line2.P2) < distanceThreshold2)
						|| (line1.P1.DistanceSquared(line2.P2) < distanceThreshold2 && line1.P2.DistanceSquared(line2.P1) < distanceThreshold2)) )
				{
					double wingAngle = FMath::Abs(line1.N.Dot(line2.N));
					if (wingAngle > planarityThreshold)
					{   // Line is internal
						line1.bValid = false;
						line2.bValid = false;
						numDroppedLines += 2;
					}
					else
					{
						line1.AdjacentN = line2.N;
						line2.AdjacentN = line1.N;
					}
				}
			}
		}
	}

	for (const auto& line : lines)
	{
		if (line)
		{
			OutLines.Add(line);
		}
	}

	return true;
}
