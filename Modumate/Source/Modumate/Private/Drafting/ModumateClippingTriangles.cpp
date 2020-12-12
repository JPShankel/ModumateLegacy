// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateClippingTriangles.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
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
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

namespace Modumate
{
	using Vec2 = FVector2D;
	using DVec2 = FVector2d;  // Double

	static constexpr double triangleEpsilon = 0.1;  // Push triangles back slightly.
	static constexpr double minTriangleArea = 1.0;  // Cull degenerate triangles.

	FModumateClippingTriangles::FModumateClippingTriangles(const AModumateObjectInstance& CutPlane)
	{
		if (CutPlane.GetObjectType() == EObjectType::OTCutPlane)
		{
			Position = CutPlane.GetLocation();
			Normal = CutPlane.GetNormal();
		}
	}

	void FModumateClippingTriangles::AddTrianglesFromDoc(const UModumateDocument * doc)
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
			if (objectType == EObjectType::OTDoor || objectType == EObjectType::OTWindow)
			{
				const auto* parent = object->GetParentObject();
				if (parent)
				{
					const ACompoundMeshActor* portalActor = Cast<ACompoundMeshActor>(object->GetActor());
					int32 numCorners = parent->GetNumCorners();
					if (!ensure(numCorners == 4))
					{
						continue;
					}
					
					const FMOIStateData& portalState = object->GetStateData();
					FMOIPortalData portalData;
					portalState.CustomData.LoadStructData(portalData);
					FVector faceOffset = parent->GetNormal() * portalData.Justification;
					const UStaticMeshComponent* panelMesh = nullptr;
					const UStaticMeshComponent* frameMesh = nullptr;

					faceOffset += parent->GetNormal() * portalActor->GetPortalCenter(doc, object->GetStateData().AssemblyKey);
					for (int32 c = 0; c < numCorners; ++c)
					{
						vertices.Emplace(parent->GetCorner(c) + faceOffset);
					}
					triangles = { 0, 1, 2, 0, 2, 3 };
				}
			}
			else
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
							vertices.Add((vertex.Position));
						}
						triangles.Append(meshSection->ProcIndexBuffer);
					}

				}
				else
				{
					const TArray<FLayerGeomDef>& layerGeoms = meshActor->LayerGeometries;
					for (const auto& layerGeom : layerGeoms)
					{
						layerGeom.TriangulateMesh(vertices, triangles, normals, uvs, tangents, uvAnchor, 0.0f);
					}
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
				Algo::ForEach(verts, [localToWorld](FVector& v) {v = localToWorld.TransformPosition(v); });
				if (Algo::AnyOf(verts, [this](FVector v) {return IsPointInFront(v); }))
				{
					Occluder newOccluder;
					newOccluder.Vertices[0] = FVector3d(TransformMatrix.TransformPosition(verts[0])) + vectorEpsilon;
					newOccluder.Vertices[1] = FVector3d(TransformMatrix.TransformPosition(verts[1])) + vectorEpsilon;
					newOccluder.Vertices[2] = FVector3d(TransformMatrix.TransformPosition(verts[2])) + vectorEpsilon;
					if (newOccluder.Area2D() > minScaledArea)
					{
						if (DVec2((double*)(newOccluder.Vertices[1] - newOccluder.Vertices[0])).Cross(
							DVec2((double*)(newOccluder.Vertices[2] - newOccluder.Vertices[0])) ) < 0.0f)
						{   // Ensure right-handed projection.
							Swap(newOccluder.Vertices[1], newOccluder.Vertices[2]);
						}
						newOccluder.BoundingBox = FBox2D({ Vec2(FVector(newOccluder.Vertices[0])),
							Vec2(FVector(newOccluder.Vertices[1])), Vec2(FVector(newOccluder.Vertices[2])) });
						newOccluder.BoundingBox.ExpandBy(LineClipEpsilon);
						newOccluder.MinZ = FMath::Min3(newOccluder.Vertices[0].Z, newOccluder.Vertices[1].Z,
							newOccluder.Vertices[2].Z);
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

	double FModumateClippingTriangles::Occluder::Area2D() const
	{
		return FMath::Abs(0.5 * DVec2((double*)(Vertices[1] - Vertices[0])).Cross(DVec2((double*)(Vertices[2] - Vertices[0])) ));
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
		DVec2 Intersect2V(const DVec2& v1, const DVec2& v2, const DVec2& v3)
		{
			double d = v1.Cross(v2);
			if (d == 0)
			{
				return { -1.0, -1.0 };
			}
			return { (v3.X * v2.Y - v2.X * v3.Y) / d, (v3.Y * v1.X - v1.Y * v3.X) / d };
		}

		inline bool LineBoxIntersection(const FSegment3d& Line, const FBox2D& Box)
		{
			Vec2 startPoint(DVec2((double*)(Line.StartPoint()) ));
			Vec2 endPoint(DVec2((double*)(Line.EndPoint()) ));
			return UModumateFunctionLibrary::LineBoxIntersection(Box, startPoint, endPoint);
		}
	}

	// Clip one line in world space to (possibly) multiple lines in view space.
	TArray<FEdge> FModumateClippingTriangles::ClipWorldLineToView(FEdge line)
	{
		TArray<FSegment3d> outViewLines;
		TArray<FSegment3d> inViewLines;
		TArray<FEdge> returnValue;

		FSegment3d lineInViewSpace(FVector(TransformMatrix.TransformPosition(line.Vertex[0])),
			FVector(TransformMatrix.TransformPosition(line.Vertex[1])) );
		bool bVert0Forward = lineInViewSpace.StartPoint().Z > 0.0; 
		bool bVert1Forward = lineInViewSpace.EndPoint().Z > 0.0;

		if (!QuadTree.IsValid() || (!bVert0Forward && !bVert1Forward))
		{   // Behind cut plane.
			return returnValue;
		}

		if (bVert0Forward ^ bVert1Forward)
		{   // Clip to cut plane.
			double d = -lineInViewSpace.StartPoint().Z / (lineInViewSpace.EndPoint().Z - lineInViewSpace.StartPoint().Z);
			FVector3d intersect = lineInViewSpace.StartPoint() + d * (lineInViewSpace.EndPoint() - lineInViewSpace.StartPoint());
			if (bVert0Forward)
			{
				lineInViewSpace.SetEndPoint(intersect);
			}
			else
			{
				lineInViewSpace.SetStartPoint(intersect);
			}
		}

		inViewLines.Add(lineInViewSpace);
		while (inViewLines.Num() != 0)
		{
			auto viewLine = inViewLines.Pop();
			if (DVec2((double*)(viewLine.EndPoint() - viewLine.StartPoint()) ).Length() <= LineClipEpsilon * Scale)
			{
				continue;
			}

			if (QuadTree->Apply(viewLine, [this, &viewLine, &inViewLines](const Occluder& occluder)
				{return ClipSingleWorldLine(viewLine, occluder, inViewLines); }))
			{
				outViewLines.Add(viewLine);
			}
		}
		
		for (auto& l: outViewLines)
		{
			returnValue.Emplace(FVector(l.StartPoint()), FVector(l.EndPoint()) );
		}
		return MoveTemp(returnValue);
	}

	TArray<FEdge> FModumateClippingTriangles::ClipViewLineToView(FEdge lineInViewSpace)
	{
		TArray<FSegment3d> outViewLines;
		TArray<FSegment3d> inViewLines;

		TArray<FEdge> returnValue;

		if (!QuadTree.IsValid() || (lineInViewSpace.Vertex[0].Z <= 0.0f && lineInViewSpace.Vertex[1].Z <= 0.0f))
		{   // Behind cut plane.
			return returnValue;
		}

		inViewLines.Add(FSegment3d(lineInViewSpace.Vertex[0], lineInViewSpace.Vertex[1]));
		while (inViewLines.Num() != 0)
		{
			FSegment3d viewLine = inViewLines.Pop();

			if (DVec2((double*)(viewLine.EndPoint() - viewLine.StartPoint())).Length() <= LineClipEpsilon * Scale)
			{
				continue;
			}

			if (QuadTree->Apply(viewLine, [this, &viewLine, &inViewLines](const Occluder& occluder)
			{return ClipSingleWorldLine(viewLine, occluder, inViewLines); }))
			{
				outViewLines.Add(viewLine);
			}
		}

		for (auto& l: outViewLines)
		{
			returnValue.Emplace(FVector(l.StartPoint()), FVector(l.EndPoint()) );
		}
		return MoveTemp(returnValue);
	}

	FEdge FModumateClippingTriangles::WorldLineToView(FEdge line) const
	{
		return { TransformMatrix.TransformPosition(line.Vertex[0]), TransformMatrix.TransformPosition(line.Vertex[1]) };
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

	bool FModumateClippingTriangles::ClipSingleWorldLine(FSegment3d& viewLine, Occluder occluder, TArray<FSegment3d>& generatedLines)
	{
		double maxZ = FMath::Max(viewLine.StartPoint().Z, viewLine.EndPoint().Z);

		if (maxZ > occluder.MinZ && LineBoxIntersection(viewLine, occluder.BoundingBox))
		{
			DVec2 a((double*)(viewLine.StartPoint()));
			FVector3d delta3d(viewLine.EndPoint() - viewLine.StartPoint());
			DVec2 dir(delta3d.X, delta3d.Y);
			DVec2 q((double*)occluder.Vertices[0]);
			DVec2 u(DVec2((double*)occluder.Vertices[1]) - q);
			DVec2 v(DVec2((double*)occluder.Vertices[2]) - q);
			// Epsilon for eroding clipped line away in view-space:
			FVector3d vecEps(dir.Normalized() * (LineClipEpsilon * Scale));
			vecEps.Z = vecEps.Length() / dir.Length() * delta3d.Z;

			// Offset along line, into triangle, for depth comparison.
			double depthCompareOffset = delta3d.Normalized().Z * triangleEpsilon * 4.0;

			// Intersect line with all triangle sides.
			DVec2 t1Alpha = Intersect2V(dir, -u, q - a);
			DVec2 t2Beta = Intersect2V(dir, -v, q - a);
			DVec2 t3Gamma = Intersect2V(dir, u - v, DVec2((double*)occluder.Vertices[1]) - a);

			double t1 = t1Alpha.X;
			double t2 = t2Beta.X;
			double t3 = t3Gamma.X;
			bool bIntersectU = t1Alpha.Y >= 0.0 - KINDA_SMALL_NUMBER && t1Alpha.Y <= 1.0 + KINDA_SMALL_NUMBER && t1 >= 0.0 + KINDA_SMALL_NUMBER && t1 <= 1.0 - KINDA_SMALL_NUMBER;
			bool bIntersectV = t2Beta.Y >= 0.0 - KINDA_SMALL_NUMBER && t2Beta.Y <= 1.0 + KINDA_SMALL_NUMBER && t2 >= 0.0 + KINDA_SMALL_NUMBER && t2 <= 1.0 - KINDA_SMALL_NUMBER;
			bool bIntersectW = t3Gamma.Y >= 0.0 - KINDA_SMALL_NUMBER && t3Gamma.Y <= 1.0 + KINDA_SMALL_NUMBER && t3 >= 0.0 + KINDA_SMALL_NUMBER && t3 <= 1.0 - KINDA_SMALL_NUMBER;

			if (!bIntersectU && !bIntersectV && !bIntersectW)
			{   // No line-segment/triangle intersections.
				DVec2 alphaBeta = Intersect2V(u, v, a - q + 0.5f * dir);  // Mid-line barycentrics.

				if (alphaBeta.X > 0.0 && alphaBeta.X < 1.0 && alphaBeta.Y > 0.0 && alphaBeta.Y < 1.0
					&& (alphaBeta.X + alphaBeta.Y) < 1.0)
				{
					// Line segment entirely within triangle.
					double triangleZ = (1.0 - alphaBeta.X - alphaBeta.Y) * occluder.Vertices[0].Z
						+ alphaBeta.X * occluder.Vertices[1].Z + alphaBeta.Y * occluder.Vertices[2].Z;
					if ((viewLine.StartPoint().Z + viewLine.EndPoint().Z) / 2.0 >= triangleZ)
					{
						// Entire line hidden so drop.
						return false;
					}
				}
				// else line entirely outside.
			}
			else if (bIntersectU)
			{
				FVector3d intersect = viewLine.StartPoint() + t1 * (viewLine.EndPoint() - viewLine.StartPoint());
				double triangleZ = (1.0f - t1Alpha.Y) * occluder.Vertices[0].Z + t1Alpha.Y * occluder.Vertices[1].Z;
				if ((a - q).Cross(u) > 0.0)
				{
					if (intersect.Z + depthCompareOffset > triangleZ)
					{   // Clip line
						if (bIntersectV || bIntersectW)
						{   // Break into new segment to be clipped.
							generatedLines.Add({ intersect + vecEps, viewLine.EndPoint() });
						}
						viewLine.SetEndPoint(intersect - vecEps);
					}
				}
				else
				{
					if (intersect.Z - depthCompareOffset > triangleZ)
					{   // Clip line
						if (bIntersectV || bIntersectW)
						{   // Break into new segment to be clipped.
							generatedLines.Add({ viewLine.StartPoint(), intersect - vecEps });
						}
						viewLine.SetStartPoint(intersect + vecEps);
					}
				}
			}
			else if (bIntersectV)
			{
				FVector3d intersect = viewLine.StartPoint() + t2 * (viewLine.EndPoint() - viewLine.StartPoint());
				double triangleZ = (1.0 - t2Beta.Y) * occluder.Vertices[0].Z + t2Beta.Y * occluder.Vertices[2].Z;
				if ((a - q).Cross(v) < 0.0)
				{
					if (intersect.Z + depthCompareOffset > triangleZ)
					{   // Clip line
						if (bIntersectW)
						{
							generatedLines.Add({ intersect + vecEps, viewLine.EndPoint() });
						}
						viewLine.SetEndPoint(intersect - vecEps);
					}
				}
				else
				{
					if (intersect.Z - depthCompareOffset > triangleZ)
					{   // Clip line
						if (bIntersectW)
						{
							generatedLines.Add({ viewLine.StartPoint(), intersect - vecEps });
						}
						viewLine.SetStartPoint(intersect + vecEps);
					}
				}
			}
			else if (bIntersectW)
			{
				FVector3d intersect = viewLine.StartPoint() + t3 * (viewLine.EndPoint() - viewLine.StartPoint());
				double triangleZ = (1.0 - t3Gamma.Y) * occluder.Vertices[1].Z + t3Gamma.Y * occluder.Vertices[2].Z;
				if ((a - DVec2((double*)occluder.Vertices[1])).Cross(v - u) > 0.0)
				{
					if (intersect.Z + depthCompareOffset > triangleZ)
					{   // Clip line
						viewLine.SetEndPoint(intersect - vecEps);
					}
				}
				else
				{
					if (intersect.Z - depthCompareOffset > triangleZ)
					{   // Clip line
						viewLine.SetStartPoint(intersect + vecEps);
					}
				}
			}
		}

		return true;
	}

	void FModumateClippingTriangles::BuildAccelerationStructure()
	{
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

	}

	FModumateClippingTriangles::QuadTreeNode::QuadTreeNode(const FBox2D& Box, int Depth)
		: NodeBox(Box),
		  NodeDepth(Depth)
	{ }

	void FModumateClippingTriangles::QuadTreeNode::AddOccluder(const Occluder* NewOccluder)
	{
		if (NodeDepth < MaxTreeDepth)
		{
			const FBox2D& occluderBB = NewOccluder->BoundingBox;
			const Vec2 centre = NodeBox.GetCenter();

			const FBox2D childBoxes[4] =
			{
				{NodeBox.Min, centre},
				{Vec2(centre.X, NodeBox.Min.Y), Vec2(NodeBox.Max.X, centre.Y)},
				{Vec2(NodeBox.Min.X, centre.Y), Vec2(centre.X, NodeBox.Max.Y)},
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
					return;
				}
			}
		}

		Occluders.Add(NewOccluder);
	}

	bool FModumateClippingTriangles::QuadTreeNode::Apply(const FSegment3d& line, TFunctionRef<bool (const Occluder& occluder)> functor)
	{
		for (const auto& occluder: Occluders)
		{
			if (!functor(*occluder))
			{
				return false;
			}
		}

		for (int child = 0; child < 4; ++child)
		{
			if (Children[child].IsValid() && LineBoxIntersection(line, Children[child]->NodeBox))
			{
				if (!Children[child]->Apply(line, functor))
				{
					return false;
				}
			}
		}

		return true;
	}
}
