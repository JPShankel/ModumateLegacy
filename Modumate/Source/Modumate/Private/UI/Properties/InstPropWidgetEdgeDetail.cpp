// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetEdgeDetail.h"

#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

#define LOCTEXT_NAMESPACE "ModumateInstanceProperties"

bool UInstPropWidgetEdgeDetail::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(DetailName && ButtonSwap && ButtonEdit && ButtonDelete))
	{
		return false;
	}

	ButtonSwap->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedSwap);
	ButtonEdit->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedEdit);
	ButtonDelete->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedDelete);

	return true;
}

void UInstPropWidgetEdgeDetail::ResetInstProp()
{
	Super::ResetInstProp();

	ButtonPressedEvent.Clear();
	CurrentValue = 0;
}

void UInstPropWidgetEdgeDetail::DisplayValue()
{
	bool bValidDetail = bConsistentValue && (CurrentValue != 0);

	if (bValidDetail)
	{
		// TODO: get detail name from an actual preset
		DetailName->ModumateTextBlock->SetText(FText::FromString(FString::Printf(TEXT("MOI ID #%d"), CurrentValue)));
	}
	else
	{
		DetailName->ModumateTextBlock->SetText(LOCTEXT("EdgeDetail_None", "None"));
	}

	ButtonSwap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonEdit->SetVisibility(bValidDetail ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonDelete->SetVisibility(bValidDetail ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UInstPropWidgetEdgeDetail::RegisterValue(UObject* Source, int32 DetailID)
{
	OnRegisteredValue(Source, CurrentValue == DetailID);
	CurrentValue = DetailID;
}

void UInstPropWidgetEdgeDetail::BroadcastValueChanged()
{
	ButtonPressedEvent.Broadcast(LastClickedAction);
	LastClickedAction = EEdgeDetailWidgetActions::None;
}

void UInstPropWidgetEdgeDetail::OnClickedSwap()
{
	LastClickedAction = EEdgeDetailWidgetActions::Swap;
	PostValueChanged();
}

void UInstPropWidgetEdgeDetail::OnClickedEdit()
{
	LastClickedAction = EEdgeDetailWidgetActions::Edit;
	PostValueChanged();
}

void UInstPropWidgetEdgeDetail::OnClickedDelete()
{
	LastClickedAction = EEdgeDetailWidgetActions::Delete;
	PostValueChanged();
}

#undef LOCTEXT_NAMESPACE
