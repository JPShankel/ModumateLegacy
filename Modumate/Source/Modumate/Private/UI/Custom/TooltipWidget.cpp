// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/TooltipWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "Components/HorizontalBox.h"
#include "UI/Custom/TooltipKeyTextWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/HorizontalBoxSlot.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

#define LOCTEXT_NAMESPACE "UTooltipWidget"

UTooltipWidget::UTooltipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTooltipWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	return true;
}

void UTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PlayAnimation(FadeInAnim);
}

void UTooltipWidget::BuildPrimaryTooltip(const FTooltipData& InData)
{
	TextBlock_Title->ChangeText(InData.TooltipTitle);
	TextBlock_Desc->ChangeText(InData.TooltipText);

	// Primary tooltips with inputs should only exist during edit model
	// If main menu needs primary tooltips with inputs, use StartRootMenuWidget for UserWidgetPool
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!controller)
	{
		return;
	}

	HorizontalBoxForKeys->ClearChildren();
	for (int32 i = 0; i < InData.AllBindings.Num(); ++i)
	{
		for (auto& curInput : InData.AllBindings[i])
		{
			UTooltipKeyTextWidget* newTextWidget = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UTooltipKeyTextWidget>(TooltipKeyTextWidgetClass);
			newTextWidget->ChangeText(curInput.GetInputText());
			AddWidgetToHorizontalBoxForKeys(newTextWidget);
		}

		if (InData.AllBindings.Num() > i + 1)
		{
			UTooltipKeyTextWidget* newTextWidget = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UTooltipKeyTextWidget>(NonBoxedTooltipKeyTextWidgetClass);
			newTextWidget->ChangeText(LOCTEXT("or", " / "));
			AddWidgetToHorizontalBoxForKeys(newTextWidget);
		}
	}
}

void UTooltipWidget::BuildSecondaryTooltip(const FTooltipData& InData)
{
	TextBlock_Desc->ChangeText(InData.TooltipText);
}

void UTooltipWidget::AddWidgetToHorizontalBoxForKeys(class UWidget* InWidget)
{
	HorizontalBoxForKeys->AddChildToHorizontalBox(InWidget);
	UHorizontalBoxSlot* horzSlot = UWidgetLayoutLibrary::SlotAsHorizontalBoxSlot(InWidget);
	horzSlot->SetPadding(FMargin(1.f, 0.f, 1.f, 0.f));
}

#undef LOCTEXT_NAMESPACE
