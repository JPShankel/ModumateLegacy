// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/HelpBlockTutorialMoreLinks.h"
#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateButton.h"


UHelpBlockTutorialMoreLinks::UHelpBlockTutorialMoreLinks(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHelpBlockTutorialMoreLinks::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(Button_Forum && Button_WhatsNew))
	{
		return false;
	}

	Button_Forum->ModumateButton->OnReleased.AddDynamic(this, &UHelpBlockTutorialMoreLinks::OnButtonReleasedForum);
	Button_WhatsNew->ModumateButton->OnReleased.AddDynamic(this, &UHelpBlockTutorialMoreLinks::OnButtonReleasedWhatsNew);

	return true;
}

void UHelpBlockTutorialMoreLinks::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHelpBlockTutorialMoreLinks::OnButtonReleasedForum()
{
	FPlatformProcess::LaunchURL(*ForumURL, nullptr, nullptr);
}

void UHelpBlockTutorialMoreLinks::OnButtonReleasedWhatsNew()
{
	FPlatformProcess::LaunchURL(*WhatsNewURL, nullptr, nullptr);
}