// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockProperties.h"

#include "Components/VerticalBox.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/Properties/InstPropWidgetBase.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

// TODO: remove after generalized tool API
#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"

UToolTrayBlockProperties::UToolTrayBlockProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayBlockProperties::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	for (TSubclassOf<UInstPropWidgetBase> instPropBP : InstanceProperyBlueprints)
	{
		UClass* instPropBPClass = instPropBP.Get();
		UClass* instPropNativeClass = instPropBPClass ? instPropBP->GetSuperClass() : nullptr;
		if (instPropNativeClass && (instPropNativeClass->GetSuperClass() == UInstPropWidgetBase::StaticClass()))
		{
			InstPropClassesFromNativeClasses.Add(instPropNativeClass, instPropBPClass);
		}
	}

	if (!ensure(PropertiesListBox && (InstPropClassesFromNativeClasses.Num() > 0) && (InstPropClassesFromNativeClasses.Num() == InstanceProperyBlueprints.Num())))
	{
		return false;
	}

	return true;
}

void UToolTrayBlockProperties::ChangeBlockProperties(UEditModelToolBase* CurrentTool)
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (!ensure(controller && controller->EMPlayerState && CurrentTool))
	{
		return;
	}

	// Clear out all of the previous instance properties, so that they can be re-registered
	auto propertyEntries = PropertiesListBox->GetAllChildren();
	int32 maxNumRegistrations = 0;
	for (auto propertyEntry : propertyEntries)
	{
		if (auto instPropEntry = Cast<UInstPropWidgetBase>(propertyEntry))
		{
			instPropEntry->ResetInstProp();
			instPropEntry->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// TODO: generalize the UEditModelToolBase API
	if (auto selectTool = Cast<USelectTool>(CurrentTool))
	{
		maxNumRegistrations = controller->EMPlayerState->SelectedObjects.Num();
		for (auto selectedMOI : controller->EMPlayerState->SelectedObjects)
		{
			if (selectedMOI)
			{
				selectedMOI->RegisterInstanceDataUI(this);
			}
		}
	}

	// For all instance property members, set their visibility to whether their value has been consistently registered
	bool bAnyRegistrations = maxNumRegistrations > 0;
	propertyEntries = PropertiesListBox->GetAllChildren();
	for (auto propertyEntry : propertyEntries)
	{
		if (auto instPropEntry = Cast<UInstPropWidgetBase>(propertyEntry))
		{
			bool bVisible = bAnyRegistrations && (instPropEntry->GetNumRegistrations() == maxNumRegistrations);
			instPropEntry->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			if (bVisible)
			{
				instPropEntry->DisplayValue();
			}
		}
	}
}

UInstPropWidgetBase* UToolTrayBlockProperties::RequestPropertyField(UObject* PropertySource, const FString& PropertyName, UClass* FieldClass)
{
	if (InstPropClassesFromNativeClasses.Contains(FieldClass))
	{
		FieldClass = InstPropClassesFromNativeClasses[FieldClass];
	}

	UInstPropWidgetBase* propertyField = RequestedPropertyFields.FindRef(PropertyName);
	if (propertyField)
	{
		if (ensure(propertyField->GetClass() == FieldClass))
		{
			return propertyField;
		}
		else
		{
			return nullptr;
		}
	}

	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	propertyField = controller ? controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UInstPropWidgetBase>(FieldClass) : nullptr;
	if (ensure(propertyField))
	{
		RequestedPropertyFields.Add(PropertyName, propertyField);
		PropertiesListBox->AddChildToVerticalBox(propertyField);
		propertyField->SetPropertyTitle(FText::FromString(PropertyName));
	}

	return propertyField;
}
