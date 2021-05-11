// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/GeneralListItemMenuBlock.h"
#include "Components/ListView.h"


UGeneralListItemMenuBlock::UGeneralListItemMenuBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UGeneralListItemMenuBlock::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	return true;
}

void UGeneralListItemMenuBlock::NativeConstruct()
{
	Super::NativeConstruct();
}