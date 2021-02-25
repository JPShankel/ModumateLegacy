// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateCommands.h"

bool FBIMPresetPinAttachment::operator==(const FBIMPresetPinAttachment &OtherAttachment) const
{
	return ParentPinSetIndex == OtherAttachment.ParentPinSetIndex &&
		ParentPinSetPosition == OtherAttachment.ParentPinSetPosition &&
		PresetGUID == OtherAttachment.PresetGUID;
}

bool FBIMPresetInstance::Matches(const FBIMPresetInstance &OtherPreset) const
{
	if (NodeType != OtherPreset.NodeType)
	{
		return false;
	}

	if (GUID != OtherPreset.GUID)
	{
		return false;
	}

	if (PartSlots.Num() != OtherPreset.PartSlots.Num())
	{
		return false;
	}

	if (ChildPresets.Num() != OtherPreset.ChildPresets.Num())
	{
		return false;
	}

	if (ParentTagPaths.Num() != OtherPreset.ParentTagPaths.Num())
	{
		return false;
	}

	if (SlotConfigPresetGUID != OtherPreset.SlotConfigPresetGUID)
	{
		return false;
	}

	if (MyTagPath != OtherPreset.MyTagPath)
	{
		return false;
	}

	if (!Properties.Matches(OtherPreset.Properties))
	{
		return false;
	}

	for (int32 i=0;i<PartSlots.Num();++i)
	{
		if (OtherPreset.PartSlots[i] != PartSlots[i])
		{
			return false;
		}
	}

	for (auto& cp : ChildPresets)
	{
		if (!OtherPreset.ChildPresets.Contains(cp))
		{
			return false;
		}
	}

	for (auto &ptp : ParentTagPaths)
	{
		if (!OtherPreset.ParentTagPaths.Contains(ptp))
		{
			return false;
		}
	}

	if (MaterialChannelBindings != OtherPreset.MaterialChannelBindings)
	{
		return false;
	}

	return true;
}

EBIMResult FBIMPresetInstance::SortChildPresets()
{
	ChildPresets.Sort([](const FBIMPresetPinAttachment& LHS, const FBIMPresetPinAttachment& RHS)
	{
		if (LHS.ParentPinSetIndex < RHS.ParentPinSetIndex)
		{
			return true;
		}
		if (LHS.ParentPinSetIndex > RHS.ParentPinSetIndex)
		{
			return false;
		}
		return LHS.ParentPinSetPosition < RHS.ParentPinSetPosition;
	});
	return EBIMResult::Success;
}

bool FBIMPresetInstance::HasProperty(const FBIMNameType& Name) const
{
	return Properties.HasProperty<float>(NodeScope, Name) || Properties.HasProperty<FString>(NodeScope, Name);
}

void FBIMPresetInstance::SetProperties(const FBIMPropertySheet& InProperties)
{
	Properties = InProperties;
}

bool FBIMPresetInstance::HasPin(int32 PinSetIndex, int32 PinSetPosition) const
{
	for (auto& child : ChildPresets)
	{
		if (child.ParentPinSetIndex == PinSetIndex && child.ParentPinSetPosition == PinSetPosition)
		{
			return true;
		}
	}
	return false;
}

// Used by the BIM designer to determine whether to off an add button
bool FBIMPresetInstance::HasOpenPin() const
{
	// By convention, only the last pin will have additions
	return (TypeDefinition.PinSets.Num() > 0 && TypeDefinition.PinSets.Last().MaxCount == INDEX_NONE);
}

