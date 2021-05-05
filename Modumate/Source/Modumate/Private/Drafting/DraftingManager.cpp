// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/DraftingManager.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "Drafting/ModumateDraftingView.h"

const FString UDraftingManager::CutPlaneDirName(TEXT("CutPlaneRenders"));
const FString UDraftingManager::CutPlaneImageExt(TEXT(".png"));

void UDraftingManager::Init()
{
	// it is preferable for this to happen on shutdown, but check here as well just in case
	ClearCutPlaneRendersDir();
}

void UDraftingManager::Reset()
{
	ClearCutPlaneRendersDir();
}

void UDraftingManager::Shutdown()
{
	ClearCutPlaneRendersDir();
}

bool UDraftingManager::OnPageCompleted(SceneCaptureID SceneCaptureObjID, UWorld *WorldContextObject)
{
	UModumateGameInstance *modGameInst = WorldContextObject ? WorldContextObject->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;

	int32 &count = draftMan->SceneCaptureIDToDrawingAmounts[SceneCaptureObjID];
	count--;
	if (count == 0)
	{
		draftMan->SceneCaptureIDToDrawingAmounts.Remove(SceneCaptureObjID);
	}

	if (draftMan->SceneCaptureIDToDrawingAmounts.Num() == 0)
	{
		if (draftMan->CurrentDraftingView != nullptr)
		{
			draftMan->CurrentDraftingView->FinishDraft();
		}
	}

	return true;
}

bool UDraftingManager::IsRenderPending(UWorld *WorldContextObject)
{
	UModumateGameInstance *modGameInst = WorldContextObject ? WorldContextObject->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;

	return draftMan->SceneCaptureIDToDrawingAmounts.Num() > 0;
}

bool UDraftingManager::GetImageName(SceneCaptureID SceneCaptureObjID, FString &OutName)
{
	OutName =	FString::FromInt(SceneCaptureObjID.Key) + FString("-") + 
				FString::FromInt(SceneCaptureObjID.Value) + UDraftingManager::CutPlaneImageExt;

	return true;
}

bool UDraftingManager::GetImagePath(FString &OutPath)
{
	OutPath = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), *UDraftingManager::CutPlaneDirName);

	return true;
}

bool UDraftingManager::GetImageFullPath(SceneCaptureID SceneCaptureObjID, FString &OutPath)
{
	FString filePath, fileName;
	UDraftingManager::GetImagePath(filePath);
	UDraftingManager::GetImageName(SceneCaptureObjID, fileName);

	OutPath = FPaths::Combine(*filePath, *fileName);

	return true;
}

bool UDraftingManager::RequestRender(SceneCaptureID SceneCaptureObjID)
{
	int32 &count = SceneCaptureIDToDrawingAmounts.FindOrAdd(SceneCaptureObjID);
	count++;

	return true;
}

void UDraftingManager::ClearCutPlaneRendersDir()
{
	FString CutPlaneRendersPath = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), TEXT("CutPlaneRenders"));
	IFileManager::Get().DeleteDirectory(*CutPlaneRendersPath, false, true);
}
