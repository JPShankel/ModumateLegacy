// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialWalkthroughBlockIntro.h"

UTutorialWalkthroughBlockIntro::UTutorialWalkthroughBlockIntro(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialWalkthroughBlockIntro::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonIntroProceed && ButtonIntroGoBack))
	{
		return false;
	}

	return true;
}

void UTutorialWalkthroughBlockIntro::NativeConstruct()
{
	Super::NativeConstruct();
}