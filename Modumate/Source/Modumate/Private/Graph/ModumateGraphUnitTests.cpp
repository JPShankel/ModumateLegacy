#include "CoreMinimal.h"
#include "ModumateGraph.h"
#include "Graph3D.h"
#include "Graph3DDelta.h"
#include "ModumateDelta.h"
#include "JsonObjectConverter.h"

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
	void ApplyDeltas(FAutomationTestBase *Test, FGraph3D &Graph, FGraph3D &TempGraph, TArray<FGraph3DDelta> &Deltas)
	{
		bool bValidDelta = true;
		for (auto& delta : Deltas)
		{
			bValidDelta = Graph.ApplyDelta(delta);
		}
		FGraph3D::CloneFromGraph(TempGraph, Graph);

		Test->TestTrue(TEXT("Apply Inverse Deltas"), bValidDelta);
	}

	void ApplyInverseDeltas(FAutomationTestBase *Test, FGraph3D &Graph, FGraph3D &TempGraph, TArray<FGraph3DDelta> &Deltas)
	{
		bool bValidDelta = true;
		for (int32 deltaIdx = Deltas.Num() - 1; deltaIdx >= 0; deltaIdx--)
		{
			// this isn't great, but it is preferable to the unit test knowing about Document or World, 
			// which are both required by FDelta::ApplyTo
			TSharedPtr<FGraph3DDelta> delta = StaticCastSharedPtr<FGraph3DDelta>(Deltas[deltaIdx].MakeInverse());
			bValidDelta = Graph.ApplyDelta(*delta);
		}
		FGraph3D::CloneFromGraph(TempGraph, Graph);

		Test->TestTrue(TEXT("Apply Deltas"), bValidDelta);
	}

	void TestGraph(FAutomationTestBase *Test, FGraph3D& Graph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		if (TestNumFaces != -1)
		{
			Test->TestEqual((TEXT("Num Faces")), Graph.GetFaces().Num(), TestNumFaces);
		}
		if (TestNumVertices != -1)
		{
			Test->TestEqual((TEXT("Num Vertices")), Graph.GetVertices().Num(), TestNumVertices);
		}
		if (TestNumEdges != -1)
		{
			Test->TestEqual((TEXT("Num Edges")), Graph.GetEdges().Num(), TestNumEdges);
		}

		Test->TestTrue((TEXT("Validate Graph")), Graph.Validate());
	}
	
	void TestKnownVertexLocations(FAutomationTestBase *Test, FGraph3D& Graph, TArray<FVector> &InKnownLocations)
	{
		for (auto& vector : InKnownLocations)
		{
			Test->TestTrue((TEXT("%s: find vertex at known location %s"), Test->GetTestName(), *vector.ToString()), Graph.FindVertex(vector) != nullptr);
		}
	}

	void TestDeltas(FAutomationTestBase *Test, TArray<FGraph3DDelta> &Deltas, FGraph3D& Graph, FGraph3D& TempGraph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		ApplyDeltas(Test, Graph, TempGraph, Deltas);
		TestGraph(Test, Graph, TestNumFaces, TestNumVertices, TestNumEdges);
		Deltas.Reset();
	}

	void TestDeltasAndResetGraph(FAutomationTestBase *Test, TArray<FGraph3DDelta> &Deltas, FGraph3D& Graph, FGraph3D& TempGraph, int32 TestNumFaces = -1, int32 TestNumVertices = -1, int32 TestNumEdges = -1)
	{
		// make sure amounts of objects match before and after the test
		int32 resetNumFaces = Graph.GetFaces().Num();
		int32 resetNumVertices = Graph.GetVertices().Num();
		int32 resetNumEdges = Graph.GetEdges().Num();

		ApplyDeltas(Test, Graph, TempGraph, Deltas);
		TestGraph(Test, Graph, TestNumFaces, TestNumVertices, TestNumEdges);
		ApplyInverseDeltas(Test, Graph, TempGraph, Deltas);
		TestGraph(Test, Graph, resetNumFaces, resetNumVertices, resetNumEdges);

		Deltas.Reset();
	}

	void ResetGraph(FAutomationTestBase *Test, TArray<FGraph3DDelta> &Deltas, FGraph3D& Graph, FGraph3D& TempGraph)
	{
		FGraph3D::CloneFromGraph(TempGraph, Graph);
		Deltas.Reset();
	}

	void SetupOneFace(FAutomationTestBase *Test, FGraph3D &Graph, FGraph3D& TempGraph, int32 &NextID)
	{
		FGraph3D tempGraph;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		TArray<FGraph3DDelta> OutDeltas;
		int32 ExistingID = 0;

		Test->TestTrue(TEXT("Add Face"),
			Graph.GetDeltaForFaceAddition(&Graph, &TempGraph, vertices, OutDeltas, NextID, ExistingID));

		ApplyDeltas(Test, Graph, TempGraph, OutDeltas);
	}

	// Add one face
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphAddFace, "Modumate.Graph.3D.AddFace", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphAddFace::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;
		int32 NextID = 1;

		SetupOneFace(this, graph, tempGraph, NextID);
		TestGraph(this, graph, 1, 4, 4);

		return true;
	}

	// Add one edge
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphAddEdge, "Modumate.Graph.3D.AddEdge", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphAddEdge::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		TArray<int32> OutEdgeIDs;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("add edge"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 2, 1);

		return true;
	}

	// Add grid of edges
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphAddEdges, "Modumate.Graph.3D.AddEdges", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphAddEdges::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		TArray<int32> OutEdgeIDs;

		TArray<FVector> vertices;

		int32 dim = 3;

		// add horizontal edges
		for (int32 idx = 0; idx <= dim; idx++)
		{
			vertices = {
				FVector(0.0f, idx * 100.0f, 0.0f),
				FVector(dim * 100.0f, idx * 100.0f, 0.0f),
			};

			TestTrue(TEXT("add horizontal edge"),
				FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
			TestDeltas(this, OutDeltas, graph, tempGraph, 0, 2*(idx+1), idx+1);
		}

		int32 numEdges = dim+1;
		int32 numVertices = 2 * (dim+1);
		// add vertical edges, which complete faces along the way
		for (int32 idx = 0; idx <= dim; idx++)
		{
			vertices = {
				FVector(idx * 100.0f, 0.0f, 0.0f),
				FVector(idx * 100.0f, dim * 100.0f, 0.0f),
			};

			if (idx == 0 || idx == dim)
			{
				// edge won't split other edges
				numEdges += dim;
				// vertices exist already
				// numVertices += 0;
			}
			else
			{
				// edge splits other edges
				numEdges += (2 * dim) +1;
				// vertices don't exist already
				numVertices += dim+1;
			}

			TestTrue(TEXT("add vertical edge"),
				FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
			TestDeltas(this, OutDeltas, graph, tempGraph, dim * idx, numVertices, numEdges);
		}

		return true;
	}

	// Add two faces that cross in the middle
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphBasicSplit, "Modumate.Graph.3D.BasicSplit", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphBasicSplit::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		FVector v1 = FVector(0.0f, 0.0f, 0.0f);
		FVector v2 = FVector(100.0f, 100.0f, 0.0f);
		FVector v3 = FVector(100.0f, 100.0f, 100.0f);
		FVector v4 = FVector(0.0f, 0.0f, 100.0f);

		FVector v5 = FVector(0.0f, 100.0f, 0.0f);
		FVector v6 = FVector(100.0f, 0.0f, 0.0f);
		FVector v7 = FVector(100.0f, 0.0f, 100.0f);
		FVector v8 = FVector(0.0f, 100.0f, 100.0f);

		FVector v9 = FVector(50.0f, 50.0f, 0.0f);
		FVector v10 = FVector(0.0f, 50.0f, 0.0f);
		FVector v11 = FVector(0.0f, 50.0f, 100.0f);
		FVector v12 = FVector(50.0f, 50.0f, 100.0f);

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TestTrue(TEXT("Add first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v1, v2, v3, v4 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		TestTrue(TEXT("Add second face that splits both faces in half"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v5, v6, v7, v8 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 10, 13);

		TestTrue(TEXT("Add third face connecting two existing edges"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v1, v5, v8, v4 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 5, 10, 15);

		TestTrue(TEXT("Add fourth face "),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v9, v10, v11, v12 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 7, 12, 20);

		return true;
	}

	// Create several parallel vertical walls, then test basic interactions with them
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphBasicMultiSplits, "Modumate.Graph.3D.BasicMultiSplits", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphBasicMultiSplits::RunTest(const FString& Parameters)
	{
		int32 numSplitFaces = 10;

		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 100.0f),
			FVector(0.0f, 0.0f, 100.0f)
		};

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		for (int32 i = 1; i <= numSplitFaces; i++)
		{
			for (auto& vertex : vertices)
			{
				vertex.Y += 100.0f;
			}

			TestTrue(TEXT("Add face for splitting"),
				FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
			TestDeltas(this, OutDeltas, graph, tempGraph, i, 4*i, 4*i);
		}

		// wall through the faces (same height)
		vertices = {
			FVector(50.0f, -100.0f, 0.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 0.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 100.0f),
			FVector(50.0f, -100.0f, 100.0f)
		};

		// numSplitFace * 2 -> at each intersection the new face and the existing face split into two faces
		int32 numFaces = (numSplitFaces + 1) + numSplitFaces * 2;

		// 4 vertices per existing rectangular face, 2 vertices per intersection
		int32 numVertices = ((numSplitFaces+1) * 4) + numSplitFaces * 2;

		// numSplitFaces * 5 -> 2 on the new face, 2 on the existing face, 1 at the intersection
		int32 numEdges = ((numSplitFaces+1) * 4) + numSplitFaces * 5;

		TestTrue(TEXT("split all faces (walls same height)"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, numFaces, numVertices, numEdges);

		// wall through the faces (new face is taller)
		vertices = {
			FVector(50.0f, -100.0f, 0.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 0.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 125.0f),
			FVector(50.0f, -100.0f, 125.0f)
		};

		// at each intersection the existing face splits into two faces
		numFaces = (numSplitFaces + 1) + numSplitFaces;

		// 4 vertices per existing rectangular face, 2 vertices per intersection
		numVertices = ((numSplitFaces+1) * 4) + numSplitFaces * 2;

		// numSplitFaces * 4 -> 1 on the new face, 2 on the existing face, 1 at the intersection
		numEdges = ((numSplitFaces+1) * 4) + numSplitFaces * 4;

		TestTrue(TEXT("split all faces (walls same height)"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, numFaces, numVertices, numEdges);

		// wall through the faces (new face is shorter)
		vertices = {
			FVector(50.0f, -100.0f, 0.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 0.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 75.0f),
			FVector(50.0f, -100.0f, 75.0f)
		};

		// at each intersection the new face splits into two faces
		numFaces = (numSplitFaces + 1) + numSplitFaces;

		// 4 vertices per existing rectangular face, 2 vertices per intersection
		numVertices = ((numSplitFaces+1) * 4) + numSplitFaces * 2;

		// numSplitFaces * 4 -> 2 on the new face, 1 on the existing face, 1 at the intersection
		numEdges = ((numSplitFaces+1) * 4) + numSplitFaces * 4;

		TestTrue(TEXT("split all faces (walls same height)"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, numFaces, numVertices, numEdges);

		// wall through the faces (no intersection splits an intersecting edge)
		vertices = {
			FVector(50.0f, -100.0f, 25.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 25.0f),
			FVector(50.0f, (numSplitFaces+1)*100.0f, 75.0f),
			FVector(50.0f, -100.0f, 75.0f)
		};

		// at each intersection the new face splits into two faces
		numFaces = (numSplitFaces + 1) + numSplitFaces;

		// 4 vertices per existing rectangular face, 2 vertices per intersection
		numVertices = ((numSplitFaces+1) * 4) + numSplitFaces * 2;

		// numSplitFaces * 3 -> 2 on the new face, 1 at the intersection
		numEdges = ((numSplitFaces+1) * 4) + numSplitFaces * 3;

		TestTrue(TEXT("split all faces (walls same height)"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, numFaces, numVertices, numEdges);

		// wall tangent to a side of the faces (same height)
		vertices = {
			FVector(0.0f, -100.0f, 0.0f),
			FVector(0.0f, (numSplitFaces+1)*100.0f, 0.0f),
			FVector(0.0f, (numSplitFaces+1)*100.0f, 100.0f),
			FVector(0.0f, -100.0f, 100.0f)
		};

		// numSplitFace * 2 -> at each intersection the new face splits into two faces
		numFaces = (numSplitFaces + 1) + numSplitFaces;

		// 4 vertices per existing rectangular face, vertices at the intersections already exist
		numVertices = ((numSplitFaces+1) * 4);

		// numSplitFaces * 2 -> 2 on the new face, edges at the intersections already exist
		numEdges = ((numSplitFaces+1) * 4) + numSplitFaces * 2;

		TestTrue(TEXT("split all faces (walls same height)"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, numFaces, numVertices, numEdges);


		return true;
	}

	// Add two faces that cross in the middle
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphComplexMultiSplits, "Modumate.Graph.3D.ComplexMultiSplits", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphComplexMultiSplits::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 100.0f),
			FVector(0.0f, 0.0f, 100.0f)
		};


		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TestTrue(TEXT("Add first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 100.0f),
			FVector(0.0f, 100.0f, 100.0f)
		};

		TestTrue(TEXT("Add second face that splits both faces in half"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 10, 13);

		vertices = {
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(0.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Horizontal plane - split into four pieces along the existing bottom edges"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 8, 10, 17);

		vertices = {
			FVector(-50.0f, 150.0f, 10.0f),
			FVector(150.0f, 150.0f, 10.0f),
			FVector(150.0f, -50.0f, 10.0f),
			FVector(-50.0f, -50.0f, 10.0f)
		};

		TestTrue(TEXT("Horizontal plane - splits vertical planes into two"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 13, 19, 30);

		vertices = {
			FVector(-25.0f, 25.0f, 20.0f),
			FVector(25.0f, 25.0f, 20.0f),
			FVector(25.0f, -25.0f, 20.0f),
			FVector(-25.0f, -25.0f, 20.0f)
		};

		TestTrue(TEXT("Horizontal plane - intersection exists but doesn't split any faces"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 14, 24, 36);

		return true;
	}

	// Draw three planes, a floor, and two vertical walls making a t-shape
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphFloorSplit, "Modumate.Graph.3D.FloorSplit", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphFloorSplit::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		FVector v1 = FVector(0.0f, 0.0f, 0.0f);
		FVector v2 = FVector(0.0f, 100.0f, 0.0f);
		FVector v3 = FVector(100.0f, 100.0f, 0.0f);
		FVector v4 = FVector(100.0f, 0.0f, 0.0f);

		FVector v5 = FVector(0.0f, 50.0f, 0.0f);
		FVector v6 = FVector(100.0f, 50.0f, 0.0f);
		FVector v7 = FVector(100.0f, 50.0f, 100.0f);
		FVector v8 = FVector(0.0f, 50.0f, 100.0f);

		FVector v9 = FVector(50.0f, 0.0f, 0.0f);
		FVector v10 = FVector(50.0f, 50.0f, 0.0f);
		FVector v11 = FVector(50.0f, 50.0f, 100.0f);
		FVector v12 = FVector(50.0f, 0.0f, 100.0f);

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TestTrue(TEXT("Add first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v1, v2, v3, v4 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		TestTrue(TEXT("Add second face, splitting first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v5, v6, v7, v8 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		TestTrue(TEXT("Add third face, splitting first face and second face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, { v9, v10, v11, v12 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 6, 12, 17);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphSimpleMove, "Modumate.Graph.3D.SimpleMove", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphSimpleMove::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		int32 NextID = 1;
		int32 ExistingID = 0;

		SetupOneFace(this, graph, tempGraph, NextID);
		TestGraph(this, graph, 1, 4, 4);
		
		FVector vMove = FVector(0.0f, 50.0f, 0.0f);

		TArray<int32> vertexIDs;
		TArray<FVector> newPositions;
		for (auto& vector : vertices)
		{
			auto vertex = graph.FindVertex(vector);
			if (vertex == nullptr)
			{
				return false;
			}
			vertexIDs.Add(vertex->ID);
			newPositions.Add(vertex->Position + vMove);
		}

		TArray<FGraph3DDelta> OutDeltas;
		TestTrue(TEXT("move face"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, vertexIDs, newPositions, OutDeltas, NextID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		// verify vertex positions
		TestKnownVertexLocations(this, graph, newPositions);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphSimpleVertexJoin, "Modumate.Graph.3D.SimpleVertexJoin", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphSimpleVertexJoin::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TArray<FVector> firstVertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		SetupOneFace(this, graph, tempGraph, NextID);
		TestGraph(this, graph, 1, 4, 4);

		TArray<FVector> secondVertices = {
			FVector(200.0f, 200.0f, 0.0f),
			FVector(200.0f, 300.0f, 0.0f),
			FVector(300.0f, 300.0f, 0.0f),
			FVector(300.0f, 200.0f, 0.0f)
		};

		TestTrue(TEXT("add second face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		TestKnownVertexLocations(this, graph, firstVertices);
		TestKnownVertexLocations(this, graph, secondVertices);

		auto vertex = graph.FindVertex(FVector(200.0f, 200.0f, 0.0f));

		TestTrue(TEXT("join vertex"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, { vertex->ID }, { FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		// verify against correct set of positions	
		TArray<FVector> testVertices = firstVertices;
		testVertices.Append({ 
			FVector(200.0f, 300.0f, 0.0f),
			FVector(300.0f, 300.0f, 0.0f),
			FVector(300.0f, 200.0f, 0.0f) });

		TestKnownVertexLocations(this, graph, testVertices);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphSimpleEdgeJoin, "Modumate.Graph.3D.SimpleEdgeJoin", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphSimpleEdgeJoin::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		SetupOneFace(this, graph, tempGraph, NextID);
		TestGraph(this, graph, 1, 4, 4);

		TArray<FVector> secondVertices = {
			FVector(200.0f, 200.0f, 100.0f),
			FVector(200.0f, 300.0f, 100.0f),
			FVector(300.0f, 300.0f, 100.0f),
			FVector(300.0f, 200.0f, 100.0f)
		};

		TestTrue(TEXT("add second face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		TestKnownVertexLocations(this, graph, secondVertices);
		auto vertex = graph.FindVertex(FVector(200.0f, 200.0f, 100.0f));
		auto secondVertex = graph.FindVertex(FVector(300.0f, 200.0f, 100.0f));

		TestTrue(TEXT("join edges"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, { vertex->ID, secondVertex->ID }, { FVector(0.0f, 100.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphComplexEdgeJoins, "Modumate.Graph.3D.ComplexEdgeJoins", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphComplexEdgeJoins::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		// default horizontal first face
		SetupOneFace(this, graph, tempGraph, NextID);

		// separate second face that is raised to be on a different plane
		// one of its edges is used for all of the vertex movements
		TArray<FVector> secondVertices = {
			FVector(200.0f, 0.0f, 100.0f),
			FVector(200.0f, 100.0f, 100.0f),
			FVector(300.0f, 100.0f, 100.0f),
			FVector(300.0f, 0.0f, 100.0f)
		};

		TestTrue(TEXT("add second face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		TestKnownVertexLocations(this, graph, secondVertices);
		auto vertex = graph.FindVertex(FVector(200.0f, 0.0f, 100.0f));
		auto secondVertex = graph.FindVertex(FVector(200.0f, 100.0f, 100.0f));

		TArray<int32> moveVertexIDs = { vertex->ID, secondVertex->ID };
		
		TestTrue(TEXT("Non-planar join edge and vertices."),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		TestTrue(TEXT("Split edge twice."), 
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 25.0f, 0.0f), FVector(100.0f, 75.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		TestTrue(TEXT("Join one vertex and split edge."),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 25.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		TestTrue(TEXT("Moved edge gets split twice."), 
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, -25.0f, 0.0f), FVector(100.0f, 125.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		TestTrue(TEXT("Join one vertex and split moved edge."),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, -25.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		TestTrue(TEXT("Split moved edge and original edge."),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, -25.0f, 0.0f), FVector(100.0f, 25.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		// horizontal face separate from the first face
		TArray<FVector> thirdVertices = {
			FVector(0.0f, 125.0f, 0.0f),
			FVector(0.0f, 225.0f, 0.0f),
			FVector(100.0f, 225.0f, 0.0f),
			FVector(100.0f, 125.0f, 0.0f)
		};

		TestTrue(TEXT("add third face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, thirdVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 12, 12);

		// vertical face on the edge of the third face that is involved in the joins/splits
		TArray<FVector> fourthVertices = {
			FVector(100.0f, 125.0f, 0.0f),
			FVector(100.0f, 225.0f, 0.0f),
			FVector(100.0f, 225.0f, 100.0f),
			FVector(100.0f, 125.0f, 100.0f)
		};

		TestTrue(TEXT("add fourth face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, fourthVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 14, 15);

		TestTrue(TEXT("join with two edges, make edge connecting the faces"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 225.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 12, 15);
		
		TestTrue(TEXT("join with one edge"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 12, 14);

		TestTrue(TEXT("join with two vertices, zero edges"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 125.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 12, 15);

		TestTrue(TEXT("split edge on one face, join vertex on other face"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 200.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 13, 16);

		TestTrue(TEXT("split edge on two faces"),
			FGraph3D::GetDeltaForVertexMovements(&graph, &tempGraph, moveVertexIDs, { FVector(100.0f, 75.0f, 0.0f), FVector(100.0f, 200.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 14, 17);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphTraversePlanes, "Modumate.Graph.3D.TraversePlanes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphTraversePlanes::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		FGraph3DDelta OutDelta;
		int32 NextID = 1;
		int32 ExistingID = 0;

		FGraph3D::GetDeltaForVertexAddition(&tempGraph, FVector(0.0f, 0.0f, 0.0f), OutDelta, NextID, ExistingID);
		FGraph3D::GetDeltaForVertexAddition(&tempGraph, FVector(0.0f, 100.0f, 0.0f), OutDelta, NextID, ExistingID);
		FGraph3D::GetDeltaForVertexAddition(&tempGraph, FVector(0.0f, 100.0f, 100.0f), OutDelta, NextID, ExistingID);
		graph.ApplyDelta(OutDelta);

		TestGraph(this, graph, 0, 3, 0);
		OutDelta.Reset();

		FGraph3D::GetDeltaForEdgeAddition(&tempGraph, FVertexPair(1, 2), OutDelta, NextID, ExistingID);
		FGraph3D::GetDeltaForEdgeAddition(&tempGraph, FVertexPair(2, 3), OutDelta, NextID, ExistingID);
		graph.ApplyDelta(OutDelta);

		TestGraph(this, graph, 0, 3, 2);
		OutDelta.Reset();

		FGraph3D::GetDeltaForEdgeAddition(&tempGraph, FVertexPair(1, 3), OutDelta, NextID, ExistingID);
		graph.ApplyDelta(OutDelta);
		TestGraph(this, graph, 0, 3, 3);
		OutDelta.Reset();

		bool bForward;
		auto edge = graph.FindEdgeByVertices(1, 3, bForward);
		TArray<TArray<int32>> OutVertexIDs;
		graph.TraverseFacesFromEdge(edge->ID, OutVertexIDs);

		TestTrue(TEXT("Found face"), OutVertexIDs.Num() > 0);

		TArray<int32> InParentIDs;
		TMap<int32, int32> edgeMap;
		int32 AddedFaceID;
		FGraph3D::CloneFromGraph(tempGraph, graph);
		FGraph3D::GetDeltaForFaceAddition(&tempGraph, OutVertexIDs[0], OutDelta, NextID, ExistingID, InParentIDs, edgeMap, AddedFaceID);
		OutDeltas = { OutDelta };

		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 3, 3);

		// TODO: enhance the usage of CheckFaceNormals to automatically check if the face is a duplicate in GetDeltaForFaceAddition,
		// and then add to this test


		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphFaceSplitByEdge, "Modumate.Graph.3D.FaceSplitByEdge", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphFaceSplitByEdge::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		TArray<int32> OutEdgeIDs;

		// default horizontal first face
		SetupOneFace(this, graph, tempGraph, NextID);

		TArray<FVector> vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f)
		};

		TestTrue(TEXT("split face with edge"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphFaceSubdivision, "Modumate.Graph.3D.FaceSubdivision", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphFaceSubdivision::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		FGraph3DDelta OutDelta;
		int32 NextID = 1;
		int32 ExistingID = 0;
		TArray<int32> OutEdgeIDs;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		// draw first face with edges
		TestTrue(TEXT("edge 1"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 2, 1);

		TestTrue(TEXT("edge 2"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[1], vertices[2], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 3, 2);

		TestTrue(TEXT("edge 3"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[2], vertices[3], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 4, 3);

		TestTrue(TEXT("edge 4"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, vertices[3], vertices[0], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		// single edge across face
		TArray<FVector> edgeVertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f)
		};
		TestTrue(TEXT("split edge"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, edgeVertices[0], edgeVertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		// draw box in corner
		edgeVertices = {
			FVector(90.0f, 0.0f, 0.0f),
			FVector(90.0f, 10.0f, 0.0f),
			FVector(100.0f, 10.0f, 0.0f)
		};
		TestTrue(TEXT("box edge 1"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, edgeVertices[0], edgeVertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 9);
		TestTrue(TEXT("box edge 2"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, edgeVertices[1], edgeVertices[2], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 9, 11);

		// draw a few edges across face, and verify no extra faces are found
		edgeVertices = {
			FVector(25.0f, 0.0f, 0.0f),
			FVector(25.0f, 10.0f, 0.0f),
			FVector(25.0f, 20.0f, 0.0f),
			FVector(25.0f, 100.0f, 0.0f),
		};

		TestTrue(TEXT("edge 1"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, edgeVertices[0], edgeVertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 11, 13);

		TestTrue(TEXT("edge 2"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, edgeVertices[1], edgeVertices[2], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 12, 14);

		TestTrue(TEXT("edge 3"),
			FGraph3D::GetDeltaForEdgeAdditionWithSplit(&graph, &tempGraph, edgeVertices[2], edgeVertices[3], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 13, 16);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphOverlappingFaces, "Modumate.Graph.3D.OverlappingFaces", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphOverlappingFaces::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		FGraph3DDelta OutDelta;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 10.0f, 0.0f),
			FVector(10.0f, 10.0f, 0.0f),
			FVector(10.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add overlapping face, making box in corner"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(50.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add overlapping face, covering half the existing face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		// TODO: fix ensureAlways when face matches exactly and add unit test

		vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(150.0f, 100.0f, 0.0f),
			FVector(150.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add overlapping face that should create three even faces"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		vertices = {
			FVector(-50.0f, 0.0f, 0.0f),
			FVector(-50.0f, 100.0f, 0.0f),
			FVector(150.0f, 100.0f, 0.0f),
			FVector(150.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add completely overlapping face that should create three even faces"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		vertices = {
			FVector(-50.0f, -50.0f, 0.0f),
			FVector(-50.0f, 150.0f, 0.0f),
			FVector(150.0f, 150.0f, 0.0f),
			FVector(150.0f, -50.0f, 0.0f)
		};

		TestTrue(TEXT("Add completely overlapping face that does not intersect with existing face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		vertices = {
			FVector(50.0f, 50.0f, 0.0f),
			FVector(50.0f, 150.0f, 0.0f),
			FVector(150.0f, 150.0f, 0.0f),
			FVector(150.0f, 50.0f, 0.0f)
		};

		TestTrue(TEXT("Add first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 3, 10, 12);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphDeletion, "Modumate.Graph.3D.Deletion", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphDeletion::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		// default horizontal first face
		SetupOneFace(this, graph, tempGraph, NextID);
		int32 firstFaceID = MOD_ID_NONE;
		for (auto &kvp : graph.GetFaces())
		{
			firstFaceID = kvp.Key;
			break;
		}

		TestTrue(TEXT("First face is valid"), firstFaceID != MOD_ID_NONE);

		// a second face that is connected to one of the edges of the first face
		TArray<FVector> secondVertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 100.0f),
			FVector(0.0f, 0.0f, 100.0f)
		};

		TestTrue(TEXT("Add second face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		OutDeltas.AddDefaulted();
		TestTrue(TEXT("Delete face, including connected edges and vertices"),
			FGraph3D::GetDeltaForDeleteObjects(&tempGraph, {}, {}, { firstFaceID }, OutDeltas[0], true));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		OutDeltas.AddDefaulted();
		TestTrue(TEXT("Delete face, excluding connected edges and vertices"),
			FGraph3D::GetDeltaForDeleteObjects(&tempGraph, {}, {}, { firstFaceID }, OutDeltas[0], false));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 1, 6, 7);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphEdgeJoinTool, "Modumate.Graph.3D.EdgeJoinTool", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphEdgeJoinTool::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(50.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 100.0f),
			FVector(50.0f, 0.0f, 100.0f),
			FVector(0.0f, 0.0f, 100.0f)
		};

		// test edge combinations
		TArray<FGraph3DDelta> deltas;
		deltas.AddDefaulted();
		for (auto vertex : vertices)
		{
			TestTrue(TEXT("Add vertex"),
				FGraph3D::GetDeltaForVertexAddition(&tempGraph, vertex, deltas[0], NextID, ExistingID));
		}
		TestDeltas(this, deltas, graph, tempGraph, 0, 6, 0);

		for (int i = 0; i < 4; i++)
		{
			TPair<int32, int32> edge1 = (i % 2 == 0) ? (TPair<int32, int32>(1, 2)) : (TPair<int32, int32>(2, 1));
			TPair<int32, int32> edge2 = (i / 2 == 0) ? (TPair<int32, int32>(2, 3)) : (TPair<int32, int32>(3, 2));

			OutDeltas.AddDefaulted(2);
			TestTrue(TEXT("Add edge"),
				FGraph3D::GetDeltaForEdgeAddition(&tempGraph, edge1, OutDeltas[0], NextID, ExistingID));
			TestTrue(TEXT("Add edge"),
				FGraph3D::GetDeltaForEdgeAddition(&tempGraph, edge2, OutDeltas[0], NextID, ExistingID));
			ApplyDeltas(this, graph, tempGraph, OutDeltas);
			TestTrue(TEXT("Join edge"),
				FGraph3D::GetDeltaForEdgeJoin(&tempGraph, OutDeltas[1], NextID, TPair<int32, int32>(NextID - 1, NextID - 2)));
			TArray<FGraph3DDelta> partial = { OutDeltas[0] };
			ApplyInverseDeltas(this, graph, tempGraph, partial);
			TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 0, 5, 1);
		}

		TestTrue(TEXT("Add face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 6, 6);

		// only one face
		for (auto kvp : graph.GetFaces())
		{
			auto face = kvp.Value;

			for (int32 edgeIdx = 0; edgeIdx < face.EdgeIDs.Num(); edgeIdx++)
			{
				OutDeltas.AddDefaulted();
				TestTrue(TEXT("Join edge"),
					FGraph3D::GetDeltaForEdgeJoin(&tempGraph, OutDeltas[0], NextID, TPair<int32, int32>(face.EdgeIDs[edgeIdx], face.EdgeIDs[(edgeIdx + 1) % face.EdgeIDs.Num()])));
				TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 1, 5, 5);
			}
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphBasicFaceJoinTool, "Modumate.Graph.3D.BasicFaceJoinTool", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphBasicFaceJoinTool::RunTest(const FString& Parameters)
	{
		// original face setup
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(50.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add first face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add second face"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);
		
		TArray<int32> faceIDs;
		graph.GetFaces().GenerateKeyArray(faceIDs);

		TestTrue(TEXT("join faces"),
			FGraph3D::GetDeltasForObjectJoin(&tempGraph, OutDeltas, faceIDs, NextID, EGraph3DObjectType::Face));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(50.0f, 100.0f, 100.0f),
			FVector(50.0f, 0.0f, 100.0f)
		};

		TestTrue(TEXT("Add third face that prevents the original join"),
			FGraph3D::GetDeltaForFaceAddition(&graph, &tempGraph, vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		TestTrue(TEXT("fail to join faces when there is another face connected to the edge"),
			!FGraph3D::GetDeltasForObjectJoin(&tempGraph, OutDeltas, faceIDs, NextID, EGraph3DObjectType::Face));
		ResetGraph(this, OutDeltas, graph, tempGraph);

		return true;
	}

}
