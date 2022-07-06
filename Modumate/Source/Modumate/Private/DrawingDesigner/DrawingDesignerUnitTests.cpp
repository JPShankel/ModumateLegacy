// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerUnitTests.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Misc/AutomationTest.h"
#include "JsonObjectConverter.h"
#include "Objects/CutPlane.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDrawingDesignerViewTest, "Modumate.DrawingDesigner.ViewTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateDrawingDesignerViewTest::RunTest(const FString& Parameters)
{
	UModumateDocument* testDocument = NewObject<UModumateDocument>();
	bool doVerbose = false;
	if (doVerbose) FPlatformProcess::Sleep(15.0);

	FDrawingDesignerViewList myList;
	FWebMOI moi;
	moi.isVisible = true;
	moi.Type = EObjectType::OTCutPlane;
	moi.DisplayName = "CutPlane 1";
	
	myList.cutPlanes.Add(moi);

	FString response;
	if (!myList.WriteJson(response))
	{
		UE_LOG(LogTemp, Error, TEXT("FDrawingDesignerViewList failed to serialize"));
		return false;
	}

	FDrawingDesignerViewList myListCopy;
	if (!myListCopy.ReadJson(response) || myListCopy != myList)
	{
		UE_LOG(LogTemp, Error, TEXT("FDrawingDesignerViewList failed to deserialize or the deserialization does not match"));
		return false;
	}
	FDrawingDesignerDrawingRequest req;
	req.minimum_resolution_pixels.x = 500;
	req.minimum_resolution_pixels.y = 500;
	req.moi_id = 999;

	FDrawingDesignerDrawingImage image;
	image.view = moi;
	image.image_base64 = TEXT("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==");

	FDrawingDesignerDrawingResponse resp;
	resp.request = req;
	resp.response = image;

	if (!resp.WriteJson(response))
	{
		UE_LOG(LogTemp, Error, TEXT("FDrawingDesignerViewResponse failed to serialize"));
		return false;
	}

	FDrawingDesignerDrawingResponse respCopy;
	if (!respCopy.ReadJson(response) || respCopy != resp)
	{
		UE_LOG(LogTemp, Error, TEXT("FDrawingDesignerViewResponse failed to deserialize or the deserialization does not match"));
		return false;
	}

	return true;
}

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
	FString addRootDirectoryStr   { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":0},\"details\":{\"id\":-1,\"parent\":0,\"children\":[],\"nodeType\":\"directory\",\"chunkString\":\"\"}}]}") }; 
	//ID2
	FString addSubDirectory1Str   { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":1},\"details\":{\"id\":-1,\"parent\":1,\"children\":[],\"nodeType\":\"directory\",\"chunkString\":\"\"}}]}") };
	//ID3
	FString addSubDirectory2Str   { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":1},\"details\":{\"id\":-1,\"parent\":1,\"children\":[],\"nodeType\":\"directory\",\"chunkString\":\"\"}}]}") };


	//ID4
	FString addPageStr      { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":2},\"details\":{\"id\":-1,\"parent\":2,\"children\":[],\"nodeType\":\"page\",\"chunkString\":\"\"}}]}") };
	//ID5
	FString addAnnotationStr{ TEXT("{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":4},\"details\":{\"id\":-1,\"parent\":4,\"children\":[],\"nodeType\":\"annotation\",\"chunkString\":\"\"}}]}") };

	FString modifyPageStr   { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"modify\",\"id\":4,\"parent\":3},\"details\":{\"id\":4,\"parent\":3,\"children\":[ 5 ],\"nodeType\":\"page\",\"chunkString\":\"\"}}]}") };

	//This adds a cycle to the graph
	FString invalidateAdd { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"add\",\"id\":-1,\"parent\":0},\"details\":{\"id\":-1,\"parent\":0,\"children\":[ 3 ],\"nodeType\":\"directory\",\"chunkString\":\"\"}}]}") };

	//Removes ID4, which also removes ID5
	FString removePageStr   { TEXT("{\"deltas\":[{\"header\":{\"verb\":\"remove\",\"id\":4,\"parent\":3}}]}") };

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