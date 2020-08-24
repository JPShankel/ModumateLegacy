// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuBlock.h"
#include "Components/ListView.h"


UCutPlaneMenuBlock::UCutPlaneMenuBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCutPlaneMenuBlock::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	return true;
}

void UCutPlaneMenuBlock::NativeConstruct()
{
	Super::NativeConstruct();
}