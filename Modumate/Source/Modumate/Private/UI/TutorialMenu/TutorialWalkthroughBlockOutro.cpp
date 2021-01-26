// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialWalkthroughBlockOutro.h"

UTutorialWalkthroughBlockOutro::UTutorialWalkthroughBlockOutro(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialWalkthroughBlockOutro::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonOutroProceed && ButtonOutroGoBack))
	{
		return false;
	}

	return true;
}

void UTutorialWalkthroughBlockOutro::NativeConstruct()
{
	Super::NativeConstruct();
}