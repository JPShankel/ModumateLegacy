// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetBase.h"

#include "DocumentManagement/ModumateDocument.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"

#define LOCTEXT_NAMESPACE "ModumateInstanceProperties"

bool UInstPropWidgetBase::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(PropertyTitle))
	{
		return false;
	}

	bInstPropInitialized = true;

	return true;
}

void UInstPropWidgetBase::ResetInstProp()
{
	NumRegistrations = 0;
	bConsistentValue = true;
}

bool UInstPropWidgetBase::IsInstPropInitialized() const
{
	return bInstPropInitialized;
}

bool UInstPropWidgetBase::IsRegistered() const
{
	return NumRegistrations > 0;
}

int32 UInstPropWidgetBase::GetNumRegistrations() const
{
	return NumRegistrations;
}

bool UInstPropWidgetBase::IsValueConsistent() const
{
	return bConsistentValue;
}

void UInstPropWidgetBase::SetPropertyTitle(const FText& TitleText)
{
	PropertyTitle->ModumateTextBlock->SetText(TitleText);
}

const FText& UInstPropWidgetBase::GetMixedDisplayText()
{
	static const FText mixedDisplayText = LOCTEXT("MixedDisplayText", "Mixed Values");
	return mixedDisplayText;
}

void UInstPropWidgetBase::PostValueChanged()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto doc = controller->GetDocument();
	if (!ensure(doc))
	{
		return;
	}

	doc->BeginUndoRedoMacro();

	BroadcastValueChanged();
	DisplayValue();

	doc->EndUndoRedoMacro();
}

void UInstPropWidgetBase::OnRegisteredValue(UObject* Source, bool bValueEqualsCurrent)
{
	bConsistentValue = (++NumRegistrations == 1) || bValueEqualsCurrent;
}

#undef LOCTEXT_NAMESPACE
