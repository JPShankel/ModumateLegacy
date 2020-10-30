// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingTags.h"

namespace Modumate {

	// TODO: purpose of this class should be to have:
	// bounding rectangle object
	// layout FDraftingComposite objects (Title Bar, DrawableArea)
	class FDraftingPage : public FDraftingComposite
	{
	public:
		FString StampPath;

		// Sheet Info
		int32 PageNumber;
		FText Date;
		FString Name;
		FString Number;

		float DrawingScale;
	};

}