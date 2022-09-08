// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCopyTool.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



UCopyTool::UCopyTool()
	: Super()
{

}

bool UCopyTool::Activate()
{
	Super::Activate();

	TArray<int32> objIds;
	for (auto obj : Controller->EMPlayerState->SelectedObjects)
	{
		// Filter out object types that can't be pasted effectively
		switch (obj->GetObjectType())
		{
		case EObjectType::OTTerrainMaterial:
		case EObjectType::OTTerrain:
		case EObjectType::OTTerrainVertex:
		case EObjectType::OTTerrainEdge:
		case EObjectType::OTTerrainPolygon:
			continue;
		}
		objIds.Add(obj->ID);
	}
	Algo::Transform(Controller->EMPlayerState->SelectedGroupObjects, objIds, [](AModumateObjectInstance* o) { return o->ID; });

	auto doc = Controller->GetDocument();

	FMOIDocumentRecord selectionRecord;
	FModumateObjectDeltaStatics::SaveSelection(objIds, doc, &selectionRecord);

	TSharedPtr<FJsonObject> selectionJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FMOIDocumentRecord>(selectionRecord);
	selectionJson->SetObjectField(FModumateSerializationStatics::DocObjectInstanceField, docOb);

	FString ProjectJsonString;
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

	bool bWriteJsonSuccess = FJsonSerializer::Serialize(selectionJson.ToSharedRef(), JsonStringWriter);
	if (!bWriteJsonSuccess)
	{
		return false;
	}

	FPlatformApplicationMisc::ClipboardCopy(*ProjectJsonString);

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return true;
}
