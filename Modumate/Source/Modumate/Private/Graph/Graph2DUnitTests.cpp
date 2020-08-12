#include "CoreMinimal.h"

#include "DocumentManagement/ModumateDelta.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph2DDelta.h"
#include "UnrealClasses/ModumateGameInstance.h"

namespace Modumate
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphDefaultTest, "Modumate.Graph.2D.Init", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphDefaultTest::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphOneTriangle, "Modumate.Graph.2D.OneTriangle", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphOneTriangle::RunTest(const FString& Parameters)
	{
		Modumate::FGraph2D graph;

		int32 p1ID = graph.AddVertex(FVector2D(0.0f, 0.0f))->ID;
		int32 p2ID = graph.AddVertex(FVector2D(10.0f, 0.0f))->ID;
		int32 p3ID = graph.AddVertex(FVector2D(5.0f, 10.0f))->ID;

		graph.AddEdge(p1ID, p2ID);
		graph.AddEdge(p2ID, p3ID);
		graph.AddEdge(p3ID, p1ID);

		int32 numPolys = graph.CalculatePolygons();
		return (numPolys == 2);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphOnePentagon, "Modumate.Graph.2D.OnePentagon", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphOnePentagon::RunTest(const FString& Parameters)
	{
		Modumate::FGraph2D graph;

		int32 p1ID = graph.AddVertex(FVector2D(0.0f, 0.0f))->ID;
		int32 p2ID = graph.AddVertex(FVector2D(10.0f, 0.0f))->ID;
		int32 p3ID = graph.AddVertex(FVector2D(20.0f, 10.0f))->ID;
		int32 p4ID = graph.AddVertex(FVector2D(8.0f, 5.0f))->ID;
		int32 p5ID = graph.AddVertex(FVector2D(0.0f, 10.0f))->ID;

		graph.AddEdge(p1ID, p2ID);
		graph.AddEdge(p2ID, p3ID);
		graph.AddEdge(p3ID, p4ID);
		graph.AddEdge(p4ID, p5ID);
		graph.AddEdge(p5ID, p1ID);

		int32 numPolys = graph.CalculatePolygons();
		return (numPolys == 2);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphIrregularShapes, "Modumate.Graph.2D.IrregularShapes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphIrregularShapes::RunTest(const FString& Parameters)
	{
		Modumate::FGraph2D graph;

		// Make a bow-tie, with a peninsula
		int32 tieCenterID = graph.AddVertex(FVector2D(10.0f, 5.0f))->ID;
		int32 tieLowerLeftID = graph.AddVertex(FVector2D(0.0f, 0.0f))->ID;
		int32 tieUpperLeftID = graph.AddVertex(FVector2D(0.0f, 10.0f))->ID;
		int32 tieLowerRightID = graph.AddVertex(FVector2D(20.0f, 0.0f))->ID;
		int32 tieUpperRightID = graph.AddVertex(FVector2D(20.0f, 10.0f))->ID;
		int32 tieLeftCenterID = graph.AddVertex(FVector2D(5.0f, 5.0f))->ID;

		// Make an edge to the left of it
		int32 edgeLowerID = graph.AddVertex(FVector2D(-10.0f, 0.0f))->ID;
		int32 edgeUpperID = graph.AddVertex(FVector2D(-10.0f, 10.0f))->ID;

		// Make a rectangle surrounding them both
		int32 rectLowerLeftID = graph.AddVertex(FVector2D(-20.0f, -20.0f))->ID;
		int32 rectLowerRightID = graph.AddVertex(FVector2D(30.0f, -20.0f))->ID;
		int32 rectUpperRightID = graph.AddVertex(FVector2D(30.0f, 20.0f))->ID;
		int32 rectUpperLeftID = graph.AddVertex(FVector2D(-20.0f, 20.0f))->ID;


		graph.AddEdge(tieLowerLeftID, tieUpperLeftID);
		graph.AddEdge(tieUpperLeftID, tieCenterID);
		graph.AddEdge(tieCenterID, tieLowerRightID);
		graph.AddEdge(tieLowerRightID, tieUpperRightID);
		graph.AddEdge(tieUpperRightID, tieCenterID);
		graph.AddEdge(tieCenterID, tieLowerLeftID);
		graph.AddEdge(tieLowerLeftID, tieLeftCenterID);

		graph.AddEdge(edgeLowerID, edgeUpperID);

		graph.AddEdge(rectLowerLeftID, rectLowerRightID);
		graph.AddEdge(rectLowerRightID, rectUpperRightID);
		graph.AddEdge(rectUpperRightID, rectUpperLeftID);
		graph.AddEdge(rectUpperLeftID, rectLowerLeftID);

		int32 numPolys = graph.CalculatePolygons();
		TestEqual(TEXT("Correct number of polygons"), numPolys, 6);

		for (auto& kvp : graph.GetPolygons())
		{
			const FGraph2DPolygon& poly = kvp.Value;
			if (poly.VertexIDs.Contains(tieLowerLeftID) && poly.bInterior)
			{
				TestEqual(TEXT("Correct bowtie left vertices"), poly.VertexIDs.Num(), 5);
				TestEqual(TEXT("Correct bowtie left perimeter"), poly.CachedPerimeterVertexIDs.Num(), 3);
			}
			else if (poly.VertexIDs.Contains(tieLowerRightID) && poly.bInterior)
			{
				TestEqual(TEXT("Correct bowtie right vertices"), poly.VertexIDs.Num(), 3);
				TestEqual(TEXT("Correct bowtie right perimeter"), poly.CachedPerimeterVertexIDs.Num(), 3);
			}
			else if (poly.VertexIDs.Contains(tieCenterID) && !poly.bInterior)
			{
				TestEqual(TEXT("Correct bowtie external vertices"), poly.VertexIDs.Num(), 6);
			}
			else if (poly.VertexIDs.Contains(edgeLowerID))
			{
				TestTrue(TEXT("Correct edge polygon winding"), !poly.bInterior);
				TestEqual(TEXT("Correct edge polygon vertices"), poly.VertexIDs.Num(), 2);
			}
			else if (poly.VertexIDs.Contains(rectLowerLeftID))
			{
				TestEqual(TEXT("Correct rectangle polygon vertices"), poly.VertexIDs.Num(), 4);
			}
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphRegularShapes, "Modumate.Graph.2D.RegularShapes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphRegularShapes::RunTest(const FString& Parameters)
	{
		Modumate::FGraph2D graph;

		int32 p1ID = graph.AddVertex(FVector2D(0.0f, 0.0f))->ID;
		int32 p2ID = graph.AddVertex(FVector2D(10.0f, 0.0f))->ID;
		int32 p3ID = graph.AddVertex(FVector2D(5.0f, 10.0f))->ID;

		graph.AddEdge(p1ID, p2ID);
		graph.AddEdge(p2ID, p3ID);
		graph.AddEdge(p3ID, p1ID);

		int32 p4ID = graph.AddVertex(FVector2D(2.0f, 2.0f))->ID;
		int32 p5ID = graph.AddVertex(FVector2D(8.0f, 2.0f))->ID;
		int32 p6ID = graph.AddVertex(FVector2D(5.0f, 4.0f))->ID;

		graph.AddEdge(p4ID, p5ID);
		graph.AddEdge(p5ID, p6ID);
		graph.AddEdge(p6ID, p4ID);

		int32 p7ID = graph.AddVertex(FVector2D(-10.0f, -10.0f))->ID;
		int32 p8ID = graph.AddVertex(FVector2D(-10.0f, 20.0f))->ID;
		int32 p9ID = graph.AddVertex(FVector2D(20.0f, 20.0f))->ID;
		int32 p10ID = graph.AddVertex(FVector2D(20.0f, -10.0f))->ID;

		graph.AddEdge(p7ID, p8ID);
		graph.AddEdge(p8ID, p9ID);
		graph.AddEdge(p9ID, p10ID);
		graph.AddEdge(p10ID, p7ID);

		int32 numPolys = graph.CalculatePolygons();
		return (numPolys == 6);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphPerimeter, "Modumate.Graph.2D.Perimeter", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphPerimeter::RunTest(const FString& Parameters)
	{
		Modumate::FGraph2D graph;

		// Create three nested squares, connected by bridges to test various traversals and perimeter windings

		int32 vInnerLLID = graph.AddVertex(FVector2D(25.0f, 25.0f))->ID;
		int32 vInnerLRID = graph.AddVertex(FVector2D(50.0f, 25.0f))->ID;
		int32 vInnerURID = graph.AddVertex(FVector2D(50.0f, 50.0f))->ID;
		int32 vInnerULID = graph.AddVertex(FVector2D(25.0f, 50.0f))->ID;
		TSet<int32> innerVertexIDs({ vInnerLLID, vInnerLRID, vInnerURID, vInnerULID });

		int32 eInnerLowerID = graph.AddEdge(vInnerLLID, vInnerLRID)->ID;
		int32 eInnerRightID = graph.AddEdge(vInnerLRID, vInnerURID)->ID;
		int32 eInnerUpperID = graph.AddEdge(vInnerURID, vInnerULID)->ID;
		int32 eInnerLeftID = graph.AddEdge(vInnerULID, vInnerLLID)->ID;

		int32 vMiddleLLID = graph.AddVertex(FVector2D(10.0f, 10.0f))->ID;
		int32 vMiddleLRID = graph.AddVertex(FVector2D(90.0f, 10.0f))->ID;
		int32 vMiddleURID = graph.AddVertex(FVector2D(90.0f, 90.0f))->ID;
		int32 vMiddleULID = graph.AddVertex(FVector2D(10.0f, 90.0f))->ID;
		TSet<int32> middleVertexIDs({ vMiddleLLID, vMiddleLRID, vMiddleURID, vMiddleULID });

		int32 eInnerBridgeID = graph.AddEdge(vInnerURID, vMiddleURID)->ID;

		int32 eMiddleLowerID = graph.AddEdge(vMiddleLLID, vMiddleLRID)->ID;
		int32 eMiddleRightID = graph.AddEdge(vMiddleLRID, vMiddleURID)->ID;
		int32 eMiddleUpperID = graph.AddEdge(vMiddleURID, vMiddleULID)->ID;
		int32 eMiddleLeftID = graph.AddEdge(vMiddleULID, vMiddleLLID)->ID;

		int32 vOuterLLID = graph.AddVertex(FVector2D(  0.0f,   0.0f))->ID;
		int32 vOuterLRID = graph.AddVertex(FVector2D(100.0f,   0.0f))->ID;
		int32 vOuterURID = graph.AddVertex(FVector2D(100.0f, 100.0f))->ID;
		int32 vOuterULID = graph.AddVertex(FVector2D(  0.0f, 100.0f))->ID;
		TSet<int32> outerVertexIDs({ vOuterLLID, vOuterLRID, vOuterURID, vOuterULID });

		int32 eOuterLowerID = graph.AddEdge(vOuterLLID, vOuterLRID)->ID;
		int32 eOuterRight = graph.AddEdge(vOuterLRID, vOuterURID)->ID;
		int32 eOuterUpper = graph.AddEdge(vOuterURID, vOuterULID)->ID;
		int32 eOuterLeftID = graph.AddEdge(vOuterULID, vOuterLLID)->ID;

		int32 eOuterBridgeID = graph.AddEdge(vOuterLLID, vMiddleLLID)->ID;

		int32 numPolys = graph.CalculatePolygons();
		TestEqual(TEXT("Num polygons"), numPolys, 4);

		for (auto& kvp : graph.GetPolygons())
		{
			const FGraph2DPolygon& poly = kvp.Value;
			const TArray<int32>& vertices = poly.VertexIDs;
			const TArray<FGraphSignedID>& edges = poly.Edges;
			int32 numVertices = vertices.Num();

			if (numVertices == 4)
			{
				if (vertices.Contains(vInnerLLID))
				{
					TestTrue(TEXT("Inner island polygon is interior"), poly.bInterior);
					TestTrue(TEXT("Inner island polygon has correct perimeter"), (poly.CachedPerimeterVertexIDs.Num() == 4) &&
						(TSet<int32>(poly.CachedPerimeterVertexIDs).Intersect(innerVertexIDs).Num() == 4));
				}
				else if (vertices.Contains(vMiddleLLID))
				{
					TestTrue(TEXT("Middle island polygon is interior"), poly.bInterior);
					TestTrue(TEXT("Middle island polygon has correct perimeter"), (poly.CachedPerimeterVertexIDs.Num() == 4) &&
						(TSet<int32>(poly.CachedPerimeterVertexIDs).Intersect(middleVertexIDs).Num() == 4));
				}
				else if (vertices.Contains(vOuterLLID))
				{
					TestTrue(TEXT("Outer perimeter polygon is exterior"), !poly.bInterior);
				}
				else
				{
					AddError(TEXT("Unknown polygon with 4 vertices!"));
				}
			}
			else
			{
				TSet<int32> uniqueVertices(vertices);
				int32 numUniqueVertices = uniqueVertices.Num();

				if (edges.Contains(eInnerBridgeID) || edges.Contains(-eInnerBridgeID))
				{
					TestTrue(TEXT("Non-rectangular inner polygon has correct vertices"), (numVertices == 10) && (numUniqueVertices == 8) &&
						(uniqueVertices.Intersect(innerVertexIDs).Num() == 4) && ((uniqueVertices.Intersect(middleVertexIDs).Num() == 4)));
					TestTrue(TEXT("Non-rectangular inner polygon is interior"), poly.bInterior);
					TestTrue(TEXT("Non-rectangular inner polygon has correct perimeter"), (poly.CachedPerimeterVertexIDs.Num() == 4) &&
						(TSet<int32>(poly.CachedPerimeterVertexIDs).Intersect(middleVertexIDs).Num() == 4));
				}
				else if (edges.Contains(eOuterBridgeID) || edges.Contains(-eOuterBridgeID))
				{
					TestTrue(TEXT("Non-rectangular outer polygon has correct vertices"), (numVertices == 10) && (numUniqueVertices == 8) &&
						(uniqueVertices.Intersect(middleVertexIDs).Num() == 4) && ((uniqueVertices.Intersect(outerVertexIDs).Num() == 4)));
					TestTrue(TEXT("Non-rectangular outer polygon is interior"), poly.bInterior);
					TestTrue(TEXT("Non-rectangular outer polygon has correct perimeter"), (poly.CachedPerimeterVertexIDs.Num() == 4) &&
						(TSet<int32>(poly.CachedPerimeterVertexIDs).Intersect(outerVertexIDs).Num() == 4));
				}
				else
				{
					AddError(FString::Printf(TEXT("Unknown polygon with not %d vertices!"), numVertices));
				}
			}
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphSerialization, "Modumate.Graph.2D.Serialization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphSerialization::RunTest(const FString& Parameters)
	{
		Modumate::FGraph2D graph;

		int32 p1ID = graph.AddVertex(FVector2D(0.0f, 0.0f))->ID;
		int32 p2ID = graph.AddVertex(FVector2D(10.0f, 0.0f))->ID;
		int32 p3ID = graph.AddVertex(FVector2D(10.0f, 10.0f))->ID;
		int32 p4ID = graph.AddVertex(FVector2D(0.0f, 10.0f))->ID;
		int32 p5ID = graph.AddVertex(FVector2D(15.0f, 0.0f))->ID;
		int32 p6ID = graph.AddVertex(FVector2D(15.0f, 10.0f))->ID;

		graph.AddEdge(p1ID, p2ID);
		graph.AddEdge(p2ID, p3ID);
		graph.AddEdge(p3ID, p4ID);
		graph.AddEdge(p4ID, p1ID);
		graph.AddEdge(p2ID, p5ID);
		graph.AddEdge(p5ID, p6ID);
		graph.AddEdge(p6ID, p3ID);

		graph.CalculatePolygons();
		TestEqual((TEXT("Num Computed Polygons")), graph.GetPolygons().Num(), 3);

		FGraph2DRecord graphRecord;
		bool bSaveSuccess = graph.ToDataRecord(&graphRecord);
		TestTrue(TEXT("Graph Save Success"), bSaveSuccess);

		FString graphJSONString;
		bool bJSONSuccess = FJsonObjectConverter::UStructToJsonObjectString(graphRecord, graphJSONString);
		TestTrue(TEXT("Graph Record JSON Success"), bJSONSuccess);

		FString expectedJSONString(TEXT(
"{"
"	\"vertices\":"
"	{"
"		\"1\": {\"x\": 0, \"y\": 0},"
"		\"2\": {\"x\": 10, \"y\": 0},"
"		\"3\": {\"x\": 10, \"y\": 10},"
"		\"4\": {\"x\": 0, \"y\": 10},"
"		\"5\": {\"x\": 15, \"y\": 0},"
"		\"6\": {\"x\": 15, \"y\": 10}"
"	},"
"	\"edges\":"
"	{"
"		\"1\": {\"vertexIds\": [1, 2]},"
"		\"2\": {\"vertexIds\": [2, 3]},"
"		\"3\": {\"vertexIds\": [3, 4]},"
"		\"4\": {\"vertexIds\": [4, 1]},"
"		\"5\": {\"vertexIds\": [2, 5]},"
"		\"6\": {\"vertexIds\": [5, 6]},"
"		\"7\": {\"vertexIds\": [6, 3]}"
"	},"
"	\"polygons\":"
"	{"
"		\"2\": { \"vertexIds\": [2, 1, 4, 3], \"bInterior\" : true, \"containingFaceId\" : 0, \"containedFaceIds\" : []},"
"		\"3\": { \"vertexIds\": [2, 3, 6, 5], \"bInterior\" : true, \"containingFaceId\" : 0, \"containedFaceIds\" : []}"
"	}"
"}"
		));

		expectedJSONString.ReplaceInline(TEXT(" "), TEXT(""));
		expectedJSONString.ReplaceInline(TEXT("\t"), TEXT(""));
		expectedJSONString.ReplaceInline(TEXT("\r"), TEXT(""));
		expectedJSONString.ReplaceInline(TEXT("\n"), TEXT(""));
		graphJSONString.ReplaceInline(TEXT(" "), TEXT(""));
		graphJSONString.ReplaceInline(TEXT("\t"), TEXT(""));
		graphJSONString.ReplaceInline(TEXT("\r"), TEXT(""));
		graphJSONString.ReplaceInline(TEXT("\n"), TEXT(""));

		TestEqualInsensitive(TEXT("Graph JSON string"), *expectedJSONString, *graphJSONString);

		Modumate::FGraph2D deserializedGraph;
		bool bLoadSuccess = deserializedGraph.FromDataRecord(&graphRecord);
		TestTrue(TEXT("Graph Load Success"), bLoadSuccess);

		TestEqual(TEXT("Num Loaded Polygons"), deserializedGraph.GetPolygons().Num(), 2);

		return true;
	}

	// 2D graph test helper functions
	void ApplyDeltas(FAutomationTestBase *Test, FGraph2D &Graph, TArray<FGraph2DDelta> &Deltas)
	{
		Test->TestTrue(TEXT("Apply Deltas"), Graph.ApplyDeltas(Deltas));
	}

	void ApplyInverseDeltas(FAutomationTestBase *Test, FGraph2D &Graph, TArray<FGraph2DDelta> &Deltas)
	{
		Test->TestTrue(TEXT("Apply Inverse Deltas"), Graph.ApplyInverseDeltas(Deltas));
	}

	void TestGraph(FAutomationTestBase *Test, FGraph2D &Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		if (TestNumFaces != -1)
		{
			Test->TestEqual((TEXT("Num Faces")), Graph.GetPolygons().Num(), TestNumFaces);
		}
		if (TestNumVertices != -1)
		{
			Test->TestEqual((TEXT("Num Vertices")), Graph.GetVertices().Num(), TestNumVertices);
		}
		if (TestNumEdges != -1)
		{
			Test->TestEqual((TEXT("Num Edges")), Graph.GetEdges().Num(), TestNumEdges);
		}
	}

	void TestModifiedGraphObjects(FAutomationTestBase *Test, FGraph2D &Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		TArray<int32> OutModifiedVertices, OutModifiedEdges, OutModifiedPolygons;
		Graph.ClearModifiedObjects(OutModifiedVertices, OutModifiedEdges, OutModifiedPolygons);

		if (TestNumFaces != -1)
		{
			Test->TestEqual((TEXT("Num Modified Faces")), OutModifiedPolygons.Num(), TestNumFaces);
		}
		if (TestNumVertices != -1)
		{
			Test->TestEqual((TEXT("Num Modified Vertices")), OutModifiedVertices.Num(), TestNumVertices);
		}
		if (TestNumEdges != -1)
		{
			Test->TestEqual((TEXT("Num Modified Edges")), OutModifiedEdges.Num(), TestNumEdges);
		}
	}

	void TestDeltas(FAutomationTestBase *Test, TArray<FGraph2DDelta> &Deltas, FGraph2D &Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1, bool bResetDeltas = true)
	{
		ApplyDeltas(Test, Graph, Deltas);
		TestGraph(Test, Graph, TestNumFaces, TestNumVertices, TestNumEdges);
		if (bResetDeltas)
		{
			Deltas.Reset();
		}
	}

	void TestDeltasAndResetGraph(FAutomationTestBase *Test, TArray<FGraph2DDelta> &Deltas, FGraph2D& Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		// make sure amounts of objects match before and after the test
		int32 resetNumFaces = Graph.GetPolygons().Num();
		int32 resetNumVertices = Graph.GetVertices().Num();
		int32 resetNumEdges = Graph.GetEdges().Num();

		ApplyDeltas(Test, Graph, Deltas);
		TestGraph(Test, Graph, TestNumFaces, TestNumVertices, TestNumEdges);
		ApplyInverseDeltas(Test, Graph, Deltas);
		TestGraph(Test, Graph, resetNumFaces, resetNumVertices, resetNumEdges);

		Deltas.Reset();
	}

	bool LoadGraphs(const FString &path, TMap<int32, FGraph2D> &OutGraphs, int32 &NextID)
	{
		FString scenePathname = FPaths::ProjectDir() / UModumateGameInstance::TestScriptRelativePath / path;
		FModumateDocumentHeader docHeader;
		FMOIDocumentRecord docRec;

		if (!FModumateSerializationStatics::TryReadModumateDocumentRecord(scenePathname, docHeader, docRec))
		{
			return false;
		}

		for (auto& kvp : docRec.SurfaceGraphs)
		{
			OutGraphs.Add(kvp.Key, FGraph2D(kvp.Key));
			FGraph2D &OutGraph = OutGraphs[kvp.Key];
			if (!OutGraph.FromDataRecord(&kvp.Value))
			{
				return false;
			}

			for (auto& vertexkvp : OutGraph.GetVertices())
			{
				NextID = FMath::Max(NextID, vertexkvp.Key);
			}

			for (auto& edgekvp : OutGraph.GetEdges())
			{
				NextID = FMath::Max(NextID, edgekvp.Key);
			}

			for (auto& polykvp : OutGraph.GetPolygons())
			{
				NextID = FMath::Max(NextID, polykvp.Key);
			}
		}

		return true;
	}

	bool LoadGraph(const FString &path, FGraph2D &OutGraph, int32 &NextID)
	{
		FString scenePathname = FPaths::ProjectDir() / UModumateGameInstance::TestScriptRelativePath / path;
		FModumateDocumentHeader docHeader;
		FMOIDocumentRecord docRec;

		if (!FModumateSerializationStatics::TryReadModumateDocumentRecord(scenePathname, docHeader, docRec))
		{
			return false;
		}

		if (docRec.SurfaceGraphs.Num() > 1)
		{
			return false;
		}


		for (auto& kvp : docRec.SurfaceGraphs)
		{
			if (!OutGraph.FromDataRecord(&kvp.Value))
			{
				return false;
			}

			for (auto& vertexkvp : OutGraph.GetVertices())
			{
				NextID = FMath::Max(NextID, vertexkvp.Key);
			}

			for (auto& edgekvp : OutGraph.GetEdges())
			{
				NextID = FMath::Max(NextID, edgekvp.Key);
			}

			for (auto& polykvp : OutGraph.GetPolygons())
			{
				NextID = FMath::Max(NextID, polykvp.Key);
			}
			break;
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DAddVertex, "Modumate.Graph.2D.AddVertex", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DAddVertex::RunTest(const FString& Parameters)
	{
		FGraph2D graph(1);
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		FVector2D position = FVector2D(100.0f, 100.0f);
		
		TestTrue(TEXT("Add Vertex"),
			graph.AddVertex(deltas, NextID, position));

		TestDeltas(this, deltas, graph, 0, 1, 0);

		TestModifiedGraphObjects(this, graph, 0, 1, 0);
		TestModifiedGraphObjects(this, graph, 0, 0, 0);

		TestTrue(TEXT("Add Duplicate Vertex"),
			graph.AddVertex(deltas, NextID, position));

		TestDeltas(this, deltas, graph, 0, 1, 0);
		TestModifiedGraphObjects(this, graph, 0, 0, 0);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DAddEdge, "Modumate.Graph.2D.AddEdge", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DAddEdge::RunTest(const FString& Parameters)
	{
		FGraph2D graph(2);
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> positions = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 100.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, positions[0], positions[1]));

		TestDeltas(this, deltas, graph, 1, 2, 1);
		TestModifiedGraphObjects(this, graph, 1, 2, 1);

		TestTrue(TEXT("Add Duplicate Edge"),
			graph.AddEdge(deltas, NextID, positions[0], positions[1]));

		TestDeltas(this, deltas, graph, 1, 2, 1);
		TestModifiedGraphObjects(this, graph, 0, 0, 0);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DDeleteObjects, "Modumate.Graph.2D.DeleteObjects", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DDeleteObjects::RunTest(const FString& Parameters)
	{
		FGraph2D graph(3);
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> positions = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(200.0f, 200.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, positions[0], positions[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, positions[1], positions[2]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		for (auto& edgekvp : graph.GetEdges())
		{
			TestTrue(TEXT("Delete Edge"),
				graph.DeleteObjects(deltas, NextID, {}, { edgekvp.Key }));
			TestDeltasAndResetGraph(this, deltas, graph, 1, 2, 1);
		}

		for (auto& vertexkvp : graph.GetVertices())
		{
			int32 numConnectedEdges = vertexkvp.Value.Edges.Num();
			int32 expectedNumEdges = numConnectedEdges == 2 ? 0 : 1;
			int32 expectedNumVertices = numConnectedEdges == 2 ? 0 : 2;
			int32 expectedNumPolygons = numConnectedEdges == 2 ? 0 : 1;

			TestTrue(TEXT("Delete Vertex"),
				graph.DeleteObjects(deltas, NextID, { vertexkvp.Key }, {}));

			TestDeltasAndResetGraph(this, deltas, graph, expectedNumPolygons, expectedNumVertices, expectedNumEdges);
		}

		TArray<int32> allEdges, allVertices;
		graph.GetEdges().GenerateKeyArray(allEdges);
		graph.GetVertices().GenerateKeyArray(allVertices);

		TestTrue(TEXT("Delete All Objects"),
			graph.DeleteObjects(deltas, NextID, allVertices, allEdges));
		TestDeltasAndResetGraph(this, deltas, graph, 0, 0, 0);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DPreviousObjects, "Modumate.Graph.2D.PreviousObjects", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DPreviousObjects::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(200.0f, 0.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[2]));
		TestDeltas(this, deltas, graph, 1, 2, 1, false);

		TSet<int32> addedEdgeIDs;
		graph.AggregateAddedEdges(deltas, addedEdgeIDs, vertices[0], vertices[2]);
		deltas.Reset();
		if (addedEdgeIDs.Num() != 1)
		{
			return false;
		}
		int32 previousEdgeID = addedEdgeIDs.Array()[0];

		TestTrue(TEXT("Add Overlapping edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 1, 3, 2, false);

		addedEdgeIDs.Reset();
		graph.AggregateAddedEdges(deltas, addedEdgeIDs, vertices[0], vertices[2]);
		int32 expectedIDs = 2;
		if (addedEdgeIDs.Num() != expectedIDs)
		{
			return false;
		}

		int32 checkedIDs = 0;
		bool bCheckedPreviousObject = false;
		for (auto& delta : deltas)
		{
			for (auto& edgekvp : delta.EdgeAdditions)
			{
				int32 edgeID = edgekvp.Key;
				if (addedEdgeIDs.Contains(edgeID))
				{
					auto& parents = delta.EdgeAdditions[edgeID].ParentObjIDs;
					TestTrue(TEXT("correct previous object"),
						parents.Num() == 1 && parents[0] == previousEdgeID);
					checkedIDs++;
				}
			}

			for (auto& edgekvp : delta.EdgeDeletions)
			{
				int32 edgeID = edgekvp.Key;
				if (edgeID == previousEdgeID)
				{
					auto& parents = delta.EdgeDeletions[edgeID].ParentObjIDs;
					TestTrue(TEXT("correct amount of current objects"),
						parents.Num() == 2);
					for (int32 id : parents)
					{
						TestTrue(TEXT("correct current object id"),
							addedEdgeIDs.Contains(id));
					}
					bCheckedPreviousObject = true;
				}
			}
		}

		TestTrue(TEXT("correct amount of current objects"),
			checkedIDs == expectedIDs);
		TestTrue(TEXT("correct amount of previous objects"),
			bCheckedPreviousObject);

		deltas.Reset();

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DSplitEdges, "Modumate.Graph.2D.SplitEdges", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DSplitEdges::RunTest(const FString& Parameters)
	{
		// Create grid of edges

		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;
		int32 gridDimension = 3;

		// vertical edges
		for (int i = 0; i <= gridDimension; i++)
		{
			FVector2D startPosition = FVector2D(i*100.0f, 0.0f);
			FVector2D endPosition = FVector2D(i*100.0f, gridDimension*100.0f);

			TestTrue(TEXT("Add Horizontal Edge"),
				graph.AddEdge(deltas, NextID, startPosition, endPosition));
			TestDeltas(this, deltas, graph, i+1, 2*(i+1), i+1);
		}

		int32 expectedVertices  = graph.GetVertices().Num();
		int32 expectedEdges = graph.GetEdges().Num();
		int32 expectedPolygons = 0;
		// horizontal edges
		for (int i = 0; i <= gridDimension; i++)
		{
			FVector2D startPosition = FVector2D(0.0f, i*100.0f);
			FVector2D endPosition = FVector2D(gridDimension*100.0f, i*100.0f);

			TestTrue(TEXT("Add Vertical Edge"),
				graph.AddEdge(deltas, NextID, startPosition, endPosition));

			if (i == 0 || i == gridDimension)
			{
				expectedEdges += gridDimension;
			}
			else
			{
				expectedEdges += 2*gridDimension + 1;
				expectedVertices += gridDimension + 1;
			}

			if (i == 0)
			{
				expectedPolygons += 1;
			}
			else
			{
				expectedPolygons += gridDimension;
			}

			TestDeltas(this, deltas, graph, expectedPolygons, expectedVertices, expectedEdges);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DColinearEdges, "Modumate.Graph.2D.ColinearEdges", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DColinearEdges::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(200.0f, 0.0f),
			FVector2D(300.0f, 0.0f),
			FVector2D(400.0f, 0.0f),
			FVector2D(500.0f, 0.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);
		TestModifiedGraphObjects(this, graph, 1, 2, 1);
		TestModifiedGraphObjects(this, graph, 0, 0, 0);

		TestTrue(TEXT("Add Overlapping edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[2]));
		TestModifiedGraphObjects(this, graph, 1, 1, 0);
		TestDeltas(this, deltas, graph, 1, 3, 2);
		TestModifiedGraphObjects(this, graph, 1, 2, 1);

		TestTrue(TEXT("Add another edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[4]));
		TestDeltas(this, deltas, graph, 2, 5, 3);
		TestModifiedGraphObjects(this, graph, 1, 2, 1);

		TestTrue(TEXT("Add another overlapping edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[5]));
		TestDeltas(this, deltas, graph, 1, 6, 5);
		TestModifiedGraphObjects(this, graph, 1, 4, 2);

		TestTrue(TEXT("Add edge covered by several existing edges"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[5]));
		TestDeltas(this, deltas, graph, 1, 6, 5);
		TestModifiedGraphObjects(this, graph, 0, 0, 0);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DColinearEdges2, "Modumate.Graph.2D.ColinearEdges2", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DColinearEdges2::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(200.0f, 0.0f),
			FVector2D(300.0f, 0.0f),
			FVector2D(400.0f, 0.0f),
			FVector2D(500.0f, 0.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[4]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add containing edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[5]));
		TestDeltasAndResetGraph(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add contained edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltasAndResetGraph(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add overlapping edge containing start vertex"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[2]));
		TestDeltasAndResetGraph(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add overlapping edge containing end vertex"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[5]));
		TestDeltasAndResetGraph(this, deltas, graph, 1, 4, 3);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DMoveVertices, "Modumate.Graph.2D.MoveVertices", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DMoveVertices::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));

		TSet<int32> firstVertexIDs;
		graph.AggregateAddedVertices(deltas, firstVertexIDs);

		TestDeltas(this, deltas, graph, 1, 2, 1);

		vertices = {
			FVector2D(200.0f, 0.0f),
			FVector2D(300.0f, 0.0f),
		};
		TestTrue(TEXT("Add another Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));

		TSet<int32> secondVertexIDs;
		graph.AggregateAddedVertices(deltas, secondVertexIDs);

		TestDeltas(this, deltas, graph, 2, 4, 2);

		TestTrue(TEXT("Basic move"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(50.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 2, 4, 2);

		TestTrue(TEXT("Join one vertex"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(100.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Overlap edges"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(150.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Make edges the same"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(200.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 1, 2, 1);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DValidateGraph, "Modumate.Graph.2D.ValidateGraph", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DValidateGraph::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(0.0f, 100.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[2]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltas(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[0]));
		TestDeltas(this, deltas, graph, 2, 4, 4);

		// Make current vertices into the bounds
		int32 boundsID = NextID++;
		TPair<int32, TArray<int32>> outerBounds(boundsID, TArray<int32>());
		TMap<int32, TArray<int32>> innerBounds;
		graph.GetVertices().GenerateKeyArray(outerBounds.Value);

		TestTrue(TEXT("Add Bounds"),
			graph.SetBounds(deltas, outerBounds, innerBounds));
		TestDeltas(this, deltas, graph, 2, 4, 4);

		TestTrue(TEXT("Add Edge inside poly"),
			graph.AddEdge(deltas, NextID, FVector2D(10.0f, 10.0f), FVector2D(20.0f, 20.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 3, 6, 5);

		// TODO: potentially test magnitudes of objects (especially polygons) for the cases where
		// adding edges would be successful
		TestTrue(TEXT("Add Edge spanning poly"),
			graph.AddEdge(deltas, NextID, FVector2D(0.0f, 0.0f), FVector2D(100.0f, 100.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 3, 4, 5);

		TestTrue(TEXT("Add edge outside of bounding poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(110.0f, 110.0f), FVector2D(120.0f, 120.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add edge adjacent, but outside of bounding poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(0.0f, 0.0f), FVector2D(-10.0f, 0.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add edge adjacent, but inside of bounding poly"),
			graph.AddEdge(deltas, NextID, FVector2D(10.0f, 0.0f), FVector2D(10.0f, 10.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 2, 6, 6);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DMoveEdges, "Modumate.Graph.2D.MoveEdges", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DMoveEdges::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(50.0f, 0.0f),
			FVector2D(50.0f, 100.0f),
			FVector2D(0.0f, 25.0f),
			FVector2D(25.0f, 50.0f),
			FVector2D(0.0f, 75.0f)
		};

		TestTrue(TEXT("first Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("second Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[4]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TArray<int32> verticesToMove;
		graph.GetVertices().GenerateKeyArray(verticesToMove);

		TestTrue(TEXT("third Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 2, 5, 3);

		TestTrue(TEXT("move first two across third edge"),
			graph.MoveVertices(deltas, NextID, verticesToMove, FVector2D(40.0f, 0.0f)));
		TestDeltas(this, deltas, graph, 2, 7, 7);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DValidateGraphConcave, "Modumate.Graph.2D.ValidateGraphConcave", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DValidateGraphConcave::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(50.0f, 90.0f),
			FVector2D(0.0f, 100.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[2]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltas(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[4]));
		TestDeltas(this, deltas, graph, 1, 5, 4);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[4], vertices[0]));
		TestDeltas(this, deltas, graph, 2, 5, 5);

		// Make current vertices into the bounds
		int32 boundsID = NextID++;
		TPair<int32, TArray<int32>> outerBounds(boundsID, TArray<int32>());
		TMap<int32, TArray<int32>> innerBounds;
		graph.GetVertices().GenerateKeyArray(outerBounds.Value);

		TestTrue(TEXT("Add Bounds"),
			graph.SetBounds(deltas, outerBounds, innerBounds));
		TestDeltas(this, deltas, graph, 2, 5, 5);

		TestTrue(TEXT("Add Edge inside poly"),
			graph.AddEdge(deltas, NextID, FVector2D(10.0f, 10.0f), FVector2D(20.0f, 20.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 3, 7, 6);

		TestTrue(TEXT("Add Edge outside poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(110.0f, 110.0f), FVector2D(120.0f, 120.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner outside and away from poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(50.0f, 90.0f), FVector2D(50.0f, 100.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner outside and towards poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(50.0f, 100.0f), FVector2D(50.0f, 90.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner inside and away from poly"),
			graph.AddEdge(deltas, NextID, FVector2D(50.0f, 90.0f), FVector2D(50.0f, 80.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner inside and towards poly"),
			graph.AddEdge(deltas, NextID, FVector2D(50.0f, 80.0f), FVector2D(50.0f, 90.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner colinear with edge"),
			graph.AddEdge(deltas, NextID, FVector2D(50.0f, 90.0f), FVector2D(75.0f, 95.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner colinear with edge"),
			graph.AddEdge(deltas, NextID, FVector2D(50.0f, 90.0f), FVector2D(25.0f, 95.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner slightly inside poly"),
			graph.AddEdge(deltas, NextID, FVector2D(50.0f, 90.0f), FVector2D(25.0f, 94.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on concave corner slightly outside poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(50.0f, 90.0f), FVector2D(25.0f, 96.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge across concave corner"),
			!graph.AddEdge(deltas, NextID, FVector2D(50.0f, 80.0f), FVector2D(50.0f, 100.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge across concave edge"),
			!graph.AddEdge(deltas, NextID, FVector2D(60.0f, 80.0f), FVector2D(60.0f, 100.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on convex corner inside poly"),
			graph.AddEdge(deltas, NextID, FVector2D(100.0f, 100.0f), FVector2D(75.0f, 75.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on convex corner outside poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(100.0f, 100.0f), FVector2D(125.0f, 125.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on convex corner slightly inside poly"),
			graph.AddEdge(deltas, NextID, FVector2D(100.0f, 100.0f), FVector2D(99.0f, 50.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge on convex corner slightly outside poly"),
			!graph.AddEdge(deltas, NextID, FVector2D(100.0f, 100.0f), FVector2D(101.0f, 50.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge across convex corner"),
			!graph.AddEdge(deltas, NextID, FVector2D(75.0f, 75.0f), FVector2D(125.0f, 125.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge across concave area, connecting corners"),
			!graph.AddEdge(deltas, NextID, FVector2D(0.0f, 100.0f), FVector2D(100.0f, 100.0f)));
		deltas.Reset();

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DValidateGraphHoles, "Modumate.Graph.2D.ValidateGraphHoles", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DValidateGraphHoles::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(0.0f, 100.0f)
		};

		TArray<FVector2D> holeVertices = {
			FVector2D(25.0f, 25.0f),
			FVector2D(75.0f, 25.0f),
			FVector2D(75.0f, 75.0f),
			FVector2D(25.0f, 75.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[2]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltas(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[0]));
		TestDeltas(this, deltas, graph, 2, 4, 4);

		int32 boundsID = NextID++;
		TPair<int32, TArray<int32>> outerBounds(boundsID, TArray<int32>());
		TMap<int32, TArray<int32>> innerBounds;
		graph.GetVertices().GenerateKeyArray(outerBounds.Value);

		TSet<int32> holeVertexIDs;
		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[0], holeVertices[1]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 3, 6, 5);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[1], holeVertices[2]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 3, 7, 6);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[2], holeVertices[3]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 3, 8, 7);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[3], holeVertices[0]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 4, 8, 8);

		// add hole
		innerBounds.Add(NextID++, holeVertexIDs.Array());

		TestTrue(TEXT("Add Bounds"),
			graph.SetBounds(deltas, outerBounds, innerBounds));
		TestDeltas(this, deltas, graph, 4, 8, 8);

		TestTrue(TEXT("Add Edge inside poly"),
			graph.AddEdge(deltas, NextID, FVector2D(10.0f, 10.0f), FVector2D(20.0f, 20.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 5);

		TestTrue(TEXT("Add Edge inside hole"),
			!graph.AddEdge(deltas, NextID, FVector2D(50.0f, 50.0f), FVector2D(60.0f, 60.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge spanning hole"),
			!graph.AddEdge(deltas, NextID, FVector2D(25.0f, 25.0f), FVector2D(75.0f, 75.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge from hole to bounds"),
			graph.AddEdge(deltas, NextID, FVector2D(25.0f, 25.0f), FVector2D(0.0f, 0.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 3);

		TestTrue(TEXT("Add Edge from side of hole to bounds"),
			graph.AddEdge(deltas, NextID, FVector2D(35.0f, 25.0f), FVector2D(0.0f, 0.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 3);

		TestTrue(TEXT("Add Edge colinear with hole on corner"),
			graph.AddEdge(deltas, NextID, FVector2D(25.0f, 25.0f), FVector2D(45.0f, 25.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 4);

		TestTrue(TEXT("Add Edge colinear with hole off corner"),
			graph.AddEdge(deltas, NextID, FVector2D(35.0f, 25.0f), FVector2D(45.0f, 25.0f)));
		TestDeltasAndResetGraph(this, deltas, graph, 4);

		TestTrue(TEXT("Add Edge spanning bounds through hole"),
			!graph.AddEdge(deltas, NextID, FVector2D(0.0f, 0.0f), FVector2D(100.0f, 100.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge from bounds spanning hole"),
			!graph.AddEdge(deltas, NextID, FVector2D(0.0f, 0.0f), FVector2D(75.0f, 75.0f)));
		deltas.Reset();

		TestTrue(TEXT("Add Edge from bounds intersecting hole corner"),
			!graph.AddEdge(deltas, NextID, FVector2D(0.0f, 0.0f), FVector2D(50.0f, 50.0f)));
		deltas.Reset();

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DContainment, "Modumate.Graph.2D.Containment", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DContainment::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(0.0f, 100.0f)
		};

		TArray<FVector2D> holeVertices = {
			FVector2D(25.0f, 25.0f),
			FVector2D(75.0f, 25.0f),
			FVector2D(75.0f, 75.0f),
			FVector2D(25.0f, 75.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[2]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltas(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[0]));
		TestDeltas(this, deltas, graph, 2, 4, 4);

		// Make current vertices into the bounds
		TArray<int32> outerBounds;
		graph.GetVertices().GenerateKeyArray(outerBounds);

		int32 containingPolyID = MOD_ID_NONE;

		for (auto& polykvp : graph.GetPolygons())
		{
			if (polykvp.Value.bInterior)
			{
				containingPolyID = polykvp.Key;
			}
		}
		TestTrue(TEXT("outer loop has interior polygon"),
			containingPolyID != MOD_ID_NONE);

		TSet<int32> holeVertexIDs;
		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[0], holeVertices[1]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 3, 6, 5);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[1], holeVertices[2]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 3, 7, 6);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[2], holeVertices[3]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 3, 8, 7);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, holeVertices[3], holeVertices[0]));
		graph.AggregateAddedVertices(deltas, holeVertexIDs);
		TestDeltas(this, deltas, graph, 4, 8, 8);

		int32 numContainingPolys = 0;
		for (auto& polykvp : graph.GetPolygons())
		{
			auto& interiorPolys = polykvp.Value.InteriorPolygons;
			if (interiorPolys.Num() != 0)
			{
				TestTrue(TEXT("containing polygon contains 2 faces"),
					interiorPolys.Num() == 2);
				for (int32 containedID : interiorPolys)
				{
					auto containedPoly = graph.FindPolygon(containedID);
					TestTrue(TEXT("contained poly exists and is contained by the same poly"),
						containedPoly != nullptr && containedPoly->ParentID == polykvp.Key);
				}

				numContainingPolys++;
			}
		}

		TestTrue(TEXT("right amount of containingPolys"),
			numContainingPolys == 1);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DStarShape, "Modumate.Graph.2D.StarShape", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DStarShape::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 25.0f),
			FVector2D(50.0f, 0.0f),
			FVector2D(0.0f, 75.0f),
			FVector2D(50.0f, 100.0f),
			FVector2D(100.0f, 50.0f)
		};

		TestTrue(TEXT("Add first Edge"),
			graph.AddEdge(deltas, NextID, vertices[4], vertices[0]));
		TestDeltas(this, deltas, graph, 1, 2, 1);

		TestTrue(TEXT("Add second Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[3]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Add third Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[1]));
		TestDeltas(this, deltas, graph, 2, 5, 5);

		TestTrue(TEXT("Add fourth Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[2]));
		TestDeltas(this, deltas, graph, 4, 8, 10);

		TestTrue(TEXT("Add fifth Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[4]));
		TestDeltas(this, deltas, graph, 7, 10, 15);
		
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DStarShapeWithBounds, "Modumate.Graph.2D.StarShapeWithBounds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DStarShapeWithBounds::RunTest(const FString& Parameters)
	{
		FGraph2D graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> outerVertices = {
			FVector2D(-100.0f, -100.0f),
			FVector2D(200.0f, -100.0f),
			FVector2D(200.0f, 200.0f),
			FVector2D(-100.0f, 200.0f)
		};

		TArray<FVector2D> vertices = {
			FVector2D(0.0f, 25.0f),
			FVector2D(50.0f, 0.0f),
			FVector2D(0.0f, 75.0f),
			FVector2D(50.0f, 100.0f),
			FVector2D(100.0f, 50.0f)
		};

		// bounds
		TestTrue(TEXT("Add first bounds Edge"),
			graph.AddEdge(deltas, NextID, outerVertices[0], outerVertices[1]));
		TestDeltas(this, deltas, graph, 1, 2, 1);
		
		TestTrue(TEXT("Add second bounds Edge"),
			graph.AddEdge(deltas, NextID, outerVertices[1], outerVertices[2]));
		TestDeltas(this, deltas, graph, 1, 3, 2);

		TestTrue(TEXT("Add third bounds Edge"),
			graph.AddEdge(deltas, NextID, outerVertices[2], outerVertices[3]));
		TestDeltas(this, deltas, graph, 1, 4, 3);

		TestTrue(TEXT("Add fourth bounds Edge"),
			graph.AddEdge(deltas, NextID, outerVertices[3], outerVertices[0]));
		TestDeltas(this, deltas, graph, 2, 4, 4);

		// Make current vertices into the bounds
		int32 boundsID = NextID++;
		TPair<int32, TArray<int32>> outerBounds(boundsID, TArray<int32>());
		TMap<int32, TArray<int32>> innerBounds;
		graph.GetVertices().GenerateKeyArray(outerBounds.Value);

		TestTrue(TEXT("Add Bounds"),
			graph.SetBounds(deltas, outerBounds, innerBounds));
		TestDeltas(this, deltas, graph, 2, 4, 4);

		// 5-sided star
		TestTrue(TEXT("Add first Edge"),
			graph.AddEdge(deltas, NextID, vertices[4], vertices[0]));
		TestDeltas(this, deltas, graph, 3, 6, 5);

		TestTrue(TEXT("Add second Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[3]));
		TestDeltas(this, deltas, graph, 3, 7, 6);

		TestTrue(TEXT("Add third Edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[1]));
		TestDeltas(this, deltas, graph, 4, 9, 9);

		TestTrue(TEXT("Add fourth Edge"),
			graph.AddEdge(deltas, NextID, vertices[1], vertices[2]));
		TestDeltas(this, deltas, graph, 6, 12, 14);

		TestTrue(TEXT("Add fifth Edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[4]));
		TestDeltas(this, deltas, graph, 9, 14, 19);
		
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DLoad, "Modumate.Graph.2D.Load", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DLoad::RunTest(const FString& Parameters)
	{
		TMap<int32, FGraph2D> allGraphs;
		int32 NextID = 1;

		if (!LoadGraphs(TEXT("Graph/surface_graph_load.mdmt"), allGraphs, NextID))
		{
			return false;
		}

		if (!allGraphs.Num() == 1)
		{
			return false;
		}

		FGraph2D graph;
		for (auto& kvp : allGraphs)
		{
			graph = kvp.Value;
			break;
		}

		TestGraph(this, graph, 1, 4, 4);
		
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DLoadStar, "Modumate.Graph.2D.LoadStar", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DLoadStar::RunTest(const FString& Parameters)
	{
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;
		FGraph2D graph;

		if (!LoadGraph(TEXT("Graph/star_test.mdmt"), graph, NextID))
		{
			return false;
		}

		TestGraph(this, graph, 5, 12, 14);

		auto startVertex = graph.FindVertex(21);
		auto endVertex = graph.FindVertex(36);

		TestTrue(TEXT("target vertices exist"), startVertex != nullptr && endVertex != nullptr);

		TestTrue(TEXT("Add edge completing star"),
			graph.AddEdge(deltas, NextID, startVertex->Position, endVertex->Position));
		TestDeltas(this, deltas, graph, 9, 14, 19);
		
		return true;
	}
}
