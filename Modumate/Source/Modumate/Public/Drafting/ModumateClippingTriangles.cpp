// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateClippingTriangles.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "ModumateCore/ModumateFunctionLibrary.h"

#include "Algo/Copy.h"
#include "Algo/ForEach.h"
#include "Algo/AnyOf.h"
#include "Math/Matrix.h"
#include "Math/TranslationMatrix.h"
#include "Math/ScaleMatrix.h"
#include "Misc/AssertionMacros.h"

namespace Modumate
{
	using Vec2 = FVector2D;

	FModumateClippingTriangles::FModumateClippingTriangles(const FModumateObjectInstance& CutPlane)
	{
		if (CutPlane.GetObjectType() == EObjectType::OTCutPlane)
		{
			Position = CutPlane.GetObjectLocation();
			Normal = CutPlane.GetNormal();
		}
	}

	void FModumateClippingTriangles::AddTrianglesFromDoc(const FModumateDocument * doc)
	{
		TArray<const FModumateObjectInstance*> separatorObjects;
		Algo::CopyIf(doc->GetObjectInstances(), separatorObjects, [](const FModumateObjectInstance* moi)
		{ auto t = moi->GetObjectType(); return t == EObjectType::OTWallSegment
			|| t == EObjectType::OTFloorSegment || t == EObjectType::OTRoofFace;  });

		const int numObjects = separatorObjects.Num();
		int totalTriangles = 0;
		for (const auto& object: separatorObjects)
		{
			const ADynamicMeshActor* meshActor = dynamic_cast<const ADynamicMeshActor*>(object->GetActor());
			if (!meshActor)
			{
				continue;
			}

			FTransform localToWorld = meshActor->ActorToWorld();
			TArray<FVector> vertices;
			TArray<int32> triangles;
			TArray<FVector> normals;
			TArray<FVector2D> uvs;
			TArray<FProcMeshTangent> tangents;
			const FVector uvAnchor;

			const TArray<FLayerGeomDef>& layerGeoms = meshActor->LayerGeometries;
			for (const auto& layerGeom: layerGeoms)
			{
				layerGeom.TriangulateMesh(vertices, triangles, normals, uvs, tangents, uvAnchor, 0.0f);
			}

			static constexpr float triangleEpsilon = 1.0f;  // Push triangles back slightly.
			static constexpr float minTriangleArea = 1.0f;  // Cull degenerate triangles.

			const float minScaledArea = minTriangleArea * Scale * Scale;

			const FVector vectorEpsilon(0.0f, 0.0f, triangleEpsilon * Scale);
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
					newOccluder.Vertices[0] = TransformMatrix.TransformPosition(verts[0]) + vectorEpsilon;
					newOccluder.Vertices[1] = TransformMatrix.TransformPosition(verts[1]) + vectorEpsilon;
					newOccluder.Vertices[2] = TransformMatrix.TransformPosition(verts[2]) + vectorEpsilon;
					if (newOccluder.Area2D() > minScaledArea)
					{
						if ((Vec2(newOccluder.Vertices[1] - newOccluder.Vertices[0])
							^ Vec2(newOccluder.Vertices[2] - newOccluder.Vertices[0])) < 0.0f)
						{   // Ensure right-handed projection.
							Swap(newOccluder.Vertices[1], newOccluder.Vertices[2]);
						}
						newOccluder.BoundingBox = FBox2D({ Vec2(newOccluder.Vertices[0]),
							Vec2(newOccluder.Vertices[1]), Vec2(newOccluder.Vertices[2]) });
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
		return (Point - Position | Normal) > 0.0f;
	}

	float FModumateClippingTriangles::Occluder::Area2D() const
	{
		return FMath::Abs(0.5f * Vec2(Vertices[1] - Vertices[0]) ^ Vec2(Vertices[2] - Vertices[0]));
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
		FVector2D Intersect2V(const FVector2D& v1, const FVector2D& v2, const FVector2D& v3)
		{
			float d = v1 ^ v2;
			if (d == 0)
			{
				return { 0.0f, 0.0f };
			}
			return { (v3.X * v2.Y - v2.X * v3.Y) / d, (v3.Y * v1.X - v1.Y * v3.X) / d };
		}

		inline bool LineBoxIntersection(const FEdge& Line, const FBox2D& Box)
		{
			return UModumateFunctionLibrary::LineBoxIntersection(Box, Vec2(Line.Vertex[0]), Vec2(Line.Vertex[1]));
		}
	}

	// Clip one line in world space to (possibly) multiple lines in view space.
	TArray<FEdge> FModumateClippingTriangles::ClipWorldLineToView(FEdge line)
	{
		TArray<FEdge> outViewLines;
		TArray<FEdge> inViewLines;

		FEdge lineInViewSpace(TransformMatrix.TransformPosition(line.Vertex[0]),
			TransformMatrix.TransformPosition(line.Vertex[1]));
		if (!QuadTree.IsValid() || (lineInViewSpace.Vertex[0].Z <= 0.0f && lineInViewSpace.Vertex[1].Z <= 0.0f))
		{   // Behind cut plane.
			return outViewLines;
		}

		inViewLines.Add(lineInViewSpace);
		while (inViewLines.Num() != 0)
		{
			FEdge viewLine = inViewLines.Pop();

			if (Vec2::Distance(Vec2(viewLine.Vertex[1]), Vec2(viewLine.Vertex[0])) <= LineClipEpsilon * Scale)
			{
				continue;
			}

			if (QuadTree->Apply(viewLine, [this, &viewLine, &inViewLines](const Occluder& occluder)
				{return ClipSingleWorldLine(viewLine, occluder, inViewLines); }))
			{
				outViewLines.Add(viewLine);
			}
		}
		return outViewLines;
	}

	bool FModumateClippingTriangles::ClipSingleWorldLine(FEdge& viewLine, Occluder occluder, TArray<FEdge>& generatedLines)
	{
		float maxZ = FMath::Max(viewLine.Vertex[0].Z, viewLine.Vertex[1].Z);

		if (maxZ > occluder.MinZ && LineBoxIntersection(viewLine, occluder.BoundingBox))
		{
			Vec2 a(viewLine.Vertex[0]);
			Vec2 dir(viewLine.Vertex[1] - viewLine.Vertex[0]);
			Vec2 q(occluder.Vertices[0]);
			Vec2 u(Vec2(occluder.Vertices[1]) - q);
			Vec2 v(Vec2(occluder.Vertices[2]) - q);
			// Epsilon for eroding clipped line away in view-space:
			const FVector vecEps(dir.GetSafeNormal() * (LineClipEpsilon * Scale), 0.0f);

			// Intersect line with all triangle sides.
			Vec2 t1Alpha = Intersect2V(dir, -u, q - a);
			Vec2 t2Beta = Intersect2V(dir, -v, q - a);
			Vec2 t3Gamma = Intersect2V(dir, u - v, Vec2(occluder.Vertices[1]) - a);

			float t1 = t1Alpha.X;
			float t2 = t2Beta.X;
			float t3 = t3Gamma.X;
			bool bIntersectU = t1Alpha.Y > 0.0f && t1Alpha.Y < 1.0f && t1 > 0.0f && t1 < 1.0f;
			bool bIntersectV = t2Beta.Y > 0.0f && t2Beta.Y < 1.0f && t2 > 0.0f && t2 < 1.0f;
			bool bIntersectW = t3Gamma.Y > 0.0f && t3Gamma.Y < 1.0f && t3 > 0.0f && t3 < 1.0f;

			if (!bIntersectU && !bIntersectV && !bIntersectW)
			{   // No line-segment/triangle intersections.
				Vec2 alphaBeta = Intersect2V(u, v, a - q + 0.5f * dir);  // Mid-line barycentrics.

				if (alphaBeta.X > 0.0f && alphaBeta.X < 1.0f && alphaBeta.Y > 0.0f && alphaBeta.Y < 1.0f
					&& (alphaBeta.X + alphaBeta.Y) < 1.0f)
				{
					// Line segment entirely within triangle.
					float triangleZ = (1.0f - alphaBeta.X - alphaBeta.Y) * occluder.Vertices[0].Z
						+ alphaBeta.X * occluder.Vertices[1].Z + alphaBeta.Y * occluder.Vertices[2].Z;
					if ((viewLine.Vertex[0].Z + viewLine.Vertex[1].Z) / 2.0f >= triangleZ)
					{
						// Entire line hidden so drop.
						return false;
					}
				}
				// else line entirely outside.
			}
			else if (bIntersectU)
			{
				FVector intersect = viewLine.Vertex[0] + t1 * (viewLine.Vertex[1] - viewLine.Vertex[0]);
				float triangleZ = (1.0f - t1Alpha.Y) * occluder.Vertices[0].Z + t1Alpha.Y * occluder.Vertices[1].Z;
				if (intersect.Z > triangleZ)
				{   // Clip.
					if (((a - q) ^ u) > 0.0f)
					{
						if (bIntersectV || bIntersectW)
						{   // Break into new segment to be clipped.
							generatedLines.Add(FEdge(intersect + vecEps, viewLine.Vertex[1]));
						}
						viewLine.Vertex[1] = intersect - vecEps;
					}
					else
					{
						if (bIntersectV || bIntersectW)
						{   // Break into new segment to be clipped.
							generatedLines.Add(FEdge(viewLine.Vertex[0], intersect - vecEps));
						}
						viewLine.Vertex[0] = intersect + vecEps;
					}
				}
			}
			else if (bIntersectV)
			{
				FVector intersect = viewLine.Vertex[0] + t2 * (viewLine.Vertex[1] - viewLine.Vertex[0]);
				float triangleZ = (1.0f - t2Beta.Y) * occluder.Vertices[0].Z + t2Beta.Y * occluder.Vertices[2].Z;
				if (intersect.Z > triangleZ)
				{   // Clip.
					if (((a - q) ^ v) < 0.0f)
					{
						if (bIntersectW)
						{
							generatedLines.Add(FEdge(intersect + vecEps, viewLine.Vertex[1]));
						}
						viewLine.Vertex[1] = intersect - vecEps;
					}
					else
					{
						if (bIntersectW)
						{
							generatedLines.Add(FEdge(viewLine.Vertex[0], intersect - vecEps));
						}
						viewLine.Vertex[0] = intersect + vecEps;
					}
				}
			}
			else if (bIntersectW)
			{
				FVector intersect = viewLine.Vertex[0] + t3 * (viewLine.Vertex[1] - viewLine.Vertex[0]);
				float triangleZ = (1.0f - t3Gamma.Y) * occluder.Vertices[1].Z + t3Gamma.Y * occluder.Vertices[2].Z;
				if (intersect.Z > triangleZ)
				{   // Clip.
					if ((a - Vec2(occluder.Vertices[1]) ^ (v - u)) > 0.0f)
					{
						viewLine.Vertex[1] = intersect - vecEps;
					}
					else
					{
						viewLine.Vertex[0] = intersect + vecEps;
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

	bool FModumateClippingTriangles::QuadTreeNode::Apply(const FEdge& line, TFunctionRef<bool (const Occluder& occluder)> functor)
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
