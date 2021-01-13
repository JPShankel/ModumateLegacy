// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "Components/VerticalBox.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/TutorialMenu/TutorialMenuCardWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Sizebox.h"


UTutorialMenuWidget::UTutorialMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UTutorialMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TitleHeader->SetVisibility(AsStartMenuTutorial ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	TutorialTextHeader->SetVisibility(AsStartMenuTutorial ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UTutorialMenuWidget::BuildTutorialMenu(const TArray<FTutorialMenuCardInfo>& InTutorialCards)
{
	CurrentTutorialMenuCardInfo = InTutorialCards;
	VerticalBoxTutorialCards->ClearChildren();

	for (int32 i = 0; i < CurrentTutorialMenuCardInfo.Num(); ++i)
	{
		if (CurrentTutorialMenuCardInfo[i].ShowTitleOnly)
		{
			UModumateTextBlockUserWidget* titleCardWidget = CreateWidget<UModumateTextBlockUserWidget>(this, TutorialTitleTextClass);
			if (titleCardWidget)
			{
				titleCardWidget->EllipsizeWordAt = EllipsizeTitleWordAt;
				titleCardWidget->ChangeText(FText::FromString(CurrentTutorialMenuCardInfo[i].Title));
				VerticalBoxTutorialCards->AddChildToVerticalBox(titleCardWidget);
				UVerticalBoxSlot* slot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(titleCardWidget);
				slot->SetPadding(TitleSectionMargin);
			}
		}
		else
		{
			UTutorialMenuCardWidget* tutorialCardWidget = CreateWidget<UTutorialMenuCardWidget>(this, TutorialCardClass);
			if (tutorialCardWidget)
			{
				tutorialCardWidget->BuildTutorialCard(CurrentTutorialMenuCardInfo[i]);
				VerticalBoxTutorialCards->AddChildToVerticalBox(tutorialCardWidget);
				UVerticalBoxSlot* slot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(tutorialCardWidget);
				slot->SetPadding(CardMargin);
			}
		}
	}
}