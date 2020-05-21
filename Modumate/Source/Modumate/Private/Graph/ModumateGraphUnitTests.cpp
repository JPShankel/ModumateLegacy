#include "CoreMinimal.h"

#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "JsonObjectConverter.h"
#include "DocumentManagement/ModumateDelta.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Graph/ModumateGraph.h"
#include "DocumentManagement/ModumateSerialization.h"

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
			bValidDelta = Graph.ApplyDelta(*Deltas[deltaIdx].MakeGraphInverse());
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
		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		TArray<FGraph3DDelta> OutDeltas;
		int32 ExistingID = 0;

		Test->TestTrue(TEXT("Add Face"),
			Graph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));

		ApplyDeltas(Test, Graph, TempGraph, OutDeltas);
	}

	bool LoadGraph(const FString &path, FGraph3D &OutGraph, int32 &NextID)
	{
		FString scenePathname = FPaths::ProjectDir() / UModumateGameInstance::TestScriptRelativePath / path;
		FModumateDocumentHeader docHeader;
		FMOIDocumentRecord docRec;

		// TODO: potentially more of this document record could be used as well for some testing of the document,
		// even with a null rhi
		if (!FModumateSerializationStatics::TryReadModumateDocumentRecord(scenePathname, docHeader, docRec))
		{
			return false;
		}

		OutGraph.Load(&docRec.VolumeGraph);

		// find max ID of an object in the graph, and set NextID to be greater than that
		for (auto& kvp : OutGraph.GetFaces())
		{
			NextID = FMath::Max(NextID, kvp.Key);
		}

		for (auto& kvp : OutGraph.GetEdges())
		{
			NextID = FMath::Max(NextID, kvp.Key);
		}

		for (auto& kvp : OutGraph.GetVertices())
		{
			NextID = FMath::Max(NextID, kvp.Key);
		}
		NextID++;

		return true;
	}

	// Load simple file with one face
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphLoad, "Modumate.Graph.3D.Load", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphLoad::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		int32 NextID = 1;
	
		if (!LoadGraph(TEXT("Graph/load-test.mdmt"), graph, NextID))
		{
			return false;
		}

		TestGraph(this, graph, 1, 4, 4);

		return true;
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

	// Add three constrained faces
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphAddConstrainedFaces, "Modumate.Graph.3D.AddConstrainedFaces", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphAddConstrainedFaces::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;
		int32 NextID = 1;
		int32 ExistingID = 0;

		TArray<FGraph3DDelta> OutDeltas;

		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(0.0f, 0.0f, 150.0f),
			FVector(100.0f, 0.0f, 100.0f),
			FVector(0.0f, 100.0f, 10.0f)
		};

		TestTrue(TEXT("Add first face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[0], vertices[1], vertices[4], vertices[3] }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		TestTrue(TEXT("Add second face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[1], vertices[2], vertices[5], vertices[4] }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		TestTrue(TEXT("Add third face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[2], vertices[0], vertices[3], vertices[5] }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 6, 9);

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
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 2, 1);

		return true;
	}

	// Add one vertex, test adding duplicate vertex
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphAddVertex, "Modumate.Graph.3D.AddVertex", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphAddVertex::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		FGraph3DDelta OutDelta;
		int32 NextID = 1;
		int32 ExistingID = MOD_ID_NONE;

		FVector position = FVector(0.0f, 0.0f, 0.0f);

		OutDeltas.SetNumZeroed(1);
		TestTrue(TEXT("add vertex"),
			tempGraph.GetDeltaForVertexAddition(position, OutDeltas[0], NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 1, 0);

		OutDelta.Reset();
		TestTrue(TEXT("attempt to add duplicate vertex"),
			!tempGraph.GetDeltaForVertexAddition(position, OutDelta, NextID, ExistingID));

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
				tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
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
				tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
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
			tempGraph.GetDeltaForFaceAddition({ v1, v2, v3, v4 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		TestTrue(TEXT("Add second face that splits both faces in half"),
			tempGraph.GetDeltaForFaceAddition({ v5, v6, v7, v8 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 10, 13);

		TestTrue(TEXT("Add third face connecting two existing edges"),
			tempGraph.GetDeltaForFaceAddition({ v1, v5, v8, v4 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 5, 10, 15);

		TestTrue(TEXT("Add fourth face "),
			tempGraph.GetDeltaForFaceAddition({ v9, v10, v11, v12 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 7, 12, 20);

		return true;
	}

	// Create several parallel vertical walls, then test basic interactions with them
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphBasicMultiSplits, "Modumate.Graph.3D.BasicMultiSplits", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphBasicMultiSplits::RunTest(const FString& Parameters)
	{
		// if this number gets much larger, this test takes long enough that it triggers warnings in other tests
		// TODO: can this be run as a stress test elsewhere?
		int32 numSplitFaces = 5;

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
				tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 100.0f),
			FVector(0.0f, 100.0f, 100.0f)
		};

		TestTrue(TEXT("Add second face that splits both faces in half"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 10, 13);

		vertices = {
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(0.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Horizontal plane - split into four pieces along the existing bottom edges"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 8, 10, 17);

		vertices = {
			FVector(-50.0f, 150.0f, 10.0f),
			FVector(150.0f, 150.0f, 10.0f),
			FVector(150.0f, -50.0f, 10.0f),
			FVector(-50.0f, -50.0f, 10.0f)
		};

		TestTrue(TEXT("Horizontal plane - splits vertical planes into two"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 13, 19, 30);

		vertices = {
			FVector(-25.0f, 25.0f, 20.0f),
			FVector(25.0f, 25.0f, 20.0f),
			FVector(25.0f, -25.0f, 20.0f),
			FVector(-25.0f, -25.0f, 20.0f)
		};

		TestTrue(TEXT("Horizontal plane - intersection exists but doesn't split any faces"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition({ v1, v2, v3, v4 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		TestTrue(TEXT("Add second face, splitting first face"),
			tempGraph.GetDeltaForFaceAddition({ v5, v6, v7, v8 }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		TestTrue(TEXT("Add third face, splitting first face and second face"),
			tempGraph.GetDeltaForFaceAddition({ v9, v10, v11, v12 }, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForVertexMovements(vertexIDs, newPositions, OutDeltas, NextID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		// verify vertex positions
		TestKnownVertexLocations(this, graph, newPositions);

		// test that non-planar move fails
		OutDeltas.Reset();
		TestTrue(TEXT("fail to move face"),
			!tempGraph.GetDeltaForVertexMovements({ vertexIDs[0] }, { FVector(0.0f, 0.0f, 100.0f) }, OutDeltas, NextID));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphMoveWithDeltaReset, "Modumate.Graph.3D.MoveWithDeltaReset", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphMoveWithDeltaReset::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		int32 expectedFaces = 6;
		int32 expectedVertices = 19;
		int32 expectedEdges = 24;

		if (!LoadGraph(TEXT("Graph/move_delta_bug.mdmt"), graph, NextID))
		{
			return false;
		}
		TestGraph(this, graph, expectedFaces, expectedVertices, expectedEdges);
		FGraph3D::CloneFromGraph(tempGraph, graph);

		// setup move that reproduced bug
		auto movedVertex1 = graph.FindVertex(29);
		auto movedVertex2 = graph.FindVertex(27);
		auto staticVertex1 = graph.FindVertex(31);
		auto staticVertex2 = graph.FindVertex(12);

		TArray<int32> vertexIDs = { 29, 27, 31, 12 };
		FVector offset = FVector(0.0f, 0.0f, 100.0f);
		TArray<FVector> moves = {
			movedVertex1->Position + offset,
			movedVertex2->Position + offset,
			staticVertex1->Position,
			staticVertex2->Position
		};

		TestTrue(TEXT("move bottom edge"),
			tempGraph.GetDeltaForVertexMovements(vertexIDs, moves, OutDeltas, NextID));
		TestDeltas(this, OutDeltas, graph, tempGraph, expectedFaces, expectedVertices, expectedEdges);

		TestKnownVertexLocations(this, graph, moves);

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
			tempGraph.GetDeltaForFaceAddition(secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		TestKnownVertexLocations(this, graph, firstVertices);
		TestKnownVertexLocations(this, graph, secondVertices);

		auto vertex = graph.FindVertex(FVector(200.0f, 200.0f, 0.0f));

		TestTrue(TEXT("join vertex"),
			tempGraph.GetDeltaForVertexMovements({ vertex->ID }, { FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
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
			tempGraph.GetDeltaForFaceAddition(secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		TestKnownVertexLocations(this, graph, secondVertices);
		auto vertex = graph.FindVertex(FVector(200.0f, 200.0f, 100.0f));
		auto secondVertex = graph.FindVertex(FVector(300.0f, 200.0f, 100.0f));

		TestTrue(TEXT("join edges"),
			tempGraph.GetDeltaForVertexMovements({ vertex->ID, secondVertex->ID }, { FVector(0.0f, 100.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
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
			tempGraph.GetDeltaForFaceAddition(secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		TestKnownVertexLocations(this, graph, secondVertices);
		auto vertex = graph.FindVertex(FVector(200.0f, 0.0f, 100.0f));
		auto secondVertex = graph.FindVertex(FVector(200.0f, 100.0f, 100.0f));

		TArray<int32> moveVertexIDs = { vertex->ID, secondVertex->ID };
		
		TestTrue(TEXT("Non-planar join edge and vertices."),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		TestTrue(TEXT("Split edge twice."), 
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 25.0f, 0.0f), FVector(100.0f, 75.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		TestTrue(TEXT("Join one vertex and split edge."),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 25.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		TestTrue(TEXT("Moved edge gets split twice."), 
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, -25.0f, 0.0f), FVector(100.0f, 125.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		TestTrue(TEXT("Join one vertex and split moved edge."),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, -25.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		TestTrue(TEXT("Split moved edge and original edge."),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, -25.0f, 0.0f), FVector(100.0f, 25.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		// horizontal face separate from the first face
		TArray<FVector> thirdVertices = {
			FVector(0.0f, 125.0f, 0.0f),
			FVector(0.0f, 225.0f, 0.0f),
			FVector(100.0f, 225.0f, 0.0f),
			FVector(100.0f, 125.0f, 0.0f)
		};

		TestTrue(TEXT("add third face"),
			tempGraph.GetDeltaForFaceAddition(thirdVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 12, 12);

		// vertical face on the edge of the third face that is involved in the joins/splits
		TArray<FVector> fourthVertices = {
			FVector(100.0f, 125.0f, 0.0f),
			FVector(100.0f, 225.0f, 0.0f),
			FVector(100.0f, 225.0f, 100.0f),
			FVector(100.0f, 125.0f, 100.0f)
		};

		TestTrue(TEXT("add fourth face"),
			tempGraph.GetDeltaForFaceAddition(fourthVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 14, 15);

		TestTrue(TEXT("join with two edges, make edge connecting the faces"),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 225.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 12, 15);
		
		TestTrue(TEXT("join with one edge"),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 12, 14);

		TestTrue(TEXT("join with two vertices, zero edges"),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 125.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 12, 15);

		TestTrue(TEXT("split edge on one face, join vertex on other face"),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 200.0f, 0.0f) }, OutDeltas, NextID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 4, 13, 16);

		TestTrue(TEXT("split edge on two faces"),
			tempGraph.GetDeltaForVertexMovements(moveVertexIDs, { FVector(100.0f, 75.0f, 0.0f), FVector(100.0f, 200.0f, 0.0f) }, OutDeltas, NextID));
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

		tempGraph.GetDeltaForVertexAddition(FVector(0.0f, 0.0f, 0.0f), OutDelta, NextID, ExistingID);
		tempGraph.GetDeltaForVertexAddition(FVector(0.0f, 100.0f, 0.0f), OutDelta, NextID, ExistingID);
		tempGraph.GetDeltaForVertexAddition(FVector(0.0f, 100.0f, 100.0f), OutDelta, NextID, ExistingID);
		graph.ApplyDelta(OutDelta);

		TestGraph(this, graph, 0, 3, 0);
		OutDelta.Reset();

		tempGraph.GetDeltaForEdgeAddition(FVertexPair(1, 2), OutDelta, NextID, ExistingID);
		tempGraph.GetDeltaForEdgeAddition(FVertexPair(2, 3), OutDelta, NextID, ExistingID);
		graph.ApplyDelta(OutDelta);

		TestGraph(this, graph, 0, 3, 2);
		OutDelta.Reset();

		tempGraph.GetDeltaForEdgeAddition(FVertexPair(1, 3), OutDelta, NextID, ExistingID);
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
		tempGraph.GetDeltaForFaceAddition(OutVertexIDs[0], OutDelta, NextID, ExistingID, InParentIDs, edgeMap, AddedFaceID);
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
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
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
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[0], vertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 2, 1);

		TestTrue(TEXT("edge 2"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[1], vertices[2], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 3, 2);

		TestTrue(TEXT("edge 3"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[2], vertices[3], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 0, 4, 3);

		TestTrue(TEXT("edge 4"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[3], vertices[0], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		// single edge across face
		TArray<FVector> edgeVertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f)
		};
		TestTrue(TEXT("split edge"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(edgeVertices[0], edgeVertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		// draw box in corner
		edgeVertices = {
			FVector(90.0f, 0.0f, 0.0f),
			FVector(90.0f, 10.0f, 0.0f),
			FVector(100.0f, 10.0f, 0.0f)
		};
		TestTrue(TEXT("box edge 1"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(edgeVertices[0], edgeVertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 9);
		TestTrue(TEXT("box edge 2"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(edgeVertices[1], edgeVertices[2], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 9, 11);

		// draw a few edges across face, and verify no extra faces are found
		edgeVertices = {
			FVector(25.0f, 0.0f, 0.0f),
			FVector(25.0f, 10.0f, 0.0f),
			FVector(25.0f, 20.0f, 0.0f),
			FVector(25.0f, 100.0f, 0.0f),
		};

		TestTrue(TEXT("edge 1"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(edgeVertices[0], edgeVertices[1], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 11, 13);

		TestTrue(TEXT("edge 2"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(edgeVertices[1], edgeVertices[2], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 12, 14);

		TestTrue(TEXT("edge 3"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(edgeVertices[2], edgeVertices[3], OutDeltas, NextID, OutEdgeIDs, true));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 13, 16);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphFaceSubdivisionWithFaces, "Modumate.Graph.3D.FaceSubdivisionWithFaces", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphFaceSubdivisionWithFaces::RunTest(const FString& Parameters)
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
			FVector(0.0f, 50.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 50.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		FVector offset = FVector(0.0f, 0.0f, 100.0f);

		TestTrue(TEXT("Add first face"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 6, 6);

		// add some vertical faces around the perimeter
		TestTrue(TEXT("Add vertical face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[0], vertices[1], vertices[1] + offset, vertices[0] + offset }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 8, 9);

		TestTrue(TEXT("Add vertical face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[1], vertices[2], vertices[2] + offset, vertices[1] + offset }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 9, 11);

		TestTrue(TEXT("Add vertical face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[3], vertices[4], vertices[4] + offset, vertices[3] + offset }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 4, 11, 14);

		TestTrue(TEXT("Add vertical face"),
			tempGraph.GetDeltaForFaceAddition({ vertices[4], vertices[5], vertices[5] + offset, vertices[4] + offset }, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 5, 12, 16);
		
		// add vertical faces through the center
		FVector middlePoint = FVector(50.0f, 60.0f, 0.0f);
		TArray<FVector> middleVertices = { vertices[1], middlePoint, middlePoint + offset, vertices[1] + offset };

		TestTrue(TEXT("Add middle face"),
			tempGraph.GetDeltaForFaceAddition(middleVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 6, 14, 19);

		// second vertical face should split first face
		middleVertices = { vertices[4], middlePoint, middlePoint + offset, vertices[4] + offset };
		TestTrue(TEXT("Add middle face"),
			tempGraph.GetDeltaForFaceAddition(middleVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 8, 14, 21);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphInteriorWallSubdivision, "Modumate.Graph.3D.InteriorWallSubdivision", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphInteriorWallSubdivision::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;

		int32 expectedFaces = 15;
		int32 expectedVertices = 31;
		int32 expectedEdges = 48;

		// this file has a house with the exterior wall metaplanes and the floor metaplane created
		// when drawing two walls across the floor connecting two corners of the exterior, make sure that the floor splits
		if (!LoadGraph(TEXT("Graph/bug - interior walls not subdividing floor.mdmt"), graph, NextID))
		{
			return false;
		}

		TestGraph(this, graph, expectedFaces, expectedVertices, expectedEdges);
		FGraph3D::CloneFromGraph(tempGraph, graph);

		auto vertex1 = graph.FindVertex(14);
		auto vertex2 = graph.FindVertex(91);
		auto vertex3 = graph.FindVertex(15);

		TestTrue(TEXT("vertex 1"), vertex1 != nullptr);
		TestTrue(TEXT("vertex 2"), vertex2 != nullptr);
		TestTrue(TEXT("vertex 3"), vertex3 != nullptr);

		if (vertex1 == nullptr || vertex2 == nullptr || vertex3 == nullptr)
		{
			return false;
		}

		// new faces connect two exterior corners in a rectangular way
		// corner1 is the bottom right, corner2 is the top left, new corners are added at the top right
		FVector corner1 = vertex1->Position;
		FVector corner2 = vertex2->Position;
		float offsetZ = vertex3->Position.Z;
		FVector cornert1 = FVector(corner1.X, corner1.Y, corner1.Z + offsetZ);
		FVector cornert2 = FVector(corner2.X, corner2.Y, corner2.Z + offsetZ);
		FVector newCorner = FVector(corner1.X, corner2.Y, corner1.Z);
		FVector newCornerTop = FVector(corner1.X, corner2.Y, cornert1.Z);

		TestTrue(TEXT("face one"),
			tempGraph.GetDeltaForFaceAddition({ corner1, cornert1, newCornerTop, newCorner },OutDeltas, NextID, ExistingID));

		expectedFaces += 1;
		expectedVertices += 2;
		expectedEdges += 3;
		TestDeltas(this, OutDeltas, graph, tempGraph, expectedFaces, expectedVertices, expectedEdges);

		//*
		TestTrue(TEXT("face two, which should split the floor"),
			tempGraph.GetDeltaForFaceAddition({ corner2, newCorner, newCornerTop, cornert2 },OutDeltas, NextID, ExistingID));

		expectedFaces += 2;
		expectedVertices += 0;
		expectedEdges += 2;
		TestDeltas(this, OutDeltas, graph, tempGraph, expectedFaces, expectedVertices, expectedEdges);
		//*/

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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 10.0f, 0.0f),
			FVector(10.0f, 10.0f, 0.0f),
			FVector(10.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add overlapping face, making box in corner"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 7, 8);

		vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(50.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add overlapping face, covering half the existing face"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		// TODO: fix ensureAlways when face matches exactly and add unit test

		vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(150.0f, 100.0f, 0.0f),
			FVector(150.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add overlapping face that should create three even faces"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		vertices = {
			FVector(-50.0f, 0.0f, 0.0f),
			FVector(-50.0f, 100.0f, 0.0f),
			FVector(150.0f, 100.0f, 0.0f),
			FVector(150.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add completely overlapping face that should create three even faces"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		vertices = {
			FVector(-50.0f, -50.0f, 0.0f),
			FVector(-50.0f, 150.0f, 0.0f),
			FVector(150.0f, 150.0f, 0.0f),
			FVector(150.0f, -50.0f, 0.0f)
		};

		TestTrue(TEXT("Add completely overlapping face that does not intersect with existing face"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 2, 8, 8);

		vertices = {
			FVector(50.0f, 50.0f, 0.0f),
			FVector(50.0f, 150.0f, 0.0f),
			FVector(150.0f, 150.0f, 0.0f),
			FVector(150.0f, 50.0f, 0.0f)
		};

		TestTrue(TEXT("Add first face"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
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
			tempGraph.GetDeltaForFaceAddition(secondVertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);

		OutDeltas.AddDefaulted();
		TestTrue(TEXT("Delete face, including connected edges and vertices"),
			tempGraph.GetDeltaForDeleteObjects({}, {}, { firstFaceID }, {}, OutDeltas[0], true));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		OutDeltas.AddDefaulted();
		TestTrue(TEXT("Delete face, excluding connected edges and vertices"),
			tempGraph.GetDeltaForDeleteObjects({}, {}, { firstFaceID }, {}, OutDeltas[0], false));
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
				tempGraph.GetDeltaForVertexAddition(vertex, deltas[0], NextID, ExistingID));
		}
		TestDeltas(this, deltas, graph, tempGraph, 0, 6, 0);

		for (int i = 0; i < 4; i++)
		{
			TPair<int32, int32> edge1 = (i % 2 == 0) ? (TPair<int32, int32>(1, 2)) : (TPair<int32, int32>(2, 1));
			TPair<int32, int32> edge2 = (i / 2 == 0) ? (TPair<int32, int32>(2, 3)) : (TPair<int32, int32>(3, 2));

			OutDeltas.AddDefaulted(2);
			TestTrue(TEXT("Add edge"),
				tempGraph.GetDeltaForEdgeAddition(edge1, OutDeltas[0], NextID, ExistingID));
			TestTrue(TEXT("Add edge"),
				tempGraph.GetDeltaForEdgeAddition(edge2, OutDeltas[0], NextID, ExistingID));
			ApplyDeltas(this, graph, tempGraph, OutDeltas);
			TestTrue(TEXT("Join edge"),
				tempGraph.GetDeltaForEdgeJoin(OutDeltas[1], NextID, TPair<int32, int32>(NextID - 1, NextID - 2)));
			TArray<FGraph3DDelta> partial = { OutDeltas[0] };
			ApplyInverseDeltas(this, graph, tempGraph, partial);
			TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 0, 5, 1);
		}

		TestTrue(TEXT("Add face"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 6, 6);

		// only one face
		for (auto kvp : graph.GetFaces())
		{
			auto face = kvp.Value;

			for (int32 edgeIdx = 0; edgeIdx < face.EdgeIDs.Num(); edgeIdx++)
			{
				OutDeltas.AddDefaulted();
				TestTrue(TEXT("Join edge"),
					tempGraph.GetDeltaForEdgeJoin(OutDeltas[0], NextID, TPair<int32, int32>(face.EdgeIDs[edgeIdx], face.EdgeIDs[(edgeIdx + 1) % face.EdgeIDs.Num()])));
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
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f)
		};

		TestTrue(TEXT("Add second face"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 2, 6, 7);
		
		TArray<int32> faceIDs;
		graph.GetFaces().GenerateKeyArray(faceIDs);

		TestTrue(TEXT("join faces"),
			tempGraph.GetDeltasForObjectJoin(OutDeltas, faceIDs, NextID, EGraph3DObjectType::Face));
		TestDeltasAndResetGraph(this, OutDeltas, graph, tempGraph, 1, 4, 4);

		vertices = {
			FVector(50.0f, 0.0f, 0.0f),
			FVector(50.0f, 100.0f, 0.0f),
			FVector(50.0f, 100.0f, 100.0f),
			FVector(50.0f, 0.0f, 100.0f)
		};

		TestTrue(TEXT("Add third face that prevents the original join"),
			tempGraph.GetDeltaForFaceAddition(vertices, OutDeltas, NextID, ExistingID));
		TestDeltas(this, OutDeltas, graph, tempGraph, 3, 8, 10);

		TestTrue(TEXT("fail to join faces when there is another face connected to the edge"),
			!tempGraph.GetDeltasForObjectJoin(OutDeltas, faceIDs, NextID, EGraph3DObjectType::Face));
		ResetGraph(this, OutDeltas, graph, tempGraph);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphJoinFacesSchool, "Modumate.Graph.3D.JoinFacesSchool", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphJoinFacesSchool::RunTest(const FString& Parameters)
	{
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> OutDeltas;
		int32 NextID = 1;
		int32 ExistingID = 0;
	
		int32 expectedFaces = 168;
		int32 expectedVertices = 214;
		int32 expectedEdges = 381;

		if (!LoadGraph(TEXT("Graph/join_bug.mdmt"), graph, NextID))
		{
			return false;
		}
		TestGraph(this, graph, expectedFaces, expectedVertices, expectedEdges);
		FGraph3D::CloneFromGraph(tempGraph, graph);

		// this test file has a large face that is meant to be hexagonal.  To make it hexagonal, the two smaller
		// faces need to be joined with it.
		int32 bigJoinFaceID = 2455;
		int32 smallJoinFaceID1 = 2451;
		int32 smallJoinFaceID2 = 2430;
		if (!graph.ContainsObject(FTypedGraphObjID(bigJoinFaceID, EGraph3DObjectType::Face)) ||
			!graph.ContainsObject(FTypedGraphObjID(smallJoinFaceID2, EGraph3DObjectType::Face)) ||
			!graph.ContainsObject(FTypedGraphObjID(smallJoinFaceID1, EGraph3DObjectType::Face)))
		{
			return false;
		}

		TestTrue(TEXT("join one"),
			tempGraph.GetDeltasForFaceJoin(OutDeltas, { bigJoinFaceID, smallJoinFaceID1 }, NextID));

		// verify there is one added face and find it
		// TODO: potentially more functions to help verify the deletions as well
		int32 addedFaceID = MOD_ID_NONE;
		for (auto& delta : OutDeltas)
		{
			if (delta.FaceAdditions.Num() > 0)
			{
				TestTrue(TEXT("make sure face isn't assigned"), addedFaceID == MOD_ID_NONE);
				TestTrue(TEXT("make sure only one face is added"), delta.FaceAdditions.Num() == 1);

				for (auto& kvp : delta.FaceAdditions)
				{
					addedFaceID = kvp.Key;
				}
			}
		}
		TestTrue(TEXT("make sure face is assigned"), addedFaceID != MOD_ID_NONE);

		expectedFaces -= 1;
		expectedVertices -= 1;
		expectedEdges -= 2;
		TestDeltas(this, OutDeltas, graph, tempGraph, expectedFaces, expectedVertices, expectedEdges);

		TestTrue(TEXT("join two"),
			tempGraph.GetDeltasForFaceJoin(OutDeltas, { addedFaceID, smallJoinFaceID2 }, NextID));

		expectedFaces -= 1;
		expectedVertices -= 1;
		expectedEdges -= 2;
		TestDeltas(this, OutDeltas, graph, tempGraph, expectedFaces, expectedVertices, expectedEdges);

		// the basic verification above was necessary to get the addedFaceID for the second join
		// TODO: generalize/add verification methods that test added/removed objects and find their IDs

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGraphGroupIDsChange, "Modumate.Graph.3D.GroupIDsChange", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGraphGroupIDsChange::RunTest(const FString& Parameters)
	{
		// test adding edges, then changing group IDs, then test adding edges and setting group IDs with the same delta
		// test adding edge/face, changing group IDs, splitting them, making sure group IDs are split properly
		FGraph3D graph;
		FGraph3D tempGraph;

		TArray<FGraph3DDelta> deltas;
		int32 nextID = 1;
		int32 existingID = 0;

		// First, add a simple square face
		TArray<FVector> vertices = {
			FVector(0.0f, 0.0f, 0.0f),
			FVector(100.0f, 0.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f),
			FVector(0.0f, 100.0f, 0.0f)
		};

		TestTrue(TEXT("Add face"),
			tempGraph.GetDeltaForFaceAddition(vertices, deltas, nextID, existingID));
		TestDeltas(this, deltas, graph, tempGraph, 1, 4, 4);

		int32 groupID = 1337;

		// Add the face's edge's to a group
		deltas.Reset();
		FGraph3DDelta &groupIDsDelta = deltas.AddDefaulted_GetRef();
		TSet<int32> groupIDsToAdd({ groupID }), groupIDsToRemove;
		for (auto &kvp : graph.GetEdges())
		{
			int32 edgeID = kvp.Key;
			auto &graphEdge = kvp.Value;
			groupIDsDelta.GroupIDsUpdates.Add(FTypedGraphObjID(edgeID, EGraph3DObjectType::Edge), FGraph3DGroupIDsDelta(groupIDsToAdd, groupIDsToRemove));
		}

		TestDeltas(this, deltas, graph, tempGraph, 1, 4, 4);

		TSet<int32> originalEdgeIDs;
		for (auto &kvp : graph.GetEdges())
		{
			originalEdgeIDs.Add(kvp.Key);
			auto &graphEdge = kvp.Value;
			TestTrue(TEXT("Edge has GroupID"), graphEdge.GroupIDs.Contains(groupID) && (graphEdge.GroupIDs.Num() == 1));
		}

		TSet<FTypedGraphObjID> groupMembers;
		TestTrue(TEXT("Graph has cached group"), graph.GetGroup(groupID, groupMembers) && (groupMembers.Num() == 4));

		// Add another face, adjacent to the first
		vertices = {
			FVector(100.0f, 0.0f, 0.0f),
			FVector(200.0f, 0.0f, 0.0f),
			FVector(200.0f, 100.0f, 0.0f),
			FVector(100.0f, 100.0f, 0.0f)
		};

		TestTrue(TEXT("Add second face"),
			tempGraph.GetDeltaForFaceAddition(vertices, deltas, nextID, existingID));
		TestDeltas(this, deltas, graph, tempGraph, 2, 6, 7);

		// Make sure the new edges didn't inherit the group, and the old edges kept it
		for (auto &kvp : graph.GetEdges())
		{
			int32 edgeID = kvp.Key;
			auto &graphEdge = kvp.Value;
			TestTrue(TEXT("Only original edges have GroupID"), graphEdge.GroupIDs.Contains(groupID) == originalEdgeIDs.Contains(edgeID));
		}

		// Add an edge that should split both faces
		deltas.Reset();
		vertices = {
			FVector(0.0f, 25.0f, 0.0f),
			FVector(200.0f, 75.0f, 0.0f)
		};
		TArray<int32> newEdgeIDs;
		TestTrue(TEXT("Split faces with new edge"),
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vertices[0], vertices[1], deltas, nextID, newEdgeIDs, true));
		TestDeltas(this, deltas, graph, tempGraph, 4, 9, 12);

		// And make sure that the group still has the right number of members, including split edges
		TestTrue(TEXT("Graph group was split"), graph.GetGroup(groupID, groupMembers) && (groupMembers.Num() == 6));

		return true;
	}
}
