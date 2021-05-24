// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Online/HTTP/Public/Http.h"

class AMOICutPlane;
class UModumateDocument;

// TODO: Combine with FModumateDwgConnnect (ModumateCloudConnect?).
class FModumateAutotraceConnect
{
public:
	bool ConvertImageFromFile(const FString& filename, int32 renderID, int32 cutPlaneID, TWeakObjectPtr<UWorld> world);

private:
	struct ResponseHandler;
};
