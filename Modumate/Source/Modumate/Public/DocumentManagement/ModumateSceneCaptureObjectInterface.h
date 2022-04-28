// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectEnums.h"

class FDraftingComposite;
enum class FModumateLayerType;



DECLARE_MULTICAST_DELEGATE(FSceneCaptureHandlerDelegate);

class MODUMATE_API ISceneCaptureObject
{
public:
	virtual ~ISceneCaptureObject() {};

	virtual bool StartRender(class UModumateDocument* doc = nullptr) = 0;
	virtual void CaptureComplete() = 0;
	virtual void TraceRequestComplete(int32 TraceID, FString TraceString) = 0;

	virtual bool AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea) = 0;
	virtual void SetupPendingRenders() = 0;

	FSceneCaptureHandlerDelegate CaptureDelegate;
protected:
	struct FPendingObjectRender
	{
		// Capture keys:
		FGuid Assembly;
		FQuat ItemRotation;

		int32 MoiId = MOD_ID_NONE;

		// Per instance values:
		TArray<FVector2D> PagePosition;
		TArray<float> Depth;

		FTransform RenderTransform;
		float maxDimension = 0.0f;
		float Scale = 0.0f;
		FModumateLayerType LayerType;
		FString JsonTrace;
		int32 LocalId = 0;
	};

	int32 PendingTraceRequests { 0 };
};
