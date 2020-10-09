// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/ComponentAssemblyListItem.h"
#include "BIMKernel/BIMKey.h"

#include "ComponentListObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UComponentListObject : public UObject
{
	GENERATED_BODY()

public:

	EComponentListItemType ItemType = EComponentListItemType::None;
	EToolMode Mode = EToolMode::VE_NONE;
	FBIMKey UniqueKey;
	int32 SelectionItemCount = 0;

	// The BIM node that is pending swap
	int32 BIMNodeInstanceID = INDEX_NONE;
};
