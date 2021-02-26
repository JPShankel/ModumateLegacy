// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateSerialization.h"

struct FMOIDocumentRecordV4;
using FMOIDocumentRecord = FMOIDocumentRecordV4;

class MODUMATE_API FSelectedObjectToolMixin
{
protected:

	TSet<int32> OriginalSelectedObjects;
	TMap<int32, FTransform> OriginalTransforms;
	TWeakObjectPtr<AEditModelPlayerController> ControllerPtr;

	bool bPaste = false;
	FMOIDocumentRecord CurrentRecord;

	FSelectedObjectToolMixin(AEditModelPlayerController *InController = nullptr);

	void AcquireSelectedObjects();
	void ReleaseSelectedObjects();
	void ReleaseObjectsAndApplyDeltas();

	void ToggleIsPasting() { bPaste = !bPaste; }

	static const FName StateRequestTag;
};
