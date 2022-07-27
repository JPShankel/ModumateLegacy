// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Logging/LogCategory.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/StructureLine.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameState.h"
#include "ModumateCore/PrettyJSONWriter.h"


#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCreateDeleteSpan, "Modumate.MOI.Span.Create", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority);

namespace
{
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FModumateCreateDeleteSpanBody, FAutomationTestBase*, TestBase);
	bool FModumateCreateDeleteSpanBody::Update()
	{
		UWorld* world = nullptr;
		for (const FWorldContext& worldContext : GEngine->GetWorldContexts())
		{
			if (worldContext.WorldType == EWorldType::Game || worldContext.WorldType == EWorldType::PIE)
			{
				world = worldContext.World();
				break;
			}
		}

		if (!world)
		{
			return true;
		}

		bool bSuccess = true;
		UModumateDocument* doc = NewObject<UModumateDocument>(world);

		UModumateGameInstance* gameInstance = world->GetGameInstance<UModumateGameInstance>();
		AEditModelGameState* gameState = world->GetGameState<AEditModelGameState>();
		gameState->Document = doc;

		if (!doc)
		{
			return false;
		}

		doc->MakeNew(world);

		// Create edge span:
		TArray<FVector> points;
		points.Emplace(100, 200, 300);
		points.Emplace(100, 500, 300);

		TArray<int32> newIDs;
		TArray<FDeltaPtr> deltas;
		TArray<FGraph3DDelta> graphDeltas;

		bSuccess = bSuccess && doc->MakeMetaObject(world, points, newIDs, deltas, graphDeltas, true);

		bSuccess = bSuccess && doc->ApplyDeltas(deltas, world);

		TArray<AModumateObjectInstance*> metaEdgeObjects = doc->GetObjectsOfType(EObjectType::OTMetaEdge);
		if (metaEdgeObjects.Num() != 1)
		{
			UE_LOG(LogEngineAutomationTests, Error, TEXT("FModumateCreateDeleteSpan: create metaedge failed"));
			return true;
		}

		// Create face span:
		points.Emplace(150, 500, 300);
		points.Emplace(150, 200, 300);

		deltas.Empty();
		graphDeltas.Empty();

		bSuccess = bSuccess && doc->MakeMetaObject(world, points, newIDs, deltas, graphDeltas, true);

		bSuccess = bSuccess && doc->ApplyDeltas(deltas, world);

		points.Reset();
		points.Emplace(150, 500, 300);
		points.Emplace(150, 200, 300);
		points.Emplace(200, 200, 300);
		points.Emplace(200, 500, 300);
		bSuccess = bSuccess && doc->MakeMetaObject(world, points, newIDs, deltas, graphDeltas, true);

		bSuccess = bSuccess && doc->ApplyDeltas(deltas, world);

		metaEdgeObjects = doc->GetObjectsOfType(EObjectType::OTMetaEdge);
		if (metaEdgeObjects.Num() != 7)
		{
			UE_LOG(LogEngineAutomationTests, Error, TEXT("FModumateCreateDeleteSpan: create metaplane/metaedge failed"));
			return true;
		}

		int32 nextID = doc->GetNextAvailableID();
		FGuid assemblyGuid;
		FMOIStructureLineData structureLineData;
		FMOIStateData newObjectData(nextID++, EObjectType::OTStructureLine);
		newObjectData.CustomData.SaveStructData(structureLineData, true);

		int32 newEdgeSpanID = MOD_ID_NONE;
		int32 newFaceSpanID = MOD_ID_NONE;
		int32 newEdgeObjectID = MOD_ID_NONE;
		int32 newFaceObjectID = MOD_ID_NONE;

		TArray<FDeltaPtr> spanDeltas;
		bSuccess = bSuccess && FModumateObjectDeltaStatics::GetEdgeSpanCreationDeltas({ metaEdgeObjects[0]->ID }, nextID, assemblyGuid,
			newObjectData, spanDeltas, newEdgeSpanID, newEdgeObjectID);

		bSuccess = bSuccess && spanDeltas.Num() > 0 && newEdgeSpanID != MOD_ID_NONE && newEdgeObjectID != MOD_ID_NONE;
		bSuccess = bSuccess && doc->ApplyDeltas(spanDeltas, world);
		bSuccess = bSuccess && doc->GetObjectsOfType(EObjectType::OTMetaEdgeSpan).Num() == 1 && doc->GetObjectsOfType(EObjectType::OTMetaEdgeSpan)[0]->ID == newEdgeSpanID;


		const TArray<AModumateObjectInstance*> metaPlaneObjects = doc->GetObjectsOfType(EObjectType::OTMetaPlane);
		if (metaPlaneObjects.Num() != 2)
		{
			UE_LOG(LogEngineAutomationTests, Error, TEXT("FModumateCreateDeleteSpan: create metaplane failed"));
			return true;
		}

		for (int32 metaPlaneIdx = 0; metaPlaneIdx < 2; ++metaPlaneIdx)
		{
			newObjectData.ID = nextID++;
			newObjectData.ObjectType = EObjectType::OTFloorSegment;
			FMOIPlaneHostedObjData floorData;
			newObjectData.CustomData.SaveStructData(floorData, true);

			spanDeltas.Reset();
			bSuccess = bSuccess && FModumateObjectDeltaStatics::GetFaceSpanCreationDeltas({ metaPlaneObjects[metaPlaneIdx]->ID }, nextID, assemblyGuid,
				newObjectData, spanDeltas, newFaceSpanID, newFaceObjectID);

			bSuccess = bSuccess && spanDeltas.Num() > 0 && newFaceSpanID != MOD_ID_NONE && newFaceObjectID != MOD_ID_NONE;

			bSuccess = bSuccess && doc->ApplyDeltas(spanDeltas, world);
		}

		AModumateObjectInstance* spanSub = doc->GetObjectsOfType(EObjectType::OTMetaPlaneSpan)[0];
		AModumateObjectInstance* spanTar = doc->GetObjectsOfType(EObjectType::OTMetaPlaneSpan)[1];

		// Test span alignment web cycle
		FString webMOIJson;
		static const TCHAR* originalZone = TEXT("Original");
		static const TCHAR* modifiedZone = TEXT("Modified");

		auto* hostedSub = spanSub->GetChildObjects()[0];
		auto* hostedTar = spanTar->GetChildObjects()[0];

		hostedSub->GetStateData().Alignment.SubjectPZP.ZoneID = originalZone;
		hostedSub->GetStateData().Alignment.TargetPZP.ZoneID = originalZone;
		hostedSub->ToWebMOI(webMOIJson);
		hostedSub->GetStateData().Alignment.SubjectPZP.ZoneID = modifiedZone;
		hostedSub->GetStateData().Alignment.TargetPZP.ZoneID = modifiedZone;
		hostedSub->FromWebMOI(webMOIJson);
		
		bSuccess = hostedSub->GetStateData().Alignment.SubjectPZP.ZoneID.Equals(originalZone) && bSuccess;
		bSuccess = hostedSub->GetStateData().Alignment.TargetPZP.ZoneID.Equals(originalZone) && bSuccess;

		// Test alignment PZPs
		FWebMOI webMOI;
		ReadJsonGeneric<FWebMOI>(*webMOIJson, &webMOI);

		hostedSub->GetStateData().Alignment.SubjectPZP.Displacement = 50.0f;
		hostedSub->GetStateData().Alignment.SubjectPZP.MoiId = hostedSub->ID;
		hostedSub->GetStateData().Alignment.SubjectPZP.Origin = EZoneOrigin::Back;
		hostedSub->GetStateData().Alignment.SubjectPZP.ZoneID = hostedSub->GetAssembly().Layers[0].PresetZoneID;

		hostedSub->GetStateData().Alignment.TargetPZP.Displacement = 25.0f;
		hostedSub->GetStateData().Alignment.TargetPZP.MoiId = hostedTar->ID;
		hostedSub->GetStateData().Alignment.TargetPZP.Origin = EZoneOrigin::MassingPlane;
		hostedSub->GetStateData().Alignment.TargetPZP.ZoneID = hostedTar->GetAssembly().Layers[0].PresetZoneID;

		hostedSub->CleanObject(EObjectDirtyFlags::Mitering, nullptr);

		FMOIPlaneHostedObjData phd;
		hostedSub->GetStateData().CustomData.LoadStructData(phd);
		bSuccess = (phd.Offset.CustomValue > 40.0f && phd.Offset.CustomValue < 41.0f) && bSuccess;

		hostedSub->GetStateData().Alignment.SubjectPZP.Origin = EZoneOrigin::MassingPlane;
		hostedSub->GetStateData().Alignment.TargetPZP.MoiId = 0;
		hostedSub->CleanObject(EObjectDirtyFlags::Mitering, nullptr);

		hostedSub->GetStateData().CustomData.LoadStructData(phd);
		bSuccess = (phd.Offset.CustomValue > 42.0f && phd.Offset.CustomValue < 43.0f);

		// Delete spans:
		deltas.Empty();

		TArray<AModumateObjectInstance*> obs;
		obs.Append(metaEdgeObjects);
		obs.Append(metaPlaneObjects);
		bSuccess = bSuccess && doc->GetDeleteObjectsDeltas(deltas, obs);
		bSuccess = bSuccess && deltas.Num() > 0;
		bSuccess = bSuccess && doc->ApplyDeltas(deltas, world);
		bSuccess = bSuccess && doc->GetObjectsOfType({ EObjectType::OTMetaEdgeSpan, EObjectType::OTMetaPlaneSpan, EObjectType::OTMetaVertex,
			EObjectType::OTMetaEdge, EObjectType::OTMetaPlane, EObjectType::OTFloorSegment, EObjectType::OTStructureLine}).Num() == 0;

		if (!bSuccess)
		{
			UE_LOG(LogEngineAutomationTests, Error, TEXT("FModumateCreateDeleteSpan failed"));
		}

		TestBase->SetSuccessState(bSuccess);
		return true;
	}
}

bool FModumateCreateDeleteSpan::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand("EditModelLVL"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FModumateCreateDeleteSpanBody(this));
	ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand("MainMenuLVL"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
