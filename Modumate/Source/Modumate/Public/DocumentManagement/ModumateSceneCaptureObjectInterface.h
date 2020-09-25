// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

namespace Modumate
{
	class FDraftingComposite;
}

DECLARE_MULTICAST_DELEGATE(FSceneCaptureHandlerDelegate);

class MODUMATE_API ISceneCaptureObject
{
public:
	virtual ~ISceneCaptureObject() {};

	virtual bool StartRender(FModumateDocument* doc = nullptr) = 0;
	virtual void PublishPage() = 0;
	virtual void TraceRequestComplete() = 0;

	virtual bool AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea) = 0;

	FSceneCaptureHandlerDelegate CaptureDelegate;
protected:
	int32 PendingTraceRequests { 0 };
};
