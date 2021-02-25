// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditorForm.h"


bool FBIMPresetForm::Matches(const FBIMPresetForm& OtherForm) const
{
	if (Elements.Num() != OtherForm.Elements.Num())
	{
		return false;
	}

	for (int32 i = 0; i < Elements.Num(); ++i)
	{
		const auto& elem1 = Elements[i];
		const auto& elem2 = OtherForm.Elements[i];
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
		case EMaterialChannelFields::ColorTintVariation:
			newElement.FormElementWidgetType = EBIMFormElementWidget::ColorPicker;
			break;

		default: ensureAlways(false); return EBIMResult::Error;
		}

		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}