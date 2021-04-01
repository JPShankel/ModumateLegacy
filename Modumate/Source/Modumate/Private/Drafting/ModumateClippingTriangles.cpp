// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateClippingTriangles.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "KismetProceduralMeshLibrary.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateStats.h"
#include "Objects/Portal.h"
#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Math/Matrix.h"
#include "Math/ScaleMatrix.h"
#include "Math/TranslationMatrix.h"
#include "Misc/AssertionMacros.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Drafting/ModumateViewLineSegment.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Modumate Drafting Line Clipping"), STAT_ModumateDraftLineClip, STATGROUP_Modumate);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Modumate Drafting Clip Kernel"), STAT_ModumateDraftClipKernel, STATGROUP_Modumate);

namespace Modumate
{
	static constexpr double triangleEpsilon = 0.1;  // Push triangles back slightly.
	static constexpr double minTriangleArea = 0.02;  // Cull degenerate triangles.
	static constexpr double minOccluderAreaForCulling = 10.0;  // Don't bother with smaller than this triangles
															   // for rough culling.

	FModumateClippingTriangles::FModumateClippingTriangles(const AModumateObjectInstance& CutPlane)
	{
		if (CutPlane.GetObjectType() == EObjectType::OTCutPlane)
		{
			Position = CutPlane.GetLocation();
			Normal = CutPlane.GetNormal();
		}
	}

