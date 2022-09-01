// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditorForm.h"

#define LOCTEXT_NAMESPACE "BIMPresetEditorForm"


bool FBIMPresetForm::operator==(const FBIMPresetForm& RHS) const
{
	if (Elements.Num() != RHS.Elements.Num())
	{
		return false;
	}

	for (int32 i = 0; i < Elements.Num(); ++i)
	{
		const auto& elem1 = Elements[i];
		const auto& elem2 = RHS.Elements[i];
		if (!elem1.DisplayName.EqualTo(elem2.DisplayName))
		{
			return false;
		}

		if (elem1.FieldType != elem2.FieldType)
		{
			return false;
		}

		if (elem1.FieldName != elem2.FieldName)
		{
			return false;
		}

		if (elem1.StringRepresentation != elem2.StringRepresentation)
		{
			return false;
		}
	}

	return true;
}

bool FBIMPresetForm::operator!=(const FBIMPresetForm& RHS) const
{
	return !(*this == RHS);
}

bool FBIMPresetForm::HasField(const FName& Field) const
{
	for (auto& element : Elements)
	{
		if (Field == FName(element.FieldName))
		{
			return true;
		}
	}
	return false;
}

EBIMResult FBIMPresetForm::AddPropertyElement(const FText& DisplayName, const FName& FieldName, EBIMPresetEditorField FieldType)
{
	if (ensureAlways(!DisplayName.IsEmpty() && !FieldName.IsNone()))
	{
		FBIMPresetFormElement& elem = Elements.AddDefaulted_GetRef();
		elem.DisplayName = DisplayName;
		elem.FieldType = FieldType;
		elem.FieldName = FieldName.ToString();

		switch (FieldType)
		{
		case EBIMPresetEditorField::TextProperty:
		case EBIMPresetEditorField::NumberProperty:
		case EBIMPresetEditorField::DimensionProperty:
			elem.FormElementWidgetType = EBIMFormElementWidget::TextEntry;
			break;

		case EBIMPresetEditorField::AssetProperty:
			elem.FormElementWidgetType = EBIMFormElementWidget::GUIDSwap;
			break;

		default: ensureAlways(false); return EBIMResult::Error;
		};

		return EBIMResult::Success;
	}

	return EBIMResult::Error;
}

FBIMPresetEditorDelta FBIMPresetEditorDelta::Inverted() const
{
	FBIMPresetEditorDelta ret = *this;
	ret.NewStringRepresentation = OldStringRepresentation;
	ret.OldStringRepresentation = NewStringRepresentation;
	return ret;
}

