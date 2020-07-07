#include "CoreMinimal.h"

#include "DocumentManagement/ModumateDelta.h"
#include "Graph/ModumateGraph.h"
#include "Graph/Graph2DDelta.h"

namespace Modumate
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphDefaultTest, "Modumate.Graph.2D.Init", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphDefaultTest::RunTest(const FString& Parameters)
	{
		FGraph graph;
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphOneTriangle, "Modumate.Graph.2D.OneTriangle", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphOneTriangle::RunTest(const FString& Parameters)
	{
		Modumate::FGraph graph;

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
		Modumate::FGraph graph;

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
		Modumate::FGraph graph;

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
		Modumate::FGraph graph;

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
		Modumate::FGraph graph;

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

		Modumate::FGraph deserializedGraph;
		bool bLoadSuccess = deserializedGraph.FromDataRecord(graphRecord);
		TestTrue(TEXT("Graph Load Success"), bLoadSuccess);

		TestEqual(TEXT("Num Loaded Polygons"), deserializedGraph.GetPolygons().Num(), 2);

		return true;
	}

	// 3D graph test helper functions
	void ApplyDeltas(FAutomationTestBase *Test, FGraph &Graph, TArray<FGraph2DDelta> &Deltas)
	{
		bool bValidDelta = true;
		for (auto& delta : Deltas)
		{
			bValidDelta = Graph.ApplyDelta(delta);
		}
		Test->TestTrue(TEXT("Apply Deltas"), bValidDelta);
	}

	void ApplyInverseDeltas(FAutomationTestBase *Test, FGraph &Graph, TArray<FGraph2DDelta> &Deltas)
	{
		bool bValidDelta = true;
		for (int32 deltaIdx = Deltas.Num() - 1; deltaIdx >= 0; deltaIdx--)
		{
			bValidDelta = Graph.ApplyDelta(*Deltas[deltaIdx].MakeGraphInverse());
		}
		Test->TestTrue(TEXT("Apply Inverse Deltas"), bValidDelta);
	}

	void TestGraph(FAutomationTestBase *Test, FGraph &Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
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

	void TestDeltas(FAutomationTestBase *Test, TArray<FGraph2DDelta> &Deltas, FGraph &Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1, bool bResetDeltas = true)
	{
		ApplyDeltas(Test, Graph, Deltas);
		TestGraph(Test, Graph, TestNumFaces, TestNumVertices, TestNumEdges);
		if (bResetDeltas)
		{
			Deltas.Reset();
		}
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DAddVertex, "Modumate.Graph.2D.AddVertex", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DAddVertex::RunTest(const FString& Parameters)
	{
		FGraph graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		FVector2D position = FVector2D(100.0f, 100.0f);
		
		TestTrue(TEXT("Add Vertex"),
			graph.AddVertex(deltas, NextID, position));

		TestDeltas(this, deltas, graph, 0, 1, 0);

		TestTrue(TEXT("Add Duplicate Vertex"),
			graph.AddVertex(deltas, NextID, position));

		TestDeltas(this, deltas, graph, 0, 1, 0);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraph2DAddEdge, "Modumate.Graph.2D.AddEdge", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraph2DAddEdge::RunTest(const FString& Parameters)
	{
		FGraph graph;
		int32 NextID = 1;
		TArray<FGraph2DDelta> deltas;

		TArray<FVector2D> positions = {
			FVector2D(0.0f, 0.0f),
			FVector2D(100.0f, 100.0f)
		};

		TestTrue(TEXT("Add Edge"),
			graph.AddEdge(deltas, NextID, positions[0], positions[1]));

		TestDeltas(this, deltas, graph, 0, 2, 1);

		TestTrue(TEXT("Add Duplicate Edge"),
			graph.AddEdge(deltas, NextID, positions[0], positions[1]));

		TestDeltas(this, deltas, graph, 0, 2, 1);

		return true;
	}
}
