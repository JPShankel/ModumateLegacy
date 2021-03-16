// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMTagPath.h"

#include "BrowserItemObj.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UBrowserItemObj : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	class UNCPNavigator* ParentNCPNavigator;

	bool bAsPresetCard = false;

	// As NCPButton
	FBIMTagPath NCPTag;
	int32 TagOrder;
	bool bNCPButtonExpanded = false;

	// As PresetCard
	FGuid PresetGuid;
	bool bPresetCardExpanded = false;
};
