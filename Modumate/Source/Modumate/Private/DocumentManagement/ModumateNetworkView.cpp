// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateNetworkView.h"
#include <functional>
#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/Modumate.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "Algo/Transform.h"

namespace Modumate
{

	namespace FModumateNetworkView
	{
		bool pointsClose(const FVector2D &p1, const FVector2D &p2)
		{
			return FVector2D::DistSquared(p1, p2) < POINTS_CLOSE_DIST_SQ;
		}

		bool pointsClose(const FVector &p1, const FVector &p2)
		{
			return FVector::Dist(p1, p2) < POINTS_CLOSE_DIST_SQ;
		}

		FNetworkNode *getNodeAt(TArray<FNetworkNode> &nodes, const FVector2D &p)
		{
			for (auto &node : nodes)
			{
				if (pointsClose(FVector2D(node.Point.X, node.Point.Y), p))
				{
					return &node;
				}
			}
			return nullptr;
		}

		void getSegmentPtrsAt(TArray<FSegment> &segments, const FVector &p, TArray<FSegment *> &outSegmentPtrs)
		{
			outSegmentPtrs.Reset();
			FVector2D p2d(p.X, p.Y);
			for (auto &seg : segments)
			{
				if (pointsClose(FVector2D(seg.Points[0].X, seg.Points[0].Y), p2d) || pointsClose(FVector2D(seg.Points[1].X, seg.Points[1].Y), p2d))
				{
					outSegmentPtrs.Add(&seg);
				}
			}
		}

		bool TryMakePolygon(const TArray<const FModumateObjectInstance *> &obs, TArray<FVector> &polyPoints, const FModumateObjectInstance *startObj, TArray<int32> *connectedSegmentIDs)
		{
			bool bSuccess = true;
			TArray<FSegment> segments;
			Algo::Transform(
				obs, 
				segments,
				[](const FModumateObjectInstance *ob)
				{
					FSegment ret;
					ret.Points[0] = ob->GetControlPoint(0);
					ret.Points[1] = ob->GetControlPoint(1);
					ret.Valid = true;
					ret.ObjID = ob->ID;
					return ret;
				}
			);

			TArray<FNetworkNode> nodes;

			// Fill in node list
			for (auto &seg : segments)
			{
				for (int32 cpIdx = 0; cpIdx < 2; ++cpIdx)
				{
					if (getNodeAt(nodes, FVector2D(seg.Points[cpIdx].X, seg.Points[cpIdx].Y)) == nullptr)
					{
						FNetworkNode node;
						getSegmentPtrsAt(segments, seg.Points[cpIdx], node.Segments);
						node.Point = seg.Points[cpIdx];
						nodes.Add(node);
					}
				}
			}

			// Build node neighbors
			for (auto &node : nodes)
			{
				for (auto &seg : node.Segments)
				{
					for (int32 cpIdx = 0; cpIdx < 2; ++cpIdx)
					{
						FNetworkNode *neighbor = getNodeAt(nodes, FVector2D(seg->Points[cpIdx].X, seg->Points[cpIdx].Y));
						if (neighbor != &node)
						{
							node.Neighbors.Add(neighbor);
						}
					}
				}
			}

			// Traverse through connected nodes.
			TSet<FNetworkNode*> visitedNodes;
			TSet<FSegment*> visitedSegments;
			TArray<FNetworkNode*> openNodes;

			FNetworkNode *startNode = &nodes[0];
			if (startObj)
			{
				for (int32 cpIdx = 0; cpIdx < 2; ++cpIdx)
				{
					FNetworkNode *newStartNode = getNodeAt(nodes,
						FVector2D(startObj->GetControlPoint(cpIdx).X, startObj->GetControlPoint(cpIdx).Y));
					if (newStartNode)
					{
						startNode = newStartNode;
						break;
					}
				}
			}

			openNodes.Push(startNode);
			while (openNodes.Num() > 0)
			{
				FNetworkNode *node = openNodes.Pop();
				if (visitedNodes.Contains(node))
				{
					continue;
				}
				visitedNodes.Add(node);
				visitedSegments.Append(node->Segments);
				for (auto &neighbor : node->Neighbors)
				{
					if (!visitedNodes.Contains(neighbor) && !openNodes.Contains(neighbor))
					{
						openNodes.Push(neighbor);
					}
				}
			}

			// In a valid polygon, all nodes are connected - either to each other, or the provided start node
			if ((visitedNodes.Num() != nodes.Num()) && (startObj == nullptr))
			{
				return false;
			}

			// In a valid polygon, nodes only have two neighbors
			for (auto *node : visitedNodes)
			{
				if (node->Neighbors.Num() != 2)
				{
					for (auto *seg : node->Segments)
					{
						seg->Valid = false;
						bSuccess = false;
					}
				}
			}

			// In a valid polygon, nodes don't cross except at end points
			for (auto *seg1 : visitedSegments)
			{
				for (auto *seg2 : visitedSegments)
				{
					if (seg1 == seg2)
					{
						continue;
					}

					FVector2D s1p1(seg1->Points[0].X, seg1->Points[0].Y);
					FVector2D s1p2(seg1->Points[1].X, seg1->Points[1].Y);
					FVector2D s2p1(seg2->Points[0].X, seg2->Points[0].Y);
					FVector2D s2p2(seg2->Points[1].X, seg2->Points[1].Y);

					FVector2D intersection;

					if (UModumateFunctionLibrary::LinesIntersect2D(s1p1, s1p2, s2p1, s2p2, intersection))
					{
						if (pointsClose(s1p1, s2p1) || pointsClose(s1p1, s2p2) || pointsClose(s1p2, s2p1) || pointsClose(s1p2, s2p2))
						{
							continue;
						}
						seg1->Valid = false;
						seg2->Valid = false;
						bSuccess = false;
					}
				}
			}

			if (bSuccess)
			{
				TArray<FNetworkNode*> visitedNodesArray = visitedNodes.Array();
				TArray<FSegment*> visitedSegmentsArray = visitedSegments.Array();

				Algo::Transform(visitedNodesArray,polyPoints,[](FNetworkNode *node) { return FVector(node->Point.X, node->Point.Y, node->Point.Z); });

				if (connectedSegmentIDs)
				{
					TArray<int32> &connectedSegmentIDsRef = *connectedSegmentIDs;
					Algo::Transform(visitedSegmentsArray,connectedSegmentIDsRef,[](FSegment *segment) { return segment->ObjID; });
				}
			}

			return bSuccess;
		}