EBIMResult FBIMPresetInstance::AddChildPreset(const FGuid& ChildPresetID, int32 PinSetIndex, int32 PinSetPosition)
{
	EBIMPinTarget target = EBIMPinTarget::Default;
	for (auto& child : ChildPresets)
	{
		if (child.ParentPinSetIndex == PinSetIndex && child.ParentPinSetPosition >= PinSetPosition)
		{
			++child.ParentPinSetPosition;
			target = child.Target;
		}
	}

	FBIMPresetPinAttachment& newAttachment = ChildPresets.AddDefaulted_GetRef();
	newAttachment.ParentPinSetIndex = PinSetIndex;
	newAttachment.ParentPinSetPosition = PinSetPosition;
	newAttachment.PresetGUID = ChildPresetID;
	newAttachment.Target = target;

	SortChildPresets();

	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::RemoveChildPreset(int32 PinSetIndex, int32 PinSetPosition)
{
	int32 indexToRemove = INDEX_NONE;
	for (int32 i = 0; i < ChildPresets.Num(); ++i)
	{
		if (ChildPresets[i].ParentPinSetIndex == PinSetIndex)
		{
			if (ChildPresets[i].ParentPinSetPosition == PinSetPosition)
			{
				indexToRemove = i;
			}
			else if (ChildPresets[i].ParentPinSetPosition > PinSetPosition)
			{
				--ChildPresets[i].ParentPinSetPosition;
			}
		}
	}
	if (ensureAlways(indexToRemove != INDEX_NONE))
	{
		ChildPresets.RemoveAt(indexToRemove);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::SetPartPreset(const FGuid& SlotPreset, const FGuid& PartPreset)
{
	for (auto& partSlot : PartSlots)
	{
		if (partSlot.SlotPresetGUID == SlotPreset)
		{
			partSlot.PartPresetGUID = PartPreset;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

bool FBIMPresetInstance::ValidatePreset() const
{
	if (ChildPresets.Num() > 0)
	{
		if (ChildPresets[0].ParentPinSetPosition != 0)
		{
			return false;
		}
		for (int32 i = 1; i < ChildPresets.Num(); ++i)
		{
			const FBIMPresetPinAttachment& previous = ChildPresets[i-1];
			const FBIMPresetPinAttachment& current = ChildPresets[i];
			bool isSibling = (current.ParentPinSetIndex == previous.ParentPinSetIndex && current.ParentPinSetPosition == previous.ParentPinSetPosition + 1);
			bool isCousin = (current.ParentPinSetIndex > previous.ParentPinSetIndex && current.ParentPinSetPosition == 0);
			if (!isSibling && !isCousin)
			{
				return false;
			}
		}
	}
	return true;
}


EBIMResult FBIMPresetInstance::ApplyDelta(const FBIMPresetEditorDelta& Delta)
{
	if (Delta.FieldType == EBIMPresetEditorField::MaterialBinding)
	{
		for (auto& binding : MaterialChannelBindings)
		{
			if (binding.Channel.IsEqual(Delta.FieldName))
			{
				switch (Delta.MaterialChannelSubField)
				{
				case EMaterialChannelFields::InnerMaterial:
				{
					FGuid::Parse(Delta.NewStringRepresentation,binding.InnerMaterialGUID);
					return EBIMResult::Success;
				}
				break;

				case EMaterialChannelFields::SurfaceMaterial:
				{
					FGuid::Parse(Delta.NewStringRepresentation, binding.SurfaceMaterialGUID);
					Properties.SetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, Delta.NewStringRepresentation);
					return EBIMResult::Success;
				}
				break;

				case EMaterialChannelFields::ColorTint:
				{
					binding.ColorHexValue = Delta.NewStringRepresentation;
					Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, binding.ColorHexValue);
					return EBIMResult::Success;
				}
				break;

				case EMaterialChannelFields::ColorTintVariation:
				{
					binding.ColorTintVariationHexValue = Delta.NewStringRepresentation;
					return EBIMResult::Success;
				}
				break;

				default: return EBIMResult::Error;
				};
			}
		}
		return EBIMResult::Error;
	}

	switch (Delta.FieldType)
	{
		case EBIMPresetEditorField::AssetProperty:
		case EBIMPresetEditorField::TextProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			Properties.SetProperty(propKey.Scope, propKey.Name, Delta.NewStringRepresentation);
		}
		break;

		case EBIMPresetEditorField::NumberProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			Properties.SetProperty(propKey.Scope, propKey.Name, FCString::Atof(*Delta.NewStringRepresentation));
		}
		break;

		case EBIMPresetEditorField::DimensionProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			Properties.SetProperty(propKey.Scope, propKey.Name, FCString::Atof(*Delta.NewStringRepresentation));
		}
		break;
	};

	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::MakeDeltaForFormElement(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const
{
	OutDelta.FieldType = FormElement.FieldType;
	OutDelta.NewStringRepresentation = FormElement.StringRepresentation;
	OutDelta.FieldName = *FormElement.FieldName;
	OutDelta.MaterialChannelSubField = FormElement.MaterialChannelSubField;

	if (FormElement.FieldType == EBIMPresetEditorField::MaterialBinding)
	{
		for (auto& binding : MaterialChannelBindings)
		{
			if (binding.Channel.IsEqual(*FormElement.FieldName))
			{
				switch (FormElement.MaterialChannelSubField)
				{
					case EMaterialChannelFields::InnerMaterial:
					{
						OutDelta.OldStringRepresentation = binding.InnerMaterialGUID.ToString();
						return EBIMResult::Success;
					}
					break;

					case EMaterialChannelFields::SurfaceMaterial:
					{
						OutDelta.OldStringRepresentation = binding.SurfaceMaterialGUID.ToString();
						return EBIMResult::Success;
					}
					break;

					case EMaterialChannelFields::ColorTint:
					{
						OutDelta.OldStringRepresentation = binding.ColorHexValue;
						return EBIMResult::Success;
					}
					break;

					case EMaterialChannelFields::ColorTintVariation:
					{
						OutDelta.OldStringRepresentation = binding.ColorTintVariationHexValue;
						return EBIMResult::Success;
					}
					break;

					default: return EBIMResult::Error;
				};
			}
		}
		return EBIMResult::Error;
	}

	switch (FormElement.FieldType)
	{
		case EBIMPresetEditorField::DimensionProperty:
		{
			float v;
			FBIMPropertyKey propKey(*FormElement.FieldName);
			if (ensureAlways(Properties.TryGetProperty(propKey.Scope, propKey.Name, v)))
			{
				TArray<int32> imperialsInches;
				UModumateDimensionStatics::CentimetersToImperialInches(v, imperialsInches);
				OutDelta.OldStringRepresentation = UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches).ToString();
			}
		}
		break;
		case EBIMPresetEditorField::NumberProperty:
		{
			float v;
			FBIMPropertyKey propKey(*FormElement.FieldName);
			if (Properties.TryGetProperty(propKey.Scope, propKey.Name, v))
			{
				OutDelta.OldStringRepresentation = FString::Printf(TEXT("%f"), v);
			}
		}
		break;
		case EBIMPresetEditorField::TextProperty:
		case EBIMPresetEditorField::AssetProperty:
		{
			FBIMPropertyKey propKey(*FormElement.FieldName);
			ensureAlways(Properties.TryGetProperty(propKey.Scope, propKey.Name, OutDelta.OldStringRepresentation));
		}
		break;
		default: ensureAlways(false); return EBIMResult::Error;
	};

	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::GetForm(FBIMPresetForm& OutForm) const
{
	OutForm = TypeDefinition.FormTemplate;
	OutForm.Elements.Append(PresetForm.Elements);

	for (auto& element : OutForm.Elements)
	{
		if (element.FieldType == EBIMPresetEditorField::MaterialBinding)
		{
			for (auto& binding : MaterialChannelBindings)
			{
				if (binding.Channel.IsEqual(*element.FieldName))
				{
					switch (element.MaterialChannelSubField)
					{
					case EMaterialChannelFields::InnerMaterial:
						element.StringRepresentation = binding.InnerMaterialGUID.ToString();
						break;

					case EMaterialChannelFields::SurfaceMaterial:
						element.StringRepresentation = binding.SurfaceMaterialGUID.ToString();
						break;

					case EMaterialChannelFields::ColorTint:
						element.StringRepresentation = binding.ColorHexValue;
						break;

					case EMaterialChannelFields::ColorTintVariation:
						element.StringRepresentation = binding.ColorTintVariationHexValue;
						break;

					default: ensureAlways(false);
					};
				}
			}
			continue;
		}

		switch (element.FieldType)
		{
			case EBIMPresetEditorField::DimensionProperty:
			{
				float v;
				FBIMPropertyKey propKey(*element.FieldName);
				if (ensureAlways(Properties.TryGetProperty<float>(propKey.Scope, propKey.Name, v)))
				{
					element.StringRepresentation = FString::Printf(TEXT("%f"), v);
					TArray<int32> imperialsInches;
					UModumateDimensionStatics::CentimetersToImperialInches(v, imperialsInches);
					element.StringRepresentation = UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches).ToString();
				}
			}
			break;

			case EBIMPresetEditorField::NumberProperty:
			{
				float v;
				FBIMPropertyKey propKey(*element.FieldName);
				if (ensureAlways(Properties.TryGetProperty<float>(propKey.Scope, propKey.Name, v)))
				{
					element.StringRepresentation = FString::Printf(TEXT("%f"), v);
				}
			}
			break;

			case EBIMPresetEditorField::TextProperty:
			case EBIMPresetEditorField::AssetProperty:
			default:
			{
				FBIMPropertyKey propKey(*element.FieldName);
				ensureAlways(Properties.TryGetProperty<FString>(propKey.Scope, propKey.Name, element.StringRepresentation));
			}
			break;
		};
	}

	return EBIMResult::Success;
}


