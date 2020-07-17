#include "CoreMinimal.h"

#include "DocumentManagement/ModumateDelta.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph2DDelta.h"

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

		int32 p1ID = graph.AddVertex(FVector2D(0.0f, 10.0f))->ID;
		int32 p2ID = graph.AddVertex(FVector2D(10.0f, 5.0f))->ID;
		int32 p3ID = graph.AddVertex(FVector2D(20.0f, 0.0f))->ID;
		int32 p4ID = graph.AddVertex(FVector2D(20.0f, 10.0f))->ID;
		int32 p5ID = graph.AddVertex(FVector2D(0.0f, 0.0f))->ID;
		int32 p6ID = graph.AddVertex(FVector2D(5.0f, 5.0f))->ID;

		int32 p7ID = graph.AddVertex(FVector2D(-10.0f, 0.0f))->ID;
		int32 p8ID = graph.AddVertex(FVector2D(-10.0f, 10.0f))->ID;

		int32 p9ID = graph.AddVertex(FVector2D(-20.0f, -20.0f))->ID;
		int32 p10ID = graph.AddVertex(FVector2D(30.0f, -20.0f))->ID;
		int32 p11ID = graph.AddVertex(FVector2D(30.0f, 20.0f))->ID;
		int32 p12ID = graph.AddVertex(FVector2D(-20.0f, 20.0f))->ID;


		graph.AddEdge(p5ID, p1ID);
		graph.AddEdge(p1ID, p2ID);
		graph.AddEdge(p2ID, p3ID);
		graph.AddEdge(p3ID, p4ID);
		graph.AddEdge(p4ID, p2ID);
		graph.AddEdge(p2ID, p5ID);
		graph.AddEdge(p5ID, p6ID);

		graph.AddEdge(p7ID, p8ID);

		graph.AddEdge(p9ID, p10ID);
		graph.AddEdge(p10ID, p11ID);
		graph.AddEdge(p11ID, p12ID);
		graph.AddEdge(p12ID, p9ID);

		int32 numPolys = graph.CalculatePolygons();
		return (numPolys == 6);
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
		bool bSaveSuccess = graph.ToDataRecord(graphRecord);
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
"		\"2\": {\"edgeIds\": [-1, -4, -3, -2]},"
"		\"3\": {\"edgeIds\": [2, -7, -6, -5]}"
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
		bool bLoadSuccess = deserializedGraph.FromDataRecord(graphRecord);
		TestTrue(TEXT("Graph Load Success"), bLoadSuccess);

		TestEqual(TEXT("Num Loaded Polygons"), deserializedGraph.GetPolygons().Num(), 2);

		return true;
	}

	// 3D graph test helper functions
	void ApplyDeltas(FAutomationTestBase *Test, FGraph2D &Graph, TArray<FGraph2DDelta> &Deltas)
	{
		bool bValidDelta = true;
		for (auto& delta : Deltas)
		{
			Test->TestEqual(TEXT("Graph and delta ID equality"), Graph.GetID(), delta.ID);
			bValidDelta = Graph.ApplyDelta(delta);
		}
		Test->TestTrue(TEXT("Apply Deltas"), bValidDelta);
	}

	void ApplyInverseDeltas(FAutomationTestBase *Test, FGraph2D &Graph, TArray<FGraph2DDelta> &Deltas)
	{
		bool bValidDelta = true;
		for (int32 deltaIdx = Deltas.Num() - 1; deltaIdx >= 0; deltaIdx--)
		{
			bValidDelta = Graph.ApplyDelta(*Deltas[deltaIdx].MakeGraphInverse());
		}
		Test->TestTrue(TEXT("Apply Inverse Deltas"), bValidDelta);
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

	void TestCleanGraph(FAutomationTestBase *Test, FGraph2D &Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		TArray<int32> OutCleanedVertices, OutCleanedEdges, OutCleanedPolygons;
		Graph.CleanGraph(OutCleanedVertices, OutCleanedEdges, OutCleanedPolygons);

		if (TestNumFaces != -1)
		{
			Test->TestEqual((TEXT("Num Faces")), OutCleanedPolygons.Num(), TestNumFaces);
		}
		if (TestNumVertices != -1)
		{
			Test->TestEqual((TEXT("Num Vertices")), OutCleanedVertices.Num(), TestNumVertices);
		}
		if (TestNumEdges != -1)
		{
			Test->TestEqual((TEXT("Num Edges")), OutCleanedEdges.Num(), TestNumEdges);
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

		TestCleanGraph(this, graph, 0, 1, 0);
		TestCleanGraph(this, graph, 0, 0, 0);

		TestTrue(TEXT("Add Duplicate Vertex"),
			graph.AddVertex(deltas, NextID, position));

		TestDeltas(this, deltas, graph, 0, 1, 0);
		TestCleanGraph(this, graph, 0, 0, 0);

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

		TestDeltas(this, deltas, graph, 0, 2, 1);
		TestCleanGraph(this, graph, 0, 2, 1);

		TestTrue(TEXT("Add Duplicate Edge"),
			graph.AddEdge(deltas, NextID, positions[0], positions[1]));

		TestDeltas(this, deltas, graph, 0, 2, 1);
		TestCleanGraph(this, graph, 0, 0, 0);

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
		TestDeltas(this, deltas, graph, 0, 2, 1);

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, positions[1], positions[2]));
		TestDeltas(this, deltas, graph, 0, 3, 2);

		for (auto& edgekvp : graph.GetEdges())
		{
			TestTrue(TEXT("Delete Edge"),
				graph.DeleteObjects(deltas, {}, { edgekvp.Key }));
			TestDeltasAndResetGraph(this, deltas, graph, 0, 2, 1);
		}

		for (auto& vertexkvp : graph.GetVertices())
		{
			TestTrue(TEXT("Delete Vertex"),
				graph.DeleteObjects(deltas, { vertexkvp.Key }, {}));

			int32 numConnectedEdges = vertexkvp.Value.Edges.Num();
			int32 expectedNumEdges = numConnectedEdges == 2 ? 0 : 1;
			int32 expectedNumVertices = numConnectedEdges == 2 ? 0 : 2;

			TestDeltasAndResetGraph(this, deltas, graph, 0, expectedNumVertices, expectedNumEdges);
		}

		TArray<int32> allEdges, allVertices;
		graph.GetEdges().GenerateKeyArray(allEdges);
		graph.GetVertices().GenerateKeyArray(allVertices);

		TestTrue(TEXT("Delete All Objects"),
			graph.DeleteObjects(deltas, allVertices, allEdges));
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
		TestDeltas(this, deltas, graph, 0, 2, 1, false);

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
		TestDeltas(this, deltas, graph, 0, 3, 2, false);

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
			TestDeltas(this, deltas, graph, 0, 2*(i+1), i+1);
		}

		int32 expectedVertices  = graph.GetVertices().Num();
		int32 expectedEdges = graph.GetEdges().Num();
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
			TestDeltas(this, deltas, graph, 0, expectedVertices, expectedEdges);
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
		TestDeltas(this, deltas, graph, 0, 2, 1);
		TestCleanGraph(this, graph, 0, 2, 1);
		TestCleanGraph(this, graph, 0, 0, 0);

		TestTrue(TEXT("Add Overlapping edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[2]));
		TestCleanGraph(this, graph, 0, 0, 0);
		TestDeltas(this, deltas, graph, 0, 3, 2);
		TestCleanGraph(this, graph, 0, 2, 1);

		TestTrue(TEXT("Add another edge"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[4]));
		TestDeltas(this, deltas, graph, 0, 5, 3);
		TestCleanGraph(this, graph, 0, 2, 1);

		TestTrue(TEXT("Add another overlapping edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[5]));
		TestDeltas(this, deltas, graph, 0, 6, 5);
		TestCleanGraph(this, graph, 0, 4, 2);

		TestTrue(TEXT("Add edge covered by several existing edges"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[5]));
		TestDeltas(this, deltas, graph, 0, 6, 5);
		TestCleanGraph(this, graph, 0, 0, 0);

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
		TestDeltas(this, deltas, graph, 0, 2, 1);

		TestTrue(TEXT("Add containing edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[5]));
		TestDeltasAndResetGraph(this, deltas, graph, 0, 4, 3);

		TestTrue(TEXT("Add contained edge"),
			graph.AddEdge(deltas, NextID, vertices[2], vertices[3]));
		TestDeltasAndResetGraph(this, deltas, graph, 0, 4, 3);

		TestTrue(TEXT("Add overlapping edge containing start vertex"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[2]));
		TestDeltasAndResetGraph(this, deltas, graph, 0, 4, 3);

		TestTrue(TEXT("Add overlapping edge containing end vertex"),
			graph.AddEdge(deltas, NextID, vertices[3], vertices[5]));
		TestDeltasAndResetGraph(this, deltas, graph, 0, 4, 3);

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

		TestDeltas(this, deltas, graph, 0, 2, 1);

		vertices = {
			FVector2D(200.0f, 0.0f),
			FVector2D(300.0f, 0.0f),
		};

		TestTrue(TEXT("Add another Edge"),
			graph.AddEdge(deltas, NextID, vertices[0], vertices[1]));

		TSet<int32> secondVertexIDs;
		graph.AggregateAddedVertices(deltas, secondVertexIDs);

		TestDeltas(this, deltas, graph, 0, 4, 2);

		TestTrue(TEXT("Basic move"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(50.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 0, 4, 2);

		TestTrue(TEXT("Join one vertex"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(100.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 0, 3, 2);

		TestTrue(TEXT("Overlap edges"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(150.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 0, 4, 3);

		TestTrue(TEXT("Make edges the same"),
			graph.MoveVertices(deltas, NextID, firstVertexIDs.Array(), FVector2D(200.0f, 0.0f)));

		TestDeltasAndResetGraph(this, deltas, graph, 0, 2, 1);

		return true;
	}
}
