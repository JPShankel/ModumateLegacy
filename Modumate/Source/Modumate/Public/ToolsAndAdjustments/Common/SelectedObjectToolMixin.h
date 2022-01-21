// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateSerialization.h"

class MODUMATE_API FSelectedObjectToolMixin
{
protected:

	TSet<int32> OriginalSelectedObjects;
	TSet<int32> OriginalSelectedGroupObjects;
	TMap<int32, FTransform> OriginalTransforms;
	TMap<int32, FTransform> OriginalGroupVertexTransforms;
	TWeakObjectPtr<AEditModelPlayerController> ControllerPtr;

	bool bPaste = false;
	FMOIDocumentRecord CurrentRecord;
	TArray<TPair<bool, FDeltaPtr>> GroupCopyDeltas;

	FSelectedObjectToolMixin(AEditModelPlayerController *InController = nullptr);

	void AcquireSelectedObjects();
	void ReleaseSelectedObjects();
	void ReleaseObjectsAndApplyDeltas(const TArray<FDeltaPtr>* AdditionalDeltas = nullptr);

	void ToggleIsPasting() { bPaste = !bPaste; }

	static const FName StateRequestTag;
};
