// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMDebugPresetListItemObj.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UBIMDebugPresetListItemObj : public UObject
{
	GENERATED_BODY()

public:

	FGuid PresetKey;
	FText DisplayName;
	class UBIMDebugger* ParentDebugger;
	bool bItemIsPreset = false;
	bool bIsFromHistoryMenu = false;
};
