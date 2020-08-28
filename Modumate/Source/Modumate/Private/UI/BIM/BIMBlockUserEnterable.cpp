// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockUserEnterable.h"

UBIMBlockUserEnterable::UBIMBlockUserEnterable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockUserEnterable::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBIMBlockUserEnterable::NativeConstruct()
{
	Super::NativeConstruct();
}
