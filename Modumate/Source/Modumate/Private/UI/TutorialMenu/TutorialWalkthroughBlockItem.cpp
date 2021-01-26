// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialWalkthroughBlockItem.h"

UTutorialWalkthroughBlockItem::UTutorialWalkthroughBlockItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialWalkthroughBlockItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonWalkthroughProceed && ButtonWalkthroughGoBack))
	{
		return false;
	}

	return true;
}

void UTutorialWalkthroughBlockItem::NativeConstruct()
{
	Super::NativeConstruct();
}