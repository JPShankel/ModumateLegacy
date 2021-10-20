// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerUnitTests.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Misc/AutomationTest.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


static bool createDeltaAndApply(const FString& json, UModumateDocument* document, int expectedNodeCount)
{
	FDrawingDesignerJsDeltaPackage  package;
	if (!package.ReadJson(json))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read JSON package"));
		return false;
	}

	FDrawingDesignerDocumentDelta delta = FDrawingDesignerDocumentDelta{ document->DrawingDesignerDocument, package };

	if (!delta.ApplyTo(document, NULL))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to apply Delta"));
		return false;
	}

	if (document->DrawingDesignerDocument.nodes.Num() != expectedNodeCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("nodes.Num() (%d) != expected node count (%d)"), document->DrawingDesignerDocument.nodes.Num(), expectedNodeCount);
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDrawingDesignerUnitTest, "Modumate.DrawingDesigner.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateDrawingDesignerUnitTest::RunTest(const FString& Parameters)
{
	bool doVerbose = false;
	if(doVerbose) FPlatformProcess::Sleep(15.0);
	
	UModumateDocument* testDocument = NewObject<UModumateDocument>();

	//ID1
	FString addRootDirectoryStr   { TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":0},\"details\":{\"id\":-1,\"parent\":0,\"children\":[],\"nodeType\":\"directory\",\"chunk\":{\"version\":1,\"name\":\"Root Directory\"}}}]}}") }; 
	//ID2
	FString addSubDirectory1Str   { TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":1},\"details\":{\"id\":-1,\"parent\":1,\"children\":[],\"nodeType\":\"directory\",\"chunk\":{\"version\":1,\"name\":\"Sub Directory 1\"}}}]}}") };
	//ID3
	FString addSubDirectory2Str   { TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":1},\"details\":{\"id\":-1,\"parent\":1,\"children\":[],\"nodeType\":\"directory\",\"chunk\":{\"version\":1,\"name\":\"Sub Directory 2\"}}}]}}") };


	//ID4
	FString addPageStr      { TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":2},\"details\":{\"id\":-1,\"parent\":2,\"children\":[],\"nodeType\":\"page\",\"chunk\":{\"version\":1,\"size\":{\"x\":36,\"y\":36},\"name\":\"Empty Page\"}}}]}}") };
	//ID5
	FString addAnnotationStr{ TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":4},\"details\":{\"id\":-1,\"parent\":4,\"children\":[],\"nodeType\":\"annotation\",\"chunk\":{\"version\":1,\"view\":-1,\"anchors\":[],\"type\":\"drawing\",\"offset\":{\"x\":0,\"y\":0},\"text\":\"First Annotation Text\",\"color\":\"\",\"floatingSnaps\":{\"1\":{\"id\":1,\"x\":234,\"y\":567}}}}}]}}") };

	FString modifyPageStr   { TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"modify\",\"id\":4,\"parent\":3},\"details\":{\"id\":4,\"parent\":3,\"children\":[ 5 ],\"nodeType\":\"page\",\"chunk\":{\"version\":1,\"size\":{\"x\":36,\"y\":36},\"name\":\"Modified Page\"}}}]}}") };

	//This adds a cycle to the graph
	FString invalidateAdd{ TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":0},\"details\":{\"id\":-1,\"parent\":0,\"children\":[ 3 ],\"nodeType\":\"directory\",\"chunk\":{\"version\":1,\"name\":\"Add a Cycle Directory 2\"}}}]}}") };

	//Removes ID4, which also removes ID5
	FString removePageStr   { TEXT("{\"document\":{\"deltas\":[{\"header\":{\"verb\":\"remove\",\"id\":4,\"parent\":3}}]}}") };

	int nodeCount = testDocument->DrawingDesignerDocument.nodes.Num();

	/**
	 * Add Directories
	 */
	if (!createDeltaAndApply(addRootDirectoryStr, testDocument, ++nodeCount))
	{
		return false;
	}
	if (!createDeltaAndApply(addSubDirectory1Str, testDocument, ++nodeCount))
	{
		return false;
	}
	if (!createDeltaAndApply(addSubDirectory2Str, testDocument, ++nodeCount))
	{
		return false;
	}



	/**
	 * Add Page
	 */

	if (!createDeltaAndApply(addPageStr, testDocument, ++nodeCount))
	{
		return false;
	}

	/**
	 * Add Annotation
	 */

	if (!createDeltaAndApply(addAnnotationStr, testDocument, ++nodeCount))
	{
		return false;
	}
	
	/**
	 * Test Serialization (1)
	 */
	
	FString documentAsJson;
	testDocument->DrawingDesignerDocument.WriteJson(documentAsJson);

	if (doVerbose) UE_LOG(LogTemp, Log, TEXT("Original: %s"), *documentAsJson);

	FDrawingDesignerDocument ddocCopy1;

	if (!ddocCopy1.ReadJson(documentAsJson))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to deserialize a copy of the test document (1)"));
		return false;
	}
	
	if (ddocCopy1 != testDocument->DrawingDesignerDocument)
	{
		FString copyAsJson;
		ddocCopy1.WriteJson(copyAsJson);
		UE_LOG(LogTemp, Log, TEXT("Copy: %s"), *copyAsJson);
		UE_LOG(LogTemp, Warning, TEXT("Serialization did not match (1)"));
		return false;
	}


	/**
	 * Modify Page
	 */
	if (!createDeltaAndApply(modifyPageStr, testDocument, nodeCount))
	{
		return false;
	}

	testDocument->DrawingDesignerDocument.WriteJson(documentAsJson);
	if (doVerbose) UE_LOG(LogTemp, Log, TEXT("Page Modify: %s"), *documentAsJson);

	/**
	 * Remove Page
	 */
	nodeCount = nodeCount - 2; //Removes annotation as well
	if (!createDeltaAndApply(removePageStr, testDocument, nodeCount))
	{
		return false;
	}

	testDocument->DrawingDesignerDocument.WriteJson(documentAsJson);
	if (doVerbose) UE_LOG(LogTemp, Log, TEXT("Pages Delete: %s"), *documentAsJson);

	/**
	 * Test Serialization (2)
	 */
	FDrawingDesignerDocument ddocCopy2;
	testDocument->DrawingDesignerDocument.WriteJson(documentAsJson);

	if (!ddocCopy2.ReadJson(documentAsJson))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to deserialize a copy of the test document (2)"));
		return false;
	}

	if (ddocCopy2 != testDocument->DrawingDesignerDocument)
	{
		FString copyAsJson;
		ddocCopy1.WriteJson(copyAsJson);
		UE_LOG(LogTemp, Log, TEXT("Copy: %s"), *copyAsJson);
		UE_LOG(LogTemp, Warning, TEXT("Serialization did not match (2)"));
		return false;
	}


	/**
	 * Test Validation (should pass)
	 */
	if (!testDocument->DrawingDesignerDocument.Validate())
	{
		UE_LOG(LogTemp, Warning, TEXT("Validate failed on correct graph"));
		return false;
	}

	/**
	 * Add Cycle to graph
	 */
	if (!createDeltaAndApply(invalidateAdd, testDocument, ++nodeCount))
	{
		return false;
	}

	/**
	 * Test Validation (should fail)
	 */
	if (testDocument->DrawingDesignerDocument.Validate())
	{
		UE_LOG(LogTemp, Warning, TEXT("Validate passed on bad graph"));
		return false;
	}

	return true;
}