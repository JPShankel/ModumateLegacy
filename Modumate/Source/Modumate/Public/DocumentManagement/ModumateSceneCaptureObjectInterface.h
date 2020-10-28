// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

namespace Modumate
{
	class FDraftingComposite;
	enum class FModumateLayerType;
}

DECLARE_MULTICAST_DELEGATE(FSceneCaptureHandlerDelegate);

class MODUMATE_API ISceneCaptureObject
{
public:
	virtual ~ISceneCaptureObject() {};

	virtual bool StartRender(FModumateDocument* doc = nullptr) = 0;
	virtual void CaptureComplete() = 0;
	virtual void TraceRequestComplete(int32 TraceID, FString TraceString) = 0;

	virtual bool AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea) = 0;
	virtual void SetupPendingRenders() = 0;

	FSceneCaptureHandlerDelegate CaptureDelegate;
protected:
	struct FPendingObjectRender
	{
		int32 MoiId = MOD_ID_NONE;
		FVector2D PagePosition { ForceInitToZero };
		FVector2D Size { ForceInitToZero };
		float Depth = 0.0f;
		Modumate::FModumateLayerType LayerType;
		FString JsonTrace;
		int32 LocalId = 0;
	};

	int32 PendingTraceRequests { 0 };
};
