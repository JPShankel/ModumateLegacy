// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Online/HTTP/Public/Http.h"

class FMOICutPlaneImpl;
class FModumateDocument;

namespace Modumate
{
	class FModumateAccountManager;

	// TODO: Combine with FModumateDwgConnnect (ModumateCloudConnect?).
	class FModumateAutotraceConnect
	{
	public:
		bool ConvertImageFromFile(const FString& filename, FMOICutPlaneImpl* cutPlane, int32 cutPlaneID, UWorld* world);

	private:
		struct ResponseHandler;
	};

}