EBIMResult FBIMPresetForm::AddMaterialBindingElement(const FText& DisplayName, const FName& ChannelName, EMaterialChannelFields MaterialSubField)
{
	if (ensureAlways(!DisplayName.IsEmpty() && !ChannelName.IsNone()))
	{
		FBIMPresetFormElement& newElement = Elements.AddDefaulted_GetRef();
		newElement.DisplayName = DisplayName;
		newElement.FieldType = EBIMPresetEditorField::MaterialBinding;
		newElement.FieldName = ChannelName.ToString();
		newElement.MaterialChannelSubField = MaterialSubField;

		switch (MaterialSubField)
		{
		case EMaterialChannelFields::InnerMaterial:
		case EMaterialChannelFields::SurfaceMaterial:
			newElement.FormElementWidgetType = EBIMFormElementWidget::GUIDSwap;
			break;

		case EMaterialChannelFields::ColorTint:
			newElement.FormElementWidgetType = EBIMFormElementWidget::ColorPicker;
			break;

		case EMaterialChannelFields::ColorTintVariation:
			newElement.FormElementWidgetType = EBIMFormElementWidget::TextEntry;
			break;

		default: ensureAlways(false); return EBIMResult::Error;
		}

		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetForm::AddLayerPriorityGroupElement()
{
	FBIMPresetFormElement& newElement = Elements.AddDefaulted_GetRef();
	newElement.DisplayName = LOCTEXT("MiterGroup","Miter Group");
	newElement.FieldType = EBIMPresetEditorField::LayerPriorityGroup;
	newElement.FieldName = TEXT("MiterGroup");
	newElement.FormElementWidgetType = EBIMFormElementWidget::EnumSelect;

	UEnum* enumClass = StaticEnum<EBIMPresetLayerPriorityGroup>();
	newElement.EnumClassName = enumClass->CppType;

	return EBIMResult::Success;
}

EBIMResult FBIMPresetForm::AddLayerPriorityValueElement()
{
	FBIMPresetFormElement& newElement = Elements.AddDefaulted_GetRef();
	newElement.DisplayName = LOCTEXT("MiterPriority","Miter Priority");
	newElement.FieldType = EBIMPresetEditorField::LayerPriorityValue;
	newElement.FieldName = TEXT("MiterPriority");
	newElement.FormElementWidgetType = EBIMFormElementWidget::TextEntry;
 	return EBIMResult::Success;
}

EBIMResult FBIMPresetForm::AddConstructionCostElements()
{
	FBIMPresetFormElement& materialElement = Elements.AddDefaulted_GetRef();
	materialElement.DisplayName = LOCTEXT("ConstructionCostMaterial", "Material Cost");
	materialElement.FieldType = EBIMPresetEditorField::ConstructionCostMaterial;
	materialElement.FieldName = TEXT("MaterialCost");
	materialElement.FormElementWidgetType = EBIMFormElementWidget::TextEntry;

	FBIMPresetFormElement& laborElement = Elements.AddDefaulted_GetRef();
	laborElement.DisplayName = LOCTEXT("ConstructionCostLabor", "Labor Cost");
	laborElement.FieldType = EBIMPresetEditorField::ConstructionCostLabor;
	laborElement.FieldName = TEXT("LaborCost");
	laborElement.FormElementWidgetType = EBIMFormElementWidget::TextEntry;

	return EBIMResult::Success;
}

EBIMResult FBIMPresetForm::AddLightConfigElements()
{
	FBIMPresetFormElement& elem = Elements.AddDefaulted_GetRef();
	elem.DisplayName = LOCTEXT("LightDisplayColor", "Color");
	elem.FieldName = TEXT("Color");
	elem.FieldType = EBIMPresetEditorField::LightColor;
	elem.FormElementWidgetType = EBIMFormElementWidget::ColorPicker;

	FBIMPresetFormElement& intensity = Elements.AddDefaulted_GetRef();
	intensity.DisplayName = LOCTEXT("LightIntensity", "Intensity");
	intensity.FieldName = TEXT("Intensity");
	intensity.FieldType = EBIMPresetEditorField::LightIntensity;
	intensity.FormElementWidgetType = EBIMFormElementWidget::TextEntry;

	FBIMPresetFormElement& radius = Elements.AddDefaulted_GetRef();
	radius.DisplayName = LOCTEXT("LightRadius", "Radius");
	radius.FieldName = TEXT("Radius");
	radius.FieldType = EBIMPresetEditorField::LightRadius;
	radius.FormElementWidgetType = EBIMFormElementWidget::TextEntry;

	AddPropertyElement(LOCTEXT("IESProfile","IES Profile"), TEXT("IESProfile.AssetID"), EBIMPresetEditorField::AssetProperty);

	return EBIMResult::Success;
}


EBIMResult FBIMWebPresetForm::AddPropertyElement(const FString& DisplayName, const FString& FieldName)
{
	if (ensureAlways(!DisplayName.IsEmpty() && !FieldName.IsEmpty()))
	{
		FBIMWebPresetFormElement& elem = Elements.AddDefaulted_GetRef();
		elem.DisplayName = DisplayName;
		elem.FieldName = FieldName;
		
		return EBIMResult::Success;
	}

	return EBIMResult::Error;
}


#undef LOCTEXT_NAMESPACE