		void MakeSegmentGroups(const TArray<const FModumateObjectInstance *> &obs, TArray<TArray<const FModumateObjectInstance *>> &groups)
		{
			TArray<const FModumateObjectInstance *> openObs = obs;

			auto areConnected = [](const FModumateObjectInstance &ob1, const FModumateObjectInstance &ob2)
			{
				int32 numOb1ControlPoints = ob1.GetControlPoints().Num();
				int32 numOb2ControlPoints = ob2.GetControlPoints().Num();
				for (int32 cp1=0;cp1<numOb1ControlPoints;++cp1)
				{ 
					for (int32 cp2 = 0; cp2 < numOb2ControlPoints; ++cp2)
					{
						if (pointsClose(ob1.GetControlPoint(cp1), ob2.GetControlPoint(cp2)))
						{
							return true;
						}
					}
				}
				return false;
			};

			auto findConnected = [areConnected](const FModumateObjectInstance &cob, const TArray<const FModumateObjectInstance *> &obs)
			{
				TArray<const FModumateObjectInstance *> ret;
				for (auto ob : obs)
				{
					if (areConnected(cob, *ob))
					{
						ret.Add(ob);
					}
				}
				return ret;
			};

			while (openObs.Num() > 0)
			{
				const FModumateObjectInstance *ob = openObs.Pop();
				TArray<const FModumateObjectInstance *> connectedObs;
				connectedObs.Push(ob);
				TArray<const FModumateObjectInstance *> allConnected;
				allConnected.Push(ob);
				while (connectedObs.Num() > 0)
				{
					ob = connectedObs.Pop();
					TArray<const FModumateObjectInstance *> connected = findConnected(*ob, openObs);

					for (auto cob : connected)
					{
						openObs.Remove(cob);
						connectedObs.Push(cob);
					}

					allConnected.Append(connected);
				}
				if (allConnected.Num() > 0)
				{
					groups.Add(allConnected);
				}
			}
		}
	}
}