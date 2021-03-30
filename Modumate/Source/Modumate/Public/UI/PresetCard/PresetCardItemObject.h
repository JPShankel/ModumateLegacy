// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/PresetCard/PresetCardMain.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"

#include "PresetCardItemObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UPresetCardItemObject : public UObject
{
	GENERATED_BODY()

public:

	EPresetCardType PresetCardType = EPresetCardType::None;
	FGuid PresetGuid;
	bool bPresetCardExpanded = false;

	// As Select Tray
	UPROPERTY()
	class USelectionTrayBlockPresetList* ParentSelectionTrayBlockPresetList;

	// As AssembliesList
	UPROPERTY()
	class UToolTrayBlockAssembliesList* ParentToolTrayBlockAssembliesList;

	int32 SelectionItemCount = 0;
	EObjectType ObjectType = EObjectType::OTNone;
};
