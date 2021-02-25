// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/ComponentAssemblyListItem.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"

#include "ComponentListObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UComponentListObject : public UObject
{
	GENERATED_BODY()

public:

	EComponentListItemType ItemType = EComponentListItemType::None;
	EToolMode Mode = EToolMode::VE_NONE;
	FGuid UniqueKey;
	EObjectType ObjType = EObjectType::OTNone;
	int32 SelectionItemCount = 0;

	// The BIM node that is pending swap
	FBIMEditorNodeIDType BIMNodeInstanceID;

	FBIMPresetFormElement FormElement;
};
