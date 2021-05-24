// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCopyTool.h"

#include "HAL/PlatformApplicationMisc.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
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
		objIds.Add(obj->ID);
	}

	auto doc = Controller->GetDocument();

	FMOIDocumentRecord selectionRecord;
	FModumateObjectDeltaStatics::SaveSelection(objIds, doc, &selectionRecord);

	TSharedPtr<FJsonObject> selectionJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FMOIDocumentRecord>(selectionRecord);
	selectionJson->SetObjectField(DocObjectInstanceField, docOb);

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