	void FModumateClippingTriangles::AddTrianglesFromDoc(const UModumateDocument* doc)
	{
		const TSet<EObjectType> separatorOccluderTypes({ EObjectType::OTWallSegment, EObjectType::OTFloorSegment,
			EObjectType::OTRoofFace, EObjectType::OTCeiling, EObjectType::OTSystemPanel, EObjectType::OTDoor,
			EObjectType::OTWindow,  EObjectType::OTStaircase, EObjectType::OTRailSegment, EObjectType::OTCabinet });

		const TSet<EObjectType>  actorMeshOccluderTypes({ EObjectType::OTStructureLine, EObjectType::OTMullion });

		TSet<EObjectType> occluderTypes(separatorOccluderTypes);
		occluderTypes.Append(actorMeshOccluderTypes);

		TArray<const AModumateObjectInstance*> occluderObjects(doc->GetObjectsOfType(occluderTypes));

		const int numObjects = occluderObjects.Num();
		int totalTriangles = 0;
		for (const auto& object: occluderObjects)
		{
			FTransform localToWorld;
			TArray<FVector> vertices;
			TArray<int32> triangles;
			TArray<FVector> normals;
			TArray<FVector2D> uvs;
			TArray<FProcMeshTangent> tangents;
			const FVector uvAnchor;

			const EObjectType objectType = object->GetObjectType();

			const ADynamicMeshActor* meshActor = nullptr;
			if (objectType == EObjectType::OTDoor || objectType == EObjectType::OTWindow || objectType == EObjectType::OTCabinet)
			{
				const auto* parent = object->GetParentObject();
				if (parent)
				{
					const ACompoundMeshActor* portalActor = nullptr;
					if (objectType == EObjectType::OTCabinet)
					{
						const ADynamicMeshActor* cabinetActor = Cast<ADynamicMeshActor>(object->GetActor());
						TArray<AActor*> attachedActors;
						cabinetActor->GetAttachedActors(attachedActors);
						if (attachedActors.Num() != 0)
						{
							portalActor = Cast<ACompoundMeshActor>(attachedActors[0]);
						}
					}
					else
					{
						portalActor = Cast<ACompoundMeshActor>(object->GetActor());
					}

					if (portalActor)
					{
						localToWorld = portalActor->GetActorTransform();
						const int32 numComponents = portalActor->StaticMeshComps.Num();

						for (int32 component = 0; component < numComponents; ++component)
						{
							const UStaticMeshComponent* staticMeshComponent = portalActor->StaticMeshComps[component];
							if (staticMeshComponent == nullptr)
							{
								continue;
							}

							if (!portalActor->UseSlicedMesh[component])
							{
								UStaticMesh* staticMesh = staticMeshComponent->GetStaticMesh();
								if (staticMesh == nullptr)
								{
									continue;
								}

								const FTransform localTransform = staticMeshComponent->GetRelativeTransform() * localToWorld;
								const int32 levelOfDetailIndex = staticMesh->GetNumLODs() - 1;
								const FStaticMeshLODResources& meshResources = staticMesh->GetLODForExport(levelOfDetailIndex);
								for (int32 section = 0; section < meshResources.Sections.Num(); ++section)
								{
									TArray<FVector> sectionPositions;
									TArray<int32> sectionIndices;
									UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, levelOfDetailIndex, section,
										sectionPositions, sectionIndices, normals, uvs, tangents);

									const int32 numVerts = sectionPositions.Num();
									const int32 numIndices = sectionIndices.Num();

									int32 indexOffset = vertices.Num();

									for (int32 v = 0; v < numVerts; ++v)
									{
										vertices.Add(localTransform.TransformPosition(sectionPositions[v]));
									}
									for (int32 i = 0; i < numIndices; ++i)
									{
										triangles.Add(sectionIndices[i] + indexOffset);
									}
								}
							}
							else
							{
								for (int32 slice = 9 * component; slice < 9 * (component + 1); ++slice)
								{
									UProceduralMeshComponent* meshComponent = portalActor->NineSliceLowLODComps[slice];

									if (meshComponent == nullptr)
									{
										continue;
									}

									const FTransform localTransform = meshComponent->GetRelativeTransform() * localToWorld;
									int numSections = meshComponent->GetNumSections();
									for (int section = 0; section < numSections; ++section)
									{
										const FProcMeshSection* meshSection = meshComponent->GetProcMeshSection(section);
										if (meshSection == nullptr)
										{
											continue;
										}
										const auto& sectionVertices = meshSection->ProcVertexBuffer;
										const auto& sectionIndices = meshSection->ProcIndexBuffer;
										const int32 numIndices = sectionIndices.Num();
										const int32 numVertices = sectionVertices.Num();
										ensure(numIndices % 3 == 0);

										int32 indexOffset = vertices.Num();

										for (int32 v = 0; v < numVertices; ++v)
										{
											vertices.Add(localTransform.TransformPosition(sectionVertices[v].Position));
										}
										for (int32 i = 0; i < numIndices; ++i)
										{
											triangles.Add(sectionIndices[i] + indexOffset);
										}

									}
								}

							}
						}
					}

				}
			}
			
			// TODO: Clean up this logic mess.
			if (objectType != EObjectType::OTDoor && objectType != EObjectType::OTWindow)
			{
				meshActor = Cast<ADynamicMeshActor>(object->GetActor());

				if (!meshActor)
				{
					continue;
				}
				localToWorld = meshActor->ActorToWorld();

				if (actorMeshOccluderTypes.Contains(objectType))
				{
					UProceduralMeshComponent* meshComponent = meshActor->Mesh;
					const FProcMeshSection* meshSection = meshComponent->GetProcMeshSection(0);
					if (meshSection)
					{
						for (const auto& vertex : meshSection->ProcVertexBuffer)
						{
							vertices.Add(localToWorld.TransformPosition(vertex.Position));
						}
						triangles.Append(meshSection->ProcIndexBuffer);
					}

				}
				else
				{
					const TArray<FLayerGeomDef>& layerGeoms = meshActor->LayerGeometries;
					int32 startVertex = vertices.Num();
					for (const auto& layerGeom : layerGeoms)
					{
						layerGeom.TriangulateMesh(vertices, triangles, normals, uvs, tangents, uvAnchor, FVector2D::UnitVector, 0.0f);
					}
					for (int32 v = startVertex; v < vertices.Num(); ++v)
					{
						vertices[v] = localToWorld.TransformPosition(vertices[v]);
					}
					AddLayeredCutPlaneTriangles(layerGeoms, localToWorld);
				}
			}

			const double minScaledArea = minTriangleArea * Scale * Scale;

			const FVector3d vectorEpsilon(0.0f, 0.0f, triangleEpsilon * Scale);
			ensure(triangles.Num() % 3 == 0);
			totalTriangles += triangles.Num() / 3;
			for (int triangle = 0; triangle < triangles.Num(); triangle += 3)
			{
				FVector verts[3] = { vertices[triangles[triangle]],
					vertices[triangles[triangle + 1]], vertices[triangles[triangle + 2]] };
				if (Algo::AnyOf(verts, [this](FVector v) {return IsPointInFront(v); }))
				{
					FModumateOccluder newOccluder(
						FVector3d(TransformMatrix.TransformPosition(verts[0])) + vectorEpsilon,
						FVector3d(TransformMatrix.TransformPosition(verts[1])) + vectorEpsilon,
						FVector3d(TransformMatrix.TransformPosition(verts[2])) + vectorEpsilon
					);
					if (newOccluder.Area2D > minScaledArea)
					{
						Occluders.Add(newOccluder);
					}
				}
			}
		}

