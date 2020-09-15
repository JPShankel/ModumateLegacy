// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UnrealClasses/EditModelPlayerController_CPP.h"

class MODUMATE_API FSelectedObjectToolMixin
{
protected:

	TSet<int32> OriginalSelectedObjects;
	TMap<int32, FTransform> OriginalTransforms;
	TWeakObjectPtr<AEditModelPlayerController_CPP> ControllerPtr;

	FSelectedObjectToolMixin(AEditModelPlayerController_CPP *InController = nullptr);

	void AcquireSelectedObjects();
	void ReleaseSelectedObjects();
	void ReleaseObjectsAndApplyDeltas();

	static const FName StateRequestTag;
};
