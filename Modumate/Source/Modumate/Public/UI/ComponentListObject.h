// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/ComponentAssemblyListItem.h"

#include "ComponentListObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UComponentListObject : public UObject
{
	GENERATED_BODY()

public:

	EComponentListItemType ItemType;
	EToolMode Mode;
	FName UniqueKey;
	int32 SelectionItemCount = 0;
};
