// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UnrealClasses/EditModelPlayerController.h"

class MODUMATE_API FSelectedObjectToolMixin
{
protected:

	TSet<int32> OriginalSelectedObjects;
	TMap<int32, FTransform> OriginalTransforms;
	TWeakObjectPtr<AEditModelPlayerController> ControllerPtr;

	FSelectedObjectToolMixin(AEditModelPlayerController *InController = nullptr);

	void AcquireSelectedObjects();
	void ReleaseSelectedObjects();
	void ReleaseObjectsAndApplyDeltas();

	static const FName StateRequestTag;
};
