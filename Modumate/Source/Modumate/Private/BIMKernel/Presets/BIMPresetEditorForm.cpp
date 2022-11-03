// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditorForm.h"

#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetInstanceFactory.h"
#include "ModumateCore/EnumHelpers.h"

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

EBIMResult FBIMPresetForm::AddPropertyElement(const FText& DisplayName, const FName& MatrixName, const FName& FieldName, EBIMPresetEditorField FieldType)
{
	if (ensureAlways(!DisplayName.IsEmpty() && !FieldName.IsNone()))
	{
		EPresetPropertyMatrixNames matrixNameEnum;
		ensure(FindEnumValueByName(MatrixName, matrixNameEnum));
		
		UStruct* CData;
		if (!FBIMPresetInstanceFactory::GetCustomDataStaticStruct(matrixNameEnum, CData))
		{
			return EBIMResult::Error;
		}
		

		FBIMPresetFormElement& elem = Elements.AddDefaulted_GetRef();
		elem.DisplayName = DisplayName;
		elem.FieldType = FieldType;
		elem.FieldName = FieldName.ToString();
		elem.CustomDataStructName = CData->GetName();

		switch (FieldType)
		{
		case EBIMPresetEditorField::TextProperty:
		case EBIMPresetEditorField::NumberProperty:
		case EBIMPresetEditorField::IntegerProperty:
		case EBIMPresetEditorField::NameProperty:
		case EBIMPresetEditorField::DimensionProperty:
			elem.FormElementWidgetType = EBIMFormElementWidget::TextEntry;
			break;

		case EBIMPresetEditorField::AssetProperty:
			elem.FormElementWidgetType = EBIMFormElementWidget::GUIDSwap;
			break;
		case EBIMPresetEditorField::ColorProperty:
			elem.FormElementWidgetType = EBIMFormElementWidget::ColorPicker;
			break;
		case EBIMPresetEditorField::DropdownProperty:
		{
			elem.FormElementWidgetType = EBIMFormElementWidget::EnumSelect;

			// TODO: Currently it is hardcoded that a Dropdown property is for layer priority, this needs to dynamically
			// find the enum class name based on the type in custom data struct
			UEnum* enumClass = StaticEnum<EBIMPresetLayerPriorityGroup>();
			elem.EnumClassName = enumClass->CppType;
			break;	
		}
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

#undef LOCTEXT_NAMESPACE