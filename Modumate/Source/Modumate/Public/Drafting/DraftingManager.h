// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UObject/Object.h"

#include "DraftingManager.generated.h"

class FModumateDraftingView;
class IModumateDraftingDraw;

// the information associated with a scene capture is a 
// cutPlane ID (key) and a scopeBox ID (value)
typedef TPair<int32, int32> SceneCaptureID;

UCLASS()
class MODUMATE_API UDraftingManager : public UObject
{
	GENERATED_BODY()

public:
	static const FString CutPlaneDirName;
	static const FString CutPlaneImageExt;

	// TODO: currently, these functions all have the same body, 
	// it is unclear what the distinction should and will be.
	void Init();
	void Reset();
	void Shutdown();

	enum EDraftType { kDWG, kDD };

// communication with rendering surfaces
public:
	static bool OnPageCompleted(SceneCaptureID SceneCaptureObjID, UWorld *WorldContextObject);
	static bool IsRenderPending(UWorld *WorldContextObject);

	static bool GetImageName(SceneCaptureID SceneCaptureObjID, FString &OutName);
	static bool GetImagePath(FString &OutPath);
	static bool GetImageFullPath(SceneCaptureID SceneCaptureObjID, FString &OutPath);

	bool RequestRender(SceneCaptureID SceneCaptureObjID);

public:
	void ClearCutPlaneRendersDir();

	TMap<SceneCaptureID, int32> SceneCaptureIDToDrawingAmounts;

	FModumateDraftingView *CurrentDraftingView = nullptr;
	IModumateDraftingDraw *CurrentDrawingInterface = nullptr;
};