		BuildAccelerationStructure();

	}

	bool FModumateClippingTriangles::IsPointInFront(FVector Point) const
	{
		return (FVector3d(Point) - Position).Dot(Normal) > 0.0;
	}


	void FModumateClippingTriangles::SetTransform(FVector ViewPosition, FVector ViewXAxis, float ViewScale)
	{
		Scale = ViewScale;

		FVector ViewZAxis(Normal);  // Look along +z.
		ViewZAxis.Normalize();
		ViewXAxis.Normalize();
		FVector ViewYAxis(ViewZAxis ^ ViewXAxis);
		FMatrix viewMatrix(ViewXAxis, ViewYAxis, ViewZAxis, FVector::ZeroVector);
		FTranslationMatrix translateMatrix(-ViewPosition);
		FScaleMatrix scaleMatrix(ViewScale);
		TransformMatrix = translateMatrix * viewMatrix.GetTransposed() * scaleMatrix;
	}

	namespace
	{
		// Solve a.v1 + b.v2 = v3.
		FVec2d Intersect2V(const FVec2d& v1, const FVec2d& v2, const FVec2d& v3)
		{
			double d = v1.Cross(v2);
			if (d == 0)
			{
				return { -1.0, -1.0 };
			}
			return { (v3.X * v2.Y - v2.X * v3.Y) / d, (v3.Y * v1.X - v1.Y * v3.X) / d };
		}

		inline bool LineBoxIntersection(const FModumateViewLineSegment& Line, const FBox2D& Box)
		{
			FVector2D startPoint(FVec2d((const double*)Line.Start ));
			FVector2D endPoint(FVec2d((const double*)Line.End ));
			return UModumateFunctionLibrary::LineBoxIntersection(Box, startPoint, endPoint);
		}
	}

	// Clip one line in world space to (possibly) multiple lines in view space.
	TArray<FEdge> FModumateClippingTriangles::ClipWorldLineToView(FEdge line)
	{
		SCOPE_MS_ACCUMULATOR(STAT_ModumateDraftLineClip);
		++numberClipCalls;

		TArray<FModumateViewLineSegment> outViewLines;
		TArray<FModumateViewLineSegment> inViewLines;
		TArray<FEdge> returnValue;

		FModumateViewLineSegment lineInViewSpace(FVector(TransformMatrix.TransformPosition(line.Vertex[0])),
			FVector(TransformMatrix.TransformPosition(line.Vertex[1])) );
		bool bVert0Forward = lineInViewSpace.Start.Z > 0.0; 
		bool bVert1Forward = lineInViewSpace.End.Z > 0.0;

		if (!QuadTree.IsValid() || (!bVert0Forward && !bVert1Forward))
		{   // Behind cut plane.
			return returnValue;
		}

		if (bVert0Forward ^ bVert1Forward)
		{   // Clip to cut plane.
			double d = -lineInViewSpace.Start.Z / (lineInViewSpace.End.Z - lineInViewSpace.Start.Z);
			FVector3d intersect = lineInViewSpace.Start + d * (lineInViewSpace.End - lineInViewSpace.Start);
			if (bVert0Forward)
			{
				lineInViewSpace.End = intersect;
			}
			else
			{
				lineInViewSpace.Start = intersect;
			}
		}

		inViewLines.Add(lineInViewSpace);
		while (inViewLines.Num() != 0)
		{
			auto viewLine = inViewLines.Pop();
			if (viewLine.Length2() <= LineClipEpsilon * Scale)
			{
				continue;
			}

			if (QuadTree->Apply(viewLine, [this, &viewLine, &inViewLines](const FModumateOccluder& occluder)
				{return ClipSingleWorldLine(viewLine, occluder, inViewLines); }))
			{
				outViewLines.Add(viewLine);
			}
		}
		
		numberGeneratedLines += outViewLines.Num();
		for (auto& l: outViewLines)
		{
			returnValue.Emplace(FVector(l.Start), FVector(l.End) );
		}
		return MoveTemp(returnValue);
	}

	TArray<FEdge> FModumateClippingTriangles::ClipViewLineToView(FEdge lineInViewSpace)
	{
		SCOPE_MS_ACCUMULATOR(STAT_ModumateDraftLineClip);
		++numberClipCalls;

		TArray<FModumateViewLineSegment> outViewLines;
		TArray<FModumateViewLineSegment> inViewLines;

		TArray<FEdge> returnValue;

		if (!QuadTree.IsValid() || (lineInViewSpace.Vertex[0].Z <= 0.0f && lineInViewSpace.Vertex[1].Z <= 0.0f))
		{   // Behind cut plane.
			return returnValue;
		}

		inViewLines.Add(FModumateViewLineSegment(lineInViewSpace.Vertex[0], lineInViewSpace.Vertex[1]));
		while (inViewLines.Num() != 0)
		{
			FModumateViewLineSegment viewLine = inViewLines.Pop();

			if (viewLine.Length2() <= LineClipEpsilon * Scale)
			{
				continue;
			}

			if (QuadTree->Apply(viewLine, [this, &viewLine, &inViewLines](const FModumateOccluder& occluder)
			{return ClipSingleWorldLine(viewLine, occluder, inViewLines); }))
			{
				outViewLines.Add(viewLine);
			}
		}

		numberGeneratedLines += outViewLines.Num();
		for (auto& l: outViewLines)
		{
			returnValue.Emplace(FVector(l.Start), FVector(l.End) );
		}
		return MoveTemp(returnValue);
	}

	FEdge FModumateClippingTriangles::WorldLineToView(FEdge line) const
	{
		return { TransformMatrix.TransformPosition(line.Vertex[0]), TransformMatrix.TransformPosition(line.Vertex[1]) };
	}

	bool FModumateClippingTriangles::IsBoxOccluded(const FBox2D& Box, float Depth) const
	{
		return !QuadTree->Apply(Box, [&Box, Depth](const FModumateOccluder& occluder)
			{ return IsBoxUnoccluded(occluder, Box, Depth); });
	}

	bool FModumateClippingTriangles::IsBoxOccluded(const FBox& Box) const
	{
		FBox projectedBox = Box.TransformProjectBy(TransformMatrix);
		FBox2D box(FVector2D(projectedBox.Min), FVector2D(projectedBox.Max));
		return IsBoxOccluded(box, projectedBox.Min.Z);
	}

	void FModumateClippingTriangles::GetTriangleEdges(TArray<FEdge>& outEdges) const
	{
		for (const auto& occluder: Occluders)
		{
			outEdges.Add(FEdge(FVector(occluder.Vertices[0]), FVector(occluder.Vertices[1])) );
			outEdges.Add(FEdge(FVector(occluder.Vertices[1]), FVector(occluder.Vertices[2])) );
			outEdges.Add(FEdge(FVector(occluder.Vertices[2]), FVector(occluder.Vertices[0])) );
		}
	}

	bool FModumateClippingTriangles::ClipSingleWorldLine(FModumateViewLineSegment& viewLine, const FModumateOccluder& occluder,
		TArray<FModumateViewLineSegment>& generatedLines)
	{
		double maxZ = FMath::Max(viewLine.Start.Z, viewLine.End.Z);

		if (maxZ > occluder.MinZ)
		{
			SCOPE_MS_ACCUMULATOR(STAT_ModumateDraftClipKernel);
			++numberClipKernels;

			FVec2d a((double*)(viewLine.Start));
			FVector3d delta3d(viewLine.End - viewLine.Start);
			FVec2d dir(delta3d.X, delta3d.Y);
			FVec2d q((const double*)occluder.Vertices[0]);
			FVec2d u(FVec2d((const double*)occluder.Vertices[1]) - q);
			FVec2d v(FVec2d((const double*)occluder.Vertices[2]) - q);
			// Epsilon for eroding clipped line away in view-space:
			FVector3d vecEps(dir.Normalized() * (LineClipEpsilon * Scale));
			vecEps.Z = vecEps.Length() / dir.Length() * delta3d.Z;

			// Offset along line, into triangle, for depth comparison.
			FVector3d depthVectorOffset = delta3d.Normalized() * triangleEpsilon * 4.0;

			// Intersect line with all triangle sides.
			FVec2d t1Alpha = Intersect2V(dir, -u, q - a);
			FVec2d t2Beta = Intersect2V(dir, -v, q - a);
			FVec2d t3Gamma = Intersect2V(dir, u - v, FVec2d((const double*)occluder.Vertices[1]) - a);

			double t1 = t1Alpha.X;
			double t2 = t2Beta.X;
			double t3 = t3Gamma.X;
			bool bIntersectU = t1Alpha.Y >= 0.0 - KINDA_SMALL_NUMBER && t1Alpha.Y <= 1.0 + KINDA_SMALL_NUMBER
				&& t1 >= 0.0 + KINDA_SMALL_NUMBER && t1 <= 1.0 - KINDA_SMALL_NUMBER;
			bool bIntersectV = t2Beta.Y >= 0.0 - KINDA_SMALL_NUMBER && t2Beta.Y <= 1.0 + KINDA_SMALL_NUMBER
				&& t2 >= 0.0 + KINDA_SMALL_NUMBER && t2 <= 1.0 - KINDA_SMALL_NUMBER;
			bool bIntersectW = t3Gamma.Y >= 0.0 - KINDA_SMALL_NUMBER && t3Gamma.Y <= 1.0 + KINDA_SMALL_NUMBER
				&& t3 >= 0.0 + KINDA_SMALL_NUMBER && t3 <= 1.0 - KINDA_SMALL_NUMBER;

			if (!bIntersectU && !bIntersectV && !bIntersectW)
			{   // No line-segment/triangle intersections.
				FVec2d midPoint(a + 0.5 * dir);

				if (occluder.IsWithinTriangle(midPoint))
				{
					// Line segment entirely within triangle.
					if ((viewLine.Start.Z + viewLine.End.Z) / 2.0 >= occluder.DepthAtPoint(midPoint))
					{
						// Entire line hidden so drop.
						return false;
					}
				}
				// else line entirely outside.
			}
			else if (bIntersectU)
			{
				FVector3d intersect = viewLine.Start + t1 * viewLine.AsVector();
				if ((a - q).Cross(u) > 0.0)
				{
					FVector3d intersectForDepth = intersect + depthVectorOffset;
					double triangleZ = occluder.DepthAtPoint(intersectForDepth);

					if (intersectForDepth.Z > triangleZ)
					{   // Clip line
						if (bIntersectV || bIntersectW)
						{   // Break into new segment to be clipped.
							generatedLines.Add({ intersect + vecEps, viewLine.End, viewLine.Position });
						}
						viewLine.End = intersect - vecEps;
					}
				}
				else
				{
					FVector3d intersectForDepth = intersect - depthVectorOffset;
					double triangleZ = occluder.DepthAtPoint(intersectForDepth);

					if (intersectForDepth.Z > triangleZ)
					{   // Clip line
						if (bIntersectV || bIntersectW)
						{   // Break into new segment to be clipped.
							generatedLines.Add({ viewLine.Start, intersect - vecEps, viewLine.Position });
						}
						viewLine.Start = intersect + vecEps;
					}
				}
			}
			else if (bIntersectV)
			{
				FVector3d intersect = viewLine.Start + t2 * viewLine.AsVector();
				if ((a - q).Cross(v) < 0.0)
				{
					FVector3d intersectForDepth = intersect + depthVectorOffset;
					double triangleZ = occluder.DepthAtPoint(intersectForDepth);

					if (intersectForDepth.Z > triangleZ)
					{   // Clip line
						if (bIntersectW)
						{
							generatedLines.Add({ intersect + vecEps, viewLine.End, viewLine.Position });
						}
						viewLine.End = intersect - vecEps;
					}
				}
				else
				{
					FVector3d intersectForDepth = intersect - depthVectorOffset;
					double triangleZ = occluder.DepthAtPoint(intersectForDepth);

					if (intersectForDepth.Z > triangleZ)
					{   // Clip line
						if (bIntersectW)
						{
							generatedLines.Add({ viewLine.Start, intersect - vecEps, viewLine.Position });
						}
						viewLine.Start = intersect + vecEps;
					}
				}
			}
			else if (bIntersectW)
			{
				FVector3d intersect = viewLine.Start + t3 * viewLine.AsVector();
				if ((a - FVec2d((const double*)occluder.Vertices[1])).Cross(v - u) > 0.0)
				{
					FVector3d intersectForDepth = intersect + depthVectorOffset;
					double triangleZ = occluder.DepthAtPoint(intersectForDepth);

					if (intersectForDepth.Z > triangleZ)
					{   // Clip line
						viewLine.End = intersect - vecEps;
					}
				}
				else
				{
					FVector3d intersectForDepth = intersect - depthVectorOffset;
					double triangleZ = occluder.DepthAtPoint(intersectForDepth);

					if (intersectForDepth.Z > triangleZ)
					{   // Clip line
						viewLine.Start = intersect + vecEps;
					}
				}
			}
		}
		else
		{
			++numberDepthRejects;
		}

		return true;
	}

	bool FModumateClippingTriangles::IsBoxUnoccluded(const FModumateOccluder& Occluder, const FBox2D Box, float Depth)
	{
		FVector2D p1(Box.Min); FVector2D p2(Box.Max); FVector2D p3(p1.X, p2.Y); FVector2D p4(p2.X, p1.Y);

		return Occluder.Area2D < minOccluderAreaForCulling || !(Occluder.MinZ < Depth
			&& Occluder.IsWithinTriangle(p1) && Occluder.IsWithinTriangle(p2)
			&& Occluder.IsWithinTriangle(p3) && Occluder.IsWithinTriangle(p4)
			&& Occluder.DepthAtPoint(p1) < Depth && Occluder.DepthAtPoint(p2) < Depth
			&& Occluder.DepthAtPoint(p3) < Depth && Occluder.DepthAtPoint(p4) < Depth);
	}

	void FModumateClippingTriangles::BuildAccelerationStructure()
	{
		Occluders.Sort(std::greater<FModumateOccluder>());

		FBox2D ViewBound(ForceInit);
		for (const auto& occluder: Occluders)
		{
			ViewBound += occluder.BoundingBox;
		}

		QuadTree.Reset(new QuadTreeNode(ViewBound));
		for (const auto& occluder: Occluders)
		{
			QuadTree->AddOccluder(&occluder);
		}

		QuadTree->PostProcess();
		QuadTree->GetOccluderSizesAtlevel(occludersAtLevel);
	}

	FModumateClippingTriangles::QuadTreeNode::QuadTreeNode(const FBox2D& Box, int Depth)
		: NodeBox(Box),
		  NodeDepth(Depth)
	{ }

	void FModumateClippingTriangles::QuadTreeNode::AddOccluder(const FModumateOccluder* NewOccluder)
	{
		if (NodeDepth < MaxTreeDepth)
		{
			const FBox2D& occluderBB = NewOccluder->BoundingBox;
			const FVector2D centre = NodeBox.GetCenter();

			const FBox2D childBoxes[4] =
			{
				{NodeBox.Min, centre},
				{FVector2D(centre.X, NodeBox.Min.Y), FVector2D(NodeBox.Max.X, centre.Y)},
				{FVector2D(NodeBox.Min.X, centre.Y), FVector2D(centre.X, NodeBox.Max.Y)},
				{centre, NodeBox.Max}
			};

			for (int child = 0; child < 4; ++child)
			{
				if (childBoxes[child].IsInside(occluderBB))
				{
					if (!Children[child].IsValid())
					{
						Children[child].Reset(new QuadTreeNode(childBoxes[child], NodeDepth + 1));
					}
					Children[child]->AddOccluder(NewOccluder);
					++SubtreeSize[child];
					return;
				}
			}
		}

		Occluders.Add(NewOccluder);
	}

	bool FModumateClippingTriangles::QuadTreeNode::Apply(FModumateViewLineSegment& line, TFunctionRef<bool (const FModumateOccluder& occluder)> functor)
	{
		int32 lineStartBox = line.Position;
		const int32 numOccluders = Occluders.Num();
		ensureAlways(lineStartBox >= OccluderStart);

		for (int32 i = lineStartBox - OccluderStart; i < numOccluders; ++i)
		{
			if (LineBoxIntersection(line, Occluders[i]->BoundingBox) && !functor(*Occluders[i]))
			{
				return false;
			}
			++line.Position;
		}

		int32 occluderLimit = OccluderStart + numOccluders;

		for (int child = 0; child < 4; ++child)
		{
			occluderLimit += SubtreeSize[child];
			if (Children[child].IsValid() && line.Position < occluderLimit && LineBoxIntersection(line, Children[child]->NodeBox))
			{
				if (!Children[child]->Apply(line, functor))
				{
					return false;
				}

			}
			line.Position = FMath::Max(line.Position, occluderLimit);
		}

		return true;
	}

	bool FModumateClippingTriangles::QuadTreeNode::Apply(const FBox2D& box, TFunctionRef<bool(const FModumateOccluder& occluder)> functor)
	{
		for (const auto& occluder : Occluders)
		{
			if (box.Intersect(occluder->BoundingBox) && !functor(*occluder))
			{
				return false;
			}
		}

		for (int child = 0; child < 4; ++child)
		{
			if (Children[child].IsValid() && box.Intersect(Children[child]->NodeBox))
			{
				if (!Children[child]->Apply(box, functor))
				{
					return false;
				}
			}
		}

		return true;
	}

	void FModumateClippingTriangles::QuadTreeNode::GetOccluderSizesAtlevel(int32 sizes[]) const
	{
		if (ensureAlways(NodeDepth <= MaxTreeDepth))
		{
			sizes[NodeDepth] += Occluders.Num();
			for (int32 i = 0; i < 4; ++i)
			{
				if (Children[i].IsValid())
				{
					Children[i]->GetOccluderSizesAtlevel(sizes);
				}
			}
		}
	}

	// Occluder nodes are processed in a pre-order, depth-first manner.
	// The occluders are numbered in processing order and the
	// line-segment structure tracks the next occluder to process;
	// for lines generated from clipping processing skips already
	// processed nodes. Each node holds its position in the global
	// order and the sizes of each sub-tree to implement this.
	void FModumateClippingTriangles::QuadTreeNode::PostProcess(int32 GlobalPosition /* = 0*/)
	{
		OccluderStart = GlobalPosition;
		GlobalPosition += Occluders.Num();
		for (int32 i = 0; i < 4; ++i)
		{
			const auto& child = Children[i];
			if (child.IsValid())
			{
				child->PostProcess(GlobalPosition);
				GlobalPosition += SubtreeSize[i];
			}
		}
	}

	// Add obscuring quads in the cut plane to hide beyond lines within layered objects.
	void FModumateClippingTriangles::AddLayeredCutPlaneTriangles(const TArray<FLayerGeomDef>& LayerGeoms, const FTransform& LocalToWorld)
	{
		if (LayerGeoms.Num() == 0)
		{
			return;
		}

		const FLayerGeomDef& layer1 = LayerGeoms[0];
		const FLayerGeomDef& layer2 = LayerGeoms.Last();
		TArray<FVector> layerA = layer1.OriginalPointsA;
		TArray<FVector> layerB = layer2.OriginalPointsB;

		const FMatrix localToView = LocalToWorld.ToMatrixWithScale() * TransformMatrix;
		for (auto& v: layerA)
		{
			v = localToView.TransformPosition(v);
		}
		for (auto& v: layerB)
		{
			v = localToView.TransformPosition(v);
		}

		const int32 numPoints = layerA.Num();

		TArray<FVector3d> intersectionsA;
		TArray<FVector3d> intersectionsB;

		// Intersect wih z = 0.
		auto intersect = [](const FVector3d& v1, const FVector3d& v2, TArray<FVector3d>& intersections)
		{
			FVector3d intersection;
			if((v1.Z < 0.0) ^ (v2.Z < 0.0))
			{
				FVector3d d(v2 - v1);
				intersection = v1 - d * (v1.Z / d.Z);
				intersection.Z = triangleEpsilon;
				intersections.Add(intersection);
			}

		};

		for (int32 point = 0; point < numPoints; ++point)
		{
			const int32 point2 = (point + 1) % numPoints;
			intersect(layerA[point], layerA[point2], intersectionsA);
			intersect(layerB[point], layerB[point2], intersectionsB);
		}


		if (intersectionsA.Num() > 0 && intersectionsA.Num() == intersectionsB.Num() && intersectionsA.Num() % 2 == 0)
		{
			const FVector3d spanDelta = intersectionsA.Last() - intersectionsA[0];

			const int32 numHoles = layer1.CachedHoles2D.Num();

			for (int32 holeIndex = 0; holeIndex < numHoles; ++holeIndex)
			{
				const auto& hole1 = layer1.CachedHoles2D[holeIndex].Points;
				const auto& hole2 = layer2.CachedHoles2D[holeIndex].Points;
				const int32 numCorners = hole1.Num();
				for (int32 point = 0; point < numCorners; ++point)
				{
					int32 point2 = (point + 1) % numCorners;
					FVector p1 = localToView.TransformPosition(layer1.Deproject2DPoint(hole1[point], true));
					FVector p2 = localToView.TransformPosition(layer1.Deproject2DPoint(hole1[point2], true));
					intersect(p1, p2, intersectionsA);
					p1 = localToView.TransformPosition(layer2.Deproject2DPoint(hole1[point], false));
					p2 = localToView.TransformPosition(layer2.Deproject2DPoint(hole1[point2], false));
					intersect(p1, p2, intersectionsB);
				}
			}
		
			if (intersectionsA.Num() == intersectionsB.Num() && intersectionsA.Num() % 2 == 0)
			{
				if (FMath::Abs(spanDelta.X) > FMath::Abs(spanDelta.Y))
				{
					intersectionsA.Sort([](const FVector3d& a, const FVector3d& b)
					{ return a.X < b.X; });
					intersectionsB.Sort([](const FVector3d& a, const FVector3d& b)
					{ return a.X < b.X; });
				}
				else
				{
					intersectionsA.Sort([](const FVector3d& a, const FVector3d& b)
					{ return a.Y < b.Y; });
					intersectionsB.Sort([](const FVector3d& a, const FVector3d& b)
					{ return a.Y < b.Y; });
				}

				for (int32 i = 0; i < intersectionsA.Num(); i += 2)
				{
					FModumateOccluder occluder1(intersectionsA[i], intersectionsB[i], intersectionsB[i + 1]);
					FModumateOccluder occluder2(intersectionsA[i], intersectionsB[i + 1], intersectionsA[i + 1]);
					if (occluder1.Area2D > minTriangleArea)
					{
						Occluders.Add(occluder1);
					}
					if (occluder2.Area2D > minTriangleArea)
					{
						Occluders.Add(occluder2);
					}
				}
			}
		}
	}

}
