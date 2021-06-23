// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockProperties.h"

#include "Components/VerticalBox.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/Properties/InstPropWidgetBase.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"


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
	// Start off as collapsed, until we have more than 0 enabled properties
	SetVisibility(ESlateVisibility::Collapsed);

	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (!ensure(controller && controller->EMPlayerState && CurrentTool))
	{
		return;
	}

	// Clear out all of the previous instance properties, so that they can be re-registered
	int32 maxNumRegistrations = 0;
	for (auto propertyEntry : PropertiesListBox->GetAllChildren())
	{
		if (auto instPropEntry = Cast<UInstPropWidgetBase>(propertyEntry))
		{
			instPropEntry->ResetInstProp();
			instPropEntry->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Link property block to current tool mode
	CurrentTool->RegisterToolDataUI(this, maxNumRegistrations);

	// For all instance property members, set their visibility to whether their value has been consistently registered
	bool bAnyPropertiesEnabled = false;
	if (maxNumRegistrations > 0)
	{
		for (auto propertyEntry : PropertiesListBox->GetAllChildren())
		{
			if (auto instPropEntry = Cast<UInstPropWidgetBase>(propertyEntry))
			{
				bool bVisible = instPropEntry->GetNumRegistrations() == maxNumRegistrations;
				instPropEntry->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
				if (bVisible)
				{
					instPropEntry->DisplayValue();
					bAnyPropertiesEnabled = true;
				}
			}
		}
	}


	if (bAnyPropertiesEnabled)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

UInstPropWidgetBase* UToolTrayBlockProperties::RequestPropertyField(UObject* PropertySource, const FString& PropertyName, UClass* FieldClass)
{
	if (!ensure(FieldClass))
	{
		return nullptr;
	}

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
	if (ensure(propertyField && propertyField->IsInstPropInitialized()))
	{
		RequestedPropertyFields.Add(PropertyName, propertyField);
		PropertiesListBox->AddChildToVerticalBox(propertyField);
		propertyField->SetPropertyTitle(FText::FromString(PropertyName));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to initialize Instance Property widget of class %s; ")
			TEXT("is it a valid native child of UInstPropWidgetBase, or did you add its Blueprint child class to UToolTrayBlockProperties.InstanceProperyBlueprints?"),
			*FieldClass->GetName());
	}

	return propertyField;
}
