// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"

#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "BIMKernel/Presets/BIMPresetInstanceFactory.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMIESProfile.h"
#include "BIMKernel/Presets/CustomData/BIMMesh.h"
#include "BIMKernel/Presets/CustomData/BIMNamedDimension.h"
#include "BIMKernel/Presets/CustomData/BIMPart.h"
#include "BIMKernel/Presets/CustomData/BIMPattern.h"
#include "BIMKernel/Presets/CustomData/BIMPatternRef.h"
#include "BIMKernel/Presets/CustomData/BIMPresetInfo.h"
#include "BIMKernel/Presets/CustomData/BIMProfile.h"
#include "BIMKernel/Presets/CustomData/BIMRawMaterial.h"
#include "BIMKernel/Presets/CustomData/BIMSlot.h"
#include "BIMKernel/Presets/CustomData/BIMSlotConfig.h"

#include "ModumateCore/EnumHelpers.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Quantities/QuantitiesDimensions.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "BIMPresetInstance"

bool FBIMPresetPinAttachment::operator==(const FBIMPresetPinAttachment &OtherAttachment) const
{
	return ParentPinSetIndex == OtherAttachment.ParentPinSetIndex &&
		ParentPinSetPosition == OtherAttachment.ParentPinSetPosition &&
		PresetGUID == OtherAttachment.PresetGUID;
}

bool FBIMPresetInstance::operator==(const FBIMPresetInstance &RHS) const
{

	if (GUID != RHS.GUID)
	{
		return false;
	}

	if (PartSlots.Num() != RHS.PartSlots.Num())
	{
		return false;
	}

	if (ChildPresets.Num() != RHS.ChildPresets.Num())
	{
		return false;
	}

	if (SlotConfigPresetGUID != RHS.SlotConfigPresetGUID)
	{
		return false;
	}

	if (MyTagPath != RHS.MyTagPath)
	{
		return false;
	}

	for (int32 i=0;i<PartSlots.Num();++i)
	{
		if (RHS.PartSlots[i] != PartSlots[i])
		{
			return false;
		}
	}

	for (auto& cp : ChildPresets)
	{
		if (!RHS.ChildPresets.Contains(cp))
		{
			return false;
		}
	}

	if (!CustomDataByClassName.OrderIndependentCompareEqual(RHS.CustomDataByClassName))
	{
		return false;
	}

	return true;
}

bool FBIMPresetInstance::operator!=(const FBIMPresetInstance& RHS) const
{
	return !(*this == RHS);
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
	return (PinSets.Num() > 0 && PinSets.Last().MaxCount == INDEX_NONE);
}

EBIMResult FBIMPresetInstance::AddChildPreset(const FGuid& ChildPresetID, int32 PinSetIndex, int32 PinSetPosition)
{
	EBIMPinTarget target = PinSets[PinSetIndex].PinTarget;
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

/*
 * Replace a Child Guid reference with a different one. 
 * Note: this function can only be called before major processing steps occur, such as Materials, Meshes, etc..
 */
bool FBIMPresetInstance::ReplaceImmediateChildGuid(FGuid& OldGuid, FGuid& NewGuid)
{
	bool rtn = false;
	if (!ensure(OldGuid.IsValid())|| !ensure(NewGuid.IsValid()))
	{
		return rtn;
	}
	
	for (auto &childNode : ChildPresets)
	{
		if (childNode.PresetGUID == OldGuid)
		{
			childNode.PresetGUID = NewGuid;
			rtn = true;
		}
	}

	if (SlotConfigPresetGUID == OldGuid)
	{
		SlotConfigPresetGUID = NewGuid;
		rtn = true;
	}

	for (auto& part : PartSlots)
	{
		if (part.PartPresetGUID == OldGuid)
		{
			part.PartPresetGUID = NewGuid;
			rtn = true;
		}

		if (part.SlotPresetGUID == OldGuid)
		{
			part.SlotPresetGUID = NewGuid;
			rtn = true;
		}
	}
	
	//Cannot modify collection while iterating
	FBIMPropertySheet newProps = Properties_DEPRECATED;
	Properties_DEPRECATED.ForEachProperty([&](const FBIMPropertyKey& PropKey,const FString& Value) {
		FGuid guid;
		if (FGuid::Parse(Value, guid) && guid.IsValid() && guid == OldGuid)
		{
			rtn = true;
			newProps.SetProperty(PropKey.Scope, PropKey.Name, NewGuid.ToString());
		}
	});
	Properties_DEPRECATED = newProps;

	FBIMPresetMaterialBindingSet materialBindingSet;
	if (TryGetCustomData(materialBindingSet))
	{
		for (auto& binding : materialBindingSet.MaterialBindings)
		{
			if (binding.SurfaceMaterialGUID == OldGuid)
			{
				binding.SurfaceMaterialGUID = NewGuid;
				SetCustomData(materialBindingSet);
				rtn = true;
			}
			if (binding.InnerMaterialGUID == OldGuid)
			{
				binding.InnerMaterialGUID = NewGuid;
				SetCustomData(materialBindingSet);
				rtn = true;
			}
		}
	}

	FLightConfiguration lightConfig;
	if (TryGetCustomData(lightConfig))
	{
		if (lightConfig.IESProfileGUID == OldGuid)
		{
			lightConfig.IESProfileGUID = NewGuid;
			SetCustomData(lightConfig);
			rtn = true;
		}
	}
	
	return rtn;
}

bool FBIMPresetInstance::CheckChildrenForErrors() const
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

bool FBIMPresetInstance::EnsurePresetIsValidForUse() const
{
	if (!ensure(GUID.IsValid()))
	{
		return false;
	}
	
	if (!ensure(Origination != EPresetOrigination::Unknown && Origination != EPresetOrigination::Canonical))
	{
		return false;
	}

	if (!ensure(Origination == EPresetOrigination::Invented || CanonicalBase.IsValid()))
	{
		return false;
	}

	return true;
}

EBIMResult FBIMPresetInstance::HandleMaterialBindingDelta(const FBIMPresetEditorDelta& Delta)
{
	FBIMPresetMaterialBindingSet bindingSet;
	if (!TryGetCustomData(bindingSet))
	{
		return EBIMResult::Error;
	}
	for (auto& binding : bindingSet.MaterialBindings)
	{
		if (binding.Channel.IsEqual(FName(Delta.FieldName)))
		{
			switch (Delta.MaterialChannelSubField)
			{
			case EMaterialChannelFields::InnerMaterial:
			{
				FGuid::Parse(Delta.NewStringRepresentation, binding.InnerMaterialGUID);
				SetCustomData(bindingSet);
				return EBIMResult::Success;
			}
			break;

			case EMaterialChannelFields::SurfaceMaterial:
			{
				FGuid::Parse(Delta.NewStringRepresentation, binding.SurfaceMaterialGUID);
				SetCustomData(bindingSet);
				return EBIMResult::Success;
			}
			break;

			case EMaterialChannelFields::ColorTint:
			{
				binding.ColorHexValue = Delta.NewStringRepresentation;
				SetCustomData(bindingSet);
				return EBIMResult::Success;
			}
			break;

			case EMaterialChannelFields::ColorTintVariation:
			{
				LexTryParseString(binding.ColorTintVariationPercent, *Delta.NewStringRepresentation);
				SetCustomData(bindingSet);
				return EBIMResult::Success;
			}
			break;

			default: return EBIMResult::Error;
			};
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::ApplyDelta(const UModumateDocument* InDocument,const FBIMPresetEditorDelta& Delta)
{
	switch (Delta.FieldType)
	{
		case EBIMPresetEditorField::MaterialBinding:
		{
			return HandleMaterialBindingDelta(Delta);
		}
	
		case EBIMPresetEditorField::NameProperty:
		{
			DisplayName = FText::FromString(Delta.NewStringRepresentation);
			return EBIMResult::Success;
		}

		case EBIMPresetEditorField::AssetProperty:
		{
			auto rtn = TrySetCustomDataString(Delta.CustomDataStructName, Delta.FieldName, Delta.NewStringRepresentation) ? EBIMResult::Success : EBIMResult::Error;	
			FGuid guid;
			if (ensure(FGuid::Parse(Delta.NewStringRepresentation, guid)))
			{
				FLightConfiguration lightConfig;
				if (TryGetCustomData(lightConfig))
				{
					lightConfig.IESProfileGUID = guid;
					if (!InDocument)
					{
						return EBIMResult::Error;
					}
					const FBIMPresetInstance* profilePreset = InDocument->GetPresetCollection().PresetFromGUID(lightConfig.IESProfileGUID);
					FBIMIESProfile profileConfig;
					if (!profilePreset->TryGetCustomData(profileConfig))
					{
						return EBIMResult::Error;
					}
					
					if (profileConfig.AssetPath.IsEmpty())
					{
						return EBIMResult::Error;
					}
					FSoftObjectPath referencePath = FString(TEXT("TextureLightProfile'")).Append(profileConfig.AssetPath).Append(TEXT("'"));
					TSharedPtr<FStreamableHandle> SyncStreamableHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(referencePath);
					if (SyncStreamableHandle)
					{
						UTextureLightProfile* IESProfile = Cast<UTextureLightProfile>(SyncStreamableHandle->GetLoadedAsset());
						if (IESProfile)
						{
							lightConfig.LightProfile = IESProfile;
						}
					}
					SetCustomData(lightConfig);
				}
			}
			return rtn;
		}
	
		case EBIMPresetEditorField::TextProperty:
		{
			if (TrySetCustomDataString(Delta.CustomDataStructName, Delta.FieldName, Delta.NewStringRepresentation))
			{
				return EBIMResult::Success;
			}
			
			return EBIMResult::Error;
		}
		case EBIMPresetEditorField::DropdownProperty:
			{
				TArray<TPair<FText, int64>> enumValues;
				GetEnumDisplayNamesAndValues(Delta.EnumClassName, enumValues);
				auto value = enumValues.FindByPredicate([Delta](TPair<FText, int64>& p)
				{
					return FString::Printf(TEXT("%d"), static_cast<int32>(p.Value)) == Delta.NewStringRepresentation;
				});
				if (value != nullptr)
				{
					ensureAlways(TrySetCustomDataString(*Delta.CustomDataStructName, *Delta.FieldName, value->Key.ToString()));
				}
			}
		break;
		case EBIMPresetEditorField::ColorProperty:
		{
			if (TrySetCustomDataColor(Delta.CustomDataStructName, Delta.FieldName, Delta.NewStringRepresentation))
			{
				return EBIMResult::Success;
			}
		
			return EBIMResult::Error;
		}
		case EBIMPresetEditorField::NumberProperty:
		{
			float v;
			if (LexTryParseString(v, *Delta.NewStringRepresentation))
			{
				if (TrySetCustomDataNumber(Delta.CustomDataStructName, Delta.FieldName, v))
				{
					return EBIMResult::Success;
				}
			}

			return EBIMResult::Error;
		}
		case EBIMPresetEditorField::IntegerProperty:
		{
			int64 newValue;

			if (LexTryParseString(newValue,*Delta.NewStringRepresentation))
			{
				if (TrySetCustomDataNumber(Delta.CustomDataStructName, Delta.FieldName, newValue))
				{
					return EBIMResult::Success;
				}
			}
			return EBIMResult::Error;
		}
		case EBIMPresetEditorField::DimensionProperty:
		{
			const auto& settings = InDocument->GetCurrentSettings();
			auto dimension = UModumateDimensionStatics::StringToFormattedDimension(Delta.NewStringRepresentation, settings.DimensionType, settings.DimensionUnit);
			if (dimension.Format != EDimensionFormat::Error)
			{
				FBIMDimensions dimensions;
				if (TryGetCustomData(dimensions))
				{
					dimensions.SetCustomDimension(FName(Delta.FieldName), dimension.Centimeters);
					SetCustomData(dimensions);
					return EBIMResult::Success;	
				}
			}
				
			return EBIMResult::Error;
		}
		default: return EBIMResult::Error;
	};

	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::MakeMaterialBindingDelta(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const
{
	FBIMPresetMaterialBindingSet bindingSet;
	if (!TryGetCustomData(bindingSet))
	{
		return EBIMResult::Error;
	}
	for (auto& binding : bindingSet.MaterialBindings)
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
				OutDelta.OldStringRepresentation = FString::Printf(TEXT("%0.2f"), binding.ColorTintVariationPercent);
				return EBIMResult::Success;
			}
			break;

			default: return EBIMResult::Error;
			};
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::MakeDeltaForFormElement(const FBIMPresetFormElement& FormElement, FBIMPresetEditorDelta& OutDelta) const
{
	OutDelta.FieldType = FormElement.FieldType;
	OutDelta.NewStringRepresentation = FormElement.StringRepresentation;
	OutDelta.FieldName = *FormElement.FieldName;
	OutDelta.EnumClassName = *FormElement.EnumClassName;
	OutDelta.CustomDataStructName = *FormElement.CustomDataStructName;
	OutDelta.MaterialChannelSubField = FormElement.MaterialChannelSubField;

	switch (FormElement.FieldType)
	{
		case EBIMPresetEditorField::MaterialBinding:
		{
			return MakeMaterialBindingDelta(FormElement, OutDelta);
		}
		case EBIMPresetEditorField::DimensionProperty:
		{
			FBIMDimensions dimensions;
			if (TryGetCustomData(dimensions) && dimensions.HasCustomDimension(FName(OutDelta.FieldName)))
			{
				float v;
				dimensions.TryGetDimension(FName(OutDelta.FieldName), v);
				OutDelta.OldStringRepresentation = UModumateDimensionStatics::CentimetersToDisplayText(v).ToString();
				return EBIMResult::Success;	
			}
		}
		break;
		case EBIMPresetEditorField::NumberProperty:
		{
			float v;
			if (TryGetCustomDataNumber(*FormElement.CustomDataStructName, *FormElement.FieldName, v))
			{
				OutDelta.OldStringRepresentation = FString::SanitizeFloat(v);
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::IntegerProperty:
		{
			float v;
			if (TryGetCustomDataNumber(*FormElement.CustomDataStructName, *FormElement.FieldName, v))
			{
				int64 integerValue = static_cast<int>(v);
				OutDelta.OldStringRepresentation = FString::FromInt(integerValue);
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::TextProperty:
		case EBIMPresetEditorField::AssetProperty:
		{
			if (ensureAlways(TryGetCustomDataString(*FormElement.CustomDataStructName, *FormElement.FieldName, OutDelta.OldStringRepresentation)))
			{
				return EBIMResult::Success;
			}
		}
		case EBIMPresetEditorField::DropdownProperty:
		{
			FString enumStringValue;
			if (ensureAlways(TryGetCustomDataString(*FormElement.CustomDataStructName, *FormElement.FieldName, enumStringValue)))
			{
				TArray<TPair<FText, int64>> enumValues;
				GetEnumDisplayNamesAndValues(OutDelta.EnumClassName, enumValues);
				auto value = enumValues.FindByPredicate([enumStringValue](TPair<FText, int64>& p) { return p.Key.ToString() == enumStringValue; });
				if (value != nullptr)
				{
					OutDelta.OldStringRepresentation = FString::FromInt(value->Value);
					return EBIMResult::Success;
				}
			}
		}
		break;
		case EBIMPresetEditorField::ColorProperty:
		{
			if (ensureAlways(TryGetCustomDataColor(*FormElement.CustomDataStructName, *FormElement.FieldName, OutDelta.OldStringRepresentation)))
			{
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::NameProperty:
		{
			OutDelta.OldStringRepresentation = DisplayName.ToString();
			return EBIMResult::Success;
		}
		default: ensureAlways(false); return EBIMResult::Error;
	};

	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::UpdateFormElements(const UModumateDocument* InDocument,FBIMPresetForm& OutForm) const
{
	for (auto& element : OutForm.Elements)
	{
		if (element.FieldType == EBIMPresetEditorField::MaterialBinding)
		{
			FBIMPresetMaterialBindingSet bindingSet;
			if (!TryGetCustomData(bindingSet))
			{
				return EBIMResult::Error;
			}
			for (auto& binding : bindingSet.MaterialBindings)
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
						element.StringRepresentation = FString::Printf(TEXT("%0.2f"),binding.ColorTintVariationPercent);
						break;

					default: ensureAlways(false);
					};
				}
			}
			continue;
		}

		switch (element.FieldType)
		{
		case EBIMPresetEditorField::NameProperty:
			{
				element.StringRepresentation = DisplayName.ToString();
			}
			break;

		case EBIMPresetEditorField::DimensionProperty:
			{
				FBIMDimensions dimensions;
				if (TryGetCustomData(dimensions) && dimensions.HasCustomDimension(FName(element.FieldName)))
				{
					float v;
					dimensions.TryGetDimension(FName(element.FieldName), v);
					const auto& settings = InDocument->GetCurrentSettings();			
					element.StringRepresentation = UModumateDimensionStatics::CentimetersToDisplayText(v, 1, settings.DimensionType, settings.DimensionUnit).ToString();
				}
			}
			break;

		case EBIMPresetEditorField::NumberProperty:
			{
				float v;
				if (ensureAlways(TryGetCustomDataNumber(*element.CustomDataStructName, *element.FieldName, v)))
				{
					element.StringRepresentation = FString::SanitizeFloat(v);
				}
			}
			break;
		case EBIMPresetEditorField::IntegerProperty:
			{
				float v;
				if (TryGetCustomDataNumber(*element.CustomDataStructName, *element.FieldName, v))
				{
					int64 integerValue = static_cast<int>(v);
					element.StringRepresentation = FString::FromInt(integerValue);
				}
			}
			break;
		case EBIMPresetEditorField::ColorProperty:
			{
				ensureAlways(TryGetCustomDataColor(*element.CustomDataStructName, *element.FieldName, element.StringRepresentation));
			}
			break;
		case EBIMPresetEditorField::DropdownProperty:
			{
				FString enumStringValue;
				if (ensureAlways(TryGetCustomDataString(*element.CustomDataStructName, *element.FieldName, enumStringValue)))
				{
					TArray<TPair<FText, int64>> enumValues;
					GetEnumDisplayNamesAndValues(element.EnumClassName, enumValues);
					auto value = enumValues.FindByPredicate([enumStringValue](TPair<FText, int64>& p) { return p.Key.ToString() == enumStringValue; });
					if (value != nullptr)
					{
						element.StringRepresentation = FString::FromInt(value->Value);
					}
				}
			}
			break;
		case EBIMPresetEditorField::TextProperty:
		case EBIMPresetEditorField::AssetProperty:
		default:
			{
				ensureAlways(TryGetCustomDataString(*element.CustomDataStructName, *element.FieldName, element.StringRepresentation));
			}
			break;
		};
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::GetForm(const UModumateDocument* InDocument, FBIMPresetForm& OutForm) const
{
	AEditModelGameState* gameState = InDocument->GetWorld() ? Cast<AEditModelGameState>(InDocument->GetWorld()->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return EBIMResult::Error;
	}
	
	FBIMPresetCollection collection = gameState->Document->GetPresetCollection();
	
	// Add a default DisplayName field, it should always be added first
	OutForm.AddPropertyElement(LOCTEXT("BIMPresetInstance","Name"), TEXT("Preset"), TEXT("DisplayName"), EBIMPresetEditorField::NameProperty);

	// Add the elements from the NCP structure
	OutForm.Elements.Append(collection.PresetTaxonomy.GetFormTemplate(MyTagPath).Elements);

	// Custom Dimensions for presets are unique meaning we need to add them dynamically
	TryGetCustomDimensionFormElements(OutForm);

	// Material bindings are created based on how many material channels exist
	// TODO: make the form dynamic based on how many values exist. Meaning if we have more than 1 value in the value array we need to add them dynamically
	FBIMPresetMaterialBindingSet bindingSet;
	if (TryGetCustomData(bindingSet))
	{
		bindingSet.SetFormElements(OutForm);
	}
	
	return UpdateFormElements(InDocument,OutForm);
}

void FBIMPresetInstance::TryGetCustomDimensionFormElements(FBIMPresetForm& OutForm) const
{
	FBIMDimensions dims;
	if (TryGetCustomData(dims))
	{
		dims.ForEachCustomDimension([&OutForm](const FBIMNameType& DimKey, float Value) {
			FPartNamedDimension* namedDimension = FBIMPartSlotSpec::NamedDimensionMap.Find(DimKey.ToString());
			if (namedDimension != nullptr)
			{
				OutForm.AddPropertyElement(
					namedDimension->DisplayName,
					FName(TEXT("Dimensions")),
					DimKey,
					EBIMPresetEditorField::DimensionProperty
				);
			}
		});
	}
}

/*
* When a new mesh is assigned to a preset with material channels:
* 1. Remove any current channels the new mesh does not carry
* 2. Add default mappings for any channels in the new mesh that the preset doesn't have
*/
EBIMResult FBIMPresetInstance::SetMaterialChannelsForMesh(const FBIMPresetCollection& PresetCollection, const FGuid& InMeshGuid)
{
	FBIMPresetMaterialBindingSet bindingSet;

	if (!TryGetCustomData(bindingSet))
	{
		return EBIMResult::Error;
	}

	const FArchitecturalMesh* mesh = PresetCollection.GetArchitecturalMeshByGUID(InMeshGuid);
	if (mesh == nullptr || !mesh->EngineMesh.IsValid())
	{
		return EBIMResult::Error;
	}

	// Gather all the named channels from the engine mesh
	TArray<FName> staticMats;
	Algo::Transform(mesh->EngineMesh.Get()->GetStaticMaterials(), staticMats,
		[](const FStaticMaterial& StaticMaterial) 
		{
			return StaticMaterial.MaterialSlotName; 
		});
		
	// Preserve material bindings that are in the new mesh
	bindingSet.MaterialBindings = bindingSet.MaterialBindings.FilterByPredicate(
		[staticMats](const FBIMPresetMaterialBinding& Binding)
		{
			return staticMats.Contains(Binding.Channel);
		});

	// Gather all of the named channels from the preset
	TArray<FName> bindingMats;
	Algo::Transform(bindingSet.MaterialBindings, bindingMats,
		[](const FBIMPresetMaterialBinding& Binding)
		{
			return Binding.Channel; 
		});


	FGuid defaultMaterialGuid = PresetCollection.DefaultMaterialGUID;
	// Add default bindings for any new channels
	for (auto& staticMat : staticMats)
	{
		if (!bindingMats.Contains(staticMat))
		{
			FBIMPresetMaterialBinding& newBinding = bindingSet.MaterialBindings.AddDefaulted_GetRef();
			newBinding.Channel = staticMat;
			newBinding.ColorHexValue = FColor::White.ToHex();
			newBinding.InnerMaterialGUID = defaultMaterialGuid;
		}
	}

	SetCustomData(bindingSet);

	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::GetModularDimensions(FVector& OutDimensions, float& OutBevelWidth, float& OutThickness) const
{
	static const FBIMTagPath planarModule(TEXT("Part_0FlexDims3Fixed_ModulePlanar"));
	static const FBIMTagPath studModule(TEXT("Part_1FlexDim2Fixed_ModuleLinear"));
	static const FBIMTagPath brickModule(TEXT("Part_0FlexDims3Fixed_ModuleVolumetric"));
	static const FBIMTagPath continuousLayer(TEXT("Assembly_2FlexDims1Fixed"));
	static const FBIMTagPath gapModule2D(TEXT("Part_1FlexDim2Fixed_Gap2D"));
	static const FBIMTagPath gapModule1D(TEXT("Part_1FlexDim2Fixed_Gap1D"));

	FBIMDimensions presetDimensions;
	TryGetCustomData(presetDimensions);
	if (!presetDimensions.TryGetDimension(BIMPropertyNames::BevelWidth, OutBevelWidth))
	{
		OutBevelWidth = 0.0f;
	}
	
	// A preset is a Gap if it's in Gap scope or if it's one of the undimensioned gaps used for stud style walls
	if (NodeScope == EBIMValueScope::Gap || gapModule2D.MatchesPartial(MyTagPath) || gapModule1D.MatchesPartial(MyTagPath))
	{
		if (gapModule1D.MatchesPartial(MyTagPath))
		{
			OutDimensions = FVector::ZeroVector;
			return EBIMResult::Success;
		}
		else if (gapModule2D.MatchesPartial(MyTagPath))
		{
			OutDimensions.Z = 0;
			if (
				presetDimensions.TryGetDimension(BIMPropertyNames::Width, OutDimensions.X) &&
				presetDimensions.TryGetDimension(BIMPropertyNames::Recess, OutDimensions.Y)
				)
			{
				OutThickness = OutDimensions.Y;
				return EBIMResult::Success;
			}
		}
		return EBIMResult::Error;
	}

	/*
	* TODO: module dimensions to be refactored to have standardized terms in BIM data (DimX, DimY and DimZ)
	* In the meantime, this function will translate colloqiual property names base on the NCP of the module
	* When the data upgrade step is required, this function will translate old modular dimensions into the standardized scheme
	*/
	if (NodeScope == EBIMValueScope::Module || NodeScope == EBIMValueScope::Layer)
	{
		if (planarModule.MatchesPartial(MyTagPath))
		{
			if (presetDimensions.TryGetDimension(BIMPropertyNames::Length, OutDimensions.X) &&
				presetDimensions.TryGetDimension(BIMPropertyNames::Thickness, OutDimensions.Y) &&
				presetDimensions.TryGetDimension(BIMPropertyNames::Width, OutDimensions.Z))
			{
				OutThickness = OutDimensions.Y;
				return EBIMResult::Success;
			}
		}
		else if (studModule.MatchesPartial(MyTagPath))
		{
			OutDimensions.Y = 0.0f;
			if (presetDimensions.TryGetDimension(BIMPropertyNames::Width, OutDimensions.X))
			{
				if (presetDimensions.TryGetDimension(BIMPropertyNames::Length, OutDimensions.Z) ||
					presetDimensions.TryGetDimension(BIMPropertyNames::Depth, OutDimensions.Z))
				{
					OutThickness = OutDimensions.Z;
					return EBIMResult::Success;
				}
			}
		}
		else if (brickModule.MatchesPartial(MyTagPath))
		{
			if (presetDimensions.TryGetDimension(BIMPropertyNames::Length, OutDimensions.X) &&
				presetDimensions.TryGetDimension(BIMPropertyNames::Width, OutDimensions.Y) &&
				presetDimensions.TryGetDimension(BIMPropertyNames::Height, OutDimensions.Z))
			{
				OutThickness = OutDimensions.Y;
				return EBIMResult::Success;
			}
		}
		else if (continuousLayer.MatchesPartial(MyTagPath))
		{
			OutDimensions.X = OutDimensions.Z = 0;
			if (presetDimensions.TryGetDimension(BIMPropertyNames::Thickness, OutDimensions.Y))
			{
				OutThickness = OutDimensions.Y;
				return EBIMResult::Success;
			}
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::UpgradeData(FBIMPresetCollection& PresetCollection, int32 InDocVersion)
{
	// Prior to version 12, some NCPs had spaces
	if (InDocVersion < 12)
	{
		FString ncp;
		MyTagPath.ToString(ncp);
		MyTagPath.FromString(FString(ncp.Replace(TEXT(" "), TEXT(""))));
	}

	// Prior to version 13, patterns were stored as children. Convert to property.
	if (InDocVersion < 13)
	{
		for (auto& childPin : ChildPresets)
		{
			const FBIMPresetInstance* child = PresetCollection.PresetFromGUID(childPin.PresetGUID);
			if (ensureAlways(child) && child->NodeScope == EBIMValueScope::Pattern)
			{
				Properties_DEPRECATED.SetProperty(EBIMValueScope::Pattern, BIMPropertyNames::AssetID, child->GUID.ToString());
				RemoveChildPreset(childPin.ParentPinSetIndex, childPin.ParentPinSetPosition);
				break;
			}
		}
	}

	// Prior to version 14, beams and columns had their dimensions reversed
	if (InDocVersion < 14)
	{
		switch (ObjectType)
		{
		case EObjectType::OTMullion:
		case EObjectType::OTStructureLine:
		{
			float extrusionWidth, extrusionDepth;
			if (Properties_DEPRECATED.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, extrusionWidth) &&
				Properties_DEPRECATED.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, extrusionDepth))
			{
				Properties_DEPRECATED.SetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, extrusionDepth);
				Properties_DEPRECATED.SetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, extrusionWidth);
			}
			break;
		}
		default:
			break;
		}
	}

	// Prior to version 16, layers did not have miter priority data
	if (InDocVersion < 16 && NodeScope == EBIMValueScope::Layer)
	{
		FBIMPresetLayerPriority layerPriority;
		if (ensureAlways(!TryGetCustomData(layerPriority)))
		{
			layerPriority.PriorityGroup = EBIMPresetLayerPriorityGroup::Structure;
			layerPriority.PriorityValue = 0;
			SetCustomData(layerPriority);
		}
	}

	// Starting with version 20, all presets are expected to have mark and comment fields
	if (InDocVersion < 20)
	{
		FBIMPropertyKey propertyKey(EBIMValueScope::Preset, BIMPropertyNames::Mark);
		Properties_DEPRECATED.SetProperty(propertyKey.Scope, propertyKey.Name, FString());

		propertyKey = FBIMPropertyKey(EBIMValueScope::Preset, BIMPropertyNames::Comments);
		Properties_DEPRECATED.SetProperty(propertyKey.Scope, propertyKey.Name, FString());
	}
	
	// With version 24, Pattern->PatternRef etc. DisplayName is also a field, not a property
	if (InDocVersion < 24)
	{
		FString patternVal;
		if (Properties_DEPRECATED.TryGetProperty(EBIMValueScope::Pattern, BIMPropertyNames::AssetID, patternVal))
		{
			Properties_DEPRECATED.DeleteProperty<FString>(EBIMValueScope::Pattern, BIMPropertyNames::AssetID);
			Properties_DEPRECATED.SetProperty<FString>(EBIMValueScope::PatternRef, BIMPropertyNames::AssetID, patternVal);
		}

		FString profileRef;
		if (Properties_DEPRECATED.TryGetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID, profileRef))
		{
			Properties_DEPRECATED.DeleteProperty<FString>(EBIMValueScope::Profile, BIMPropertyNames::AssetID);
			Properties_DEPRECATED.SetProperty<FString>(EBIMValueScope::ProfileRef, BIMPropertyNames::AssetID, profileRef);
			
		}

		FString meshVal;
		if (Properties_DEPRECATED.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshVal))
		{
			Properties_DEPRECATED.DeleteProperty<FString>(EBIMValueScope::Mesh, BIMPropertyNames::AssetID);
			Properties_DEPRECATED.SetProperty<FString>(EBIMValueScope::MeshRef, BIMPropertyNames::AssetID, meshVal);
		}

		FString presetName;
		if (Properties_DEPRECATED.TryGetProperty(NodeScope, FName(TEXT("Name")), presetName))
		{
			DisplayName = FText::FromString(presetName);
			Properties_DEPRECATED.DeleteProperty<FString>(NodeScope, FName(TEXT("Name")));
		}

		// in the older csvs, two material bindings with same channels were added for some presets.
		// this removes these unused material bindings
		FBIMPresetMaterialBindingSet bindings;
		if (TryGetCustomData(bindings))
		{
			TSet<FName> usedChannels;
			bindings.MaterialBindings = bindings.MaterialBindings.FilterByPredicate([&usedChannels](const FBIMPresetMaterialBinding& binding)
			{
				if (!usedChannels.Contains(binding.Channel))
				{
					usedChannels.Add(binding.Channel);
					return true;
				}
				
				return false;
			});

			SetCustomData(bindings);
		}
	}
		
	// Add back missing pin-sets
	if (InDocVersion < 25)
	{
		PinSets = PresetCollection.PresetTaxonomy.GetPinSets(MyTagPath);
		PartSlots = PartSlots.FilterByPredicate([](const FBIMPresetPartSlot& self){return self.PartPresetGUID.IsValid();});
	}
		
	if (InDocVersion < 26)
	{
		Properties_DEPRECATED.ForEachProperty([this](const FBIMPropertyKey& Key, const FString& Value)
		{
			if (Key.Scope == EBIMValueScope::MeshRef)
			{
				FBIMMeshRef meshRef;
				FGuid::Parse(Value, meshRef.Source);
				SetCustomData(meshRef);
			}
			else if (Key.Scope == EBIMValueScope::PatternRef)
			{
				FBIMPatternRef patternRef;
				FGuid::Parse(Value, patternRef.Source);
				SetCustomData(patternRef);
			}
			else if (Key.Scope == EBIMValueScope::ProfileRef)
			{
				FBIMProfileRef profileRef;
				FGuid::Parse(Value, profileRef.Source);
				SetCustomData(profileRef);
			}
			else if (Key.Scope == EBIMValueScope::Preset)
            {
                FBIMPresetInfo presetInfo;
				if (Key.Name == TEXT("Mark"))
				{
					presetInfo.Mark = Value;	
				}
				else if (Key.Name == TEXT("Comments"))
				{
                  	presetInfo.Comments = Value;
				}
                SetCustomData(presetInfo);
            }
			else if (Key.Scope == EBIMValueScope::Part)
            {
                FBIMPartConfig partConfig;
				if (Key.Name == TEXT("Normal"))
				{
					partConfig.Normal = Value;
				}
				else if (Key.Name == TEXT("Tangent"))
				{
                  	partConfig.Tangent = Value;
				}
				else if (Key.Name == TEXT("Zalign"))
				{
					FString rawVal = Value;
					partConfig.Zalign = !rawVal.IsEmpty();
				}
                SetCustomData(partConfig);
            }
		});

		FBIMDimensions dimensions;
		int32 totalDimPropertyCount;
		bool addDimensions = false;
		Properties_DEPRECATED.ForEachProperty([this, &dimensions, &totalDimPropertyCount, &addDimensions](const FBIMPropertyKey& Key, const float Value)
		{
			if (Key.Scope == EBIMValueScope::Dimension)
			{
				addDimensions = true;
				dimensions.SetCustomDimension(Key.Name, Value);
				totalDimPropertyCount++;
			}
		});

		if (addDimensions)
		{
			// In this version, we no longer store every dimension value, only custom values. We need to know whether something
			// has default values though.
			dimensions.bHasDefaults = totalDimPropertyCount > 100;
			SetCustomData(dimensions);
		}

		FBIMTagPath OldTagPath(TEXT("Assembly_2FlexDims1Fixed_PlanarPattern"));
		FBIMTagPath NewTagPath(TEXT("Assembly_2FlexDims1Fixed_Layer0D"));
		if (MyTagPath == OldTagPath && !Properties_DEPRECATED.HasProperty<FString>(EBIMValueScope::PatternRef, BIMPropertyNames::AssetID))
		{
			MyTagPath = NewTagPath;
		}

		FBIMPresetInstanceFactory::AddMissingCustomDataToPreset(*this, PresetCollection.PresetTaxonomy);
	}

	// Single graph -> multiple graphs for Symbol Presets
	if (InDocVersion < 27 && NodeScope == EBIMValueScope::Symbol)
	{
		auto* customData = CustomDataByClassName.Find(TEXT("BIMSymbolPresetData"));
		if (ensure(customData))
		{
			TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
			customData->GetJsonObject(jsonObject);
			FBIMSymbolPresetDataV26 symbolData26;
			FJsonObjectConverter::JsonObjectToUStruct(jsonObject.ToSharedRef(), &symbolData26);
			FBIMSymbolPresetData symbolData;
 			symbolData.Members = MoveTemp(symbolData26.Members);
			int32 maxID = 0;
			for (auto& member : symbolData.EquivalentIDs)
			{
				maxID = FMath::Max(maxID, member.Key);
			}
			int32 groupID = maxID + 1;
 			symbolData.Graphs.Add(groupID, MoveTemp(symbolData26.Graph3d));
			symbolData.RootGraph = groupID;
 			symbolData.SurfaceGraphs = MoveTemp(symbolData26.SurfaceGraphs);
 			symbolData.EquivalentIDs = MoveTemp(symbolData26.EquivalentIDs);
 			symbolData.Anchor = symbolData26.Anchor;

			customData->SaveStructData(symbolData, true);
		}
	}

	return EBIMResult::Success;
}


EBIMResult FBIMPresetInstance::FromWebPreset(const FBIMWebPreset& InPreset, UWorld* World)
{
	
	MyTagPath = InPreset.tagPath;
	GUID = InPreset.guid;
	DisplayName = FText::FromString(InPreset.name);

	ensureMsgf(InPreset.origination != EPresetOrigination::Unknown, TEXT("Web preset lacks origination: %s"), *InPreset.guid.ToString());
	Origination = InPreset.origination;
	CanonicalBase = InPreset.canonicalBase;
	
	AEditModelGameState* gameState = World ? Cast<AEditModelGameState>(World->GetGameState()) : nullptr;
	
	if (gameState == nullptr)
	{
		return EBIMResult::Error;
	}

	FBIMPresetCollection collection = gameState->Document->GetPresetCollection();

	// get ncp node and if it does not exist we need to exit. Can't create preset without a valid ncp
	FBIMPresetTaxonomyNode ncpNode;
	if (collection.PresetTaxonomy.GetExactMatch(MyTagPath, ncpNode) == EBIMResult::Error)
	{
		return EBIMResult::Error;
	}
	
	// get the object type and node scope from ncp
	collection.PresetTaxonomy.PopulateDefaults(*this);

	// ets make sure that this preset has all the default information
	FBIMPresetInstanceFactory::AddMissingCustomDataToPreset(*this, collection.PresetTaxonomy);
	
	// get the property definitions
	TMap<FString, FBIMWebPresetPropertyDef> propTypes;
	collection.PresetTaxonomy.GetPropertiesForTaxonomyNode(ncpNode.tagPath, propTypes);
	
	CreateCustomDataFromWebProperties(InPreset, World);
	
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::ToWebPreset(FBIMWebPreset& OutPreset, UWorld* World) const
{
	OutPreset.name = DisplayName.ToString();
	OutPreset.guid = GUID;
	OutPreset.tagPath = MyTagPath;
	OutPreset.origination = Origination;
	OutPreset.canonicalBase = CanonicalBase;
	
	TMap<FString, FBIMWebPresetProperty> properties;

	Properties_DEPRECATED.ForEachProperty([&properties](const FBIMPropertyKey& Key, const FString& Value)
		{
			FBIMWebPresetProperty property;
			property.key = Key.QN().ToString();
			property.value.Add(Value);
			properties.Add(property.key, property);
		}
	);

	Properties_DEPRECATED.ForEachProperty([&properties](const FBIMPropertyKey& Key, float Value)
		{
			FBIMWebPresetProperty property;
			property.key = Key.QN().ToString();
			property.value.Add(FString::Printf(TEXT("%f"),Value));
			properties.Add(property.key, property);
		}
	);

	if (PartSlots.Num() > 0)
	{
		// Add slot and part preset GUIDs, part preset guids,and slot config to Properties for Web
		FBIMWebPresetProperty slotNameProperty;
		slotNameProperty.key = TEXT("Slots.SlotName");
	
		FBIMWebPresetProperty partPresetsProperty;
		partPresetsProperty.key = TEXT("Slots.PartPreset");

		FBIMWebPresetProperty slotConfigProperty;
		slotConfigProperty.key = TEXT("Slots.SlotConfig");
		slotConfigProperty.value.Add(SlotConfigPresetGUID.ToString());
	
		for (auto& partSlot : PartSlots)
		{
			slotNameProperty.value.Add(partSlot.SlotPresetGUID.ToString());
			partPresetsProperty.value.Add(partSlot.PartPresetGUID.ToString());
		}
	
		properties.Add(slotNameProperty.key, slotNameProperty);
		properties.Add(slotConfigProperty.key, slotConfigProperty);
		properties.Add(partPresetsProperty.key, partPresetsProperty);
	}
	
	OutPreset.properties = properties;

	ConvertCustomDataToWebProperties(OutPreset, World);
	ConvertChildPresetsToWebProperties(OutPreset);

	// typeMark is deprecated
	const FBIMPropertyKey propertyKey(EBIMValueScope::Preset, BIMPropertyNames::Mark);
	const FString typeMark = Properties_DEPRECATED.GetProperty<FString>(propertyKey.Scope, propertyKey.Name);
	OutPreset.typeMark = typeMark;

	return EBIMResult::Success;
}

void FBIMPresetInstance::CreateCustomDataFromWebProperties(const FBIMWebPreset& InPreset, UWorld* World)
{
	
	// TODO: the only custom data that does not work with properties currently is Symbols
	// This needs to be fixed and there is a ticket for it. This is a bugfix for the release of 3.4
	FPresetCustomDataWrapper presetCustomData;
	if (ReadJsonGeneric<FPresetCustomDataWrapper>(InPreset.customDataJSON, &presetCustomData)) {
		// we only want the symbols custom data.
		FName symbolStructName(FBIMSymbolPresetData::StaticStruct()->GetName());
		auto* symbolCustomData = presetCustomData.CustomDataWrapper.Find(symbolStructName);
		if (symbolCustomData != nullptr)
		{
				auto& addedVal = CustomDataByClassName.Add(symbolStructName, *symbolCustomData);
				addedVal.LoadFromJson();
		}
	}
	
	TMap<EPresetPropertyMatrixNames, TMap<FString, TArray<FString>>> propertyMap;
	
	for (auto& presetProp : InPreset.properties)
	{
		FString StructName = TEXT("");
		FString PropertyName = TEXT("");
		EPresetPropertyMatrixNames matrixName = EPresetPropertyMatrixNames::Error;
		presetProp.Key.Split(TEXT("."), &StructName, &PropertyName);

		if (FindEnumValueByName(FName(StructName), matrixName))
		{
			if (propertyMap.Contains(matrixName))
			{
				propertyMap[matrixName].Add(PropertyName, presetProp.Value.value);
			}
			else
			{
				TMap<FString, TArray<FString>> properties;
				properties.Add(PropertyName, presetProp.Value.value);
				propertyMap.Add(matrixName, properties);
			}
		}
	}

	for (auto& presetProp : propertyMap)
	{
		//Ty and use the CustomData deserializer
		if(!FBIMPresetInstanceFactory::DeserializeCustomData(presetProp.Key, *this, InPreset))
		{
			//It's not custom data!
			switch (presetProp.Key)
			{
				case EPresetPropertyMatrixNames::Slots:
				{
					// Get slot and part preset GUIDs, part preset guids,and slot config to Properties for Web
					FString SlotConfigPropertyName = TEXT("Slots.SlotConfig");
					FString PartPresetPropertyName = TEXT("Slots.PartPreset");
					FString SlotNamePropertyName = TEXT("Slots.SlotName");

					if (InPreset.properties.Contains(SlotConfigPropertyName))
					{
						FBIMWebPresetProperty slotConfigProperty = InPreset.properties[SlotConfigPropertyName];
						FGuid slotConfigGuid;
						FGuid::Parse(slotConfigProperty.value[0], slotConfigGuid);
						SlotConfigPresetGUID = slotConfigGuid;
					}

					if (InPreset.properties.Contains(PartPresetPropertyName) && InPreset.properties.Contains(SlotNamePropertyName))
					{
						FBIMWebPresetProperty slotNamesProperty = InPreset.properties[SlotNamePropertyName];
						FBIMWebPresetProperty partPresetsProperty = InPreset.properties[PartPresetPropertyName];
			
						for (int i = 0; i < partPresetsProperty.value.Num(); i++)
						{
							FBIMPresetPartSlot& partSlot = PartSlots.AddDefaulted_GetRef();

							FGuid slotPresetGuid, partPresetGuid;
							if (FGuid::Parse(partPresetsProperty.value[i], partPresetGuid) && FGuid::Parse(slotNamesProperty.value[i], slotPresetGuid))
							{
								partSlot.PartPresetGUID = partPresetGuid;
								partSlot.SlotPresetGUID = slotPresetGuid;
							}
						}
					}
				}
				break;
				case EPresetPropertyMatrixNames::InputPins:
				{
					//For Each property, we get the pinName
					for (auto prop : presetProp.Value)
					{
						FName pinName = FName(prop.Key);
						bool found = false;

						//This is an array of guids that corresponds to a child preset
						for (auto propValue : prop.Value)
						{
							if (propValue.IsEmpty())
							{
								continue;;
							}

							//Iterate the pinsets and find the correct setPosition and setIndex
							for (int32 setIndex = 0; setIndex < PinSets.Num(); ++setIndex)
							{
								if (PinSets[setIndex].SetName == pinName)
								{
									int32 setPosition = 0;
									for (auto& cap : ChildPresets)
									{
										if (cap.ParentPinSetIndex == setIndex)
										{
											setPosition = FMath::Max(cap.ParentPinSetPosition + 1, setPosition);
										}
									}

									FGuid guid;
									if (ensure(FGuid::Parse(propValue, guid)))
									{
										AddChildPreset(guid, setIndex, setPosition);
									}
									
									found = true;
								}
							}
						}
					
						if (!found)
						{
							//PRESET_INTEGRATION_TODO: Validate that any missed pins have a -1 in the taxonomy for that pin
							UE_LOG(LogTemp, Warning, TEXT("Could not find pin [%s] on preset [%s]"), *pinName.ToString(), *GUID.ToString());
						}
					}
				}
				break;
			default:
				ensure(false);
				break;
			}
		}
	}
}

void FBIMPresetInstance::ConvertCustomDataToWebProperties(FBIMWebPreset& OutPreset, UWorld* World) const
{
	//Populate CustomData JSON string (for legacy)
	FString customDataJSONString;
	FPresetCustomDataWrapper presetCustomData;
	presetCustomData.CustomDataWrapper = CustomDataByClassName;
	WriteJsonGeneric<FPresetCustomDataWrapper>(customDataJSONString, &presetCustomData);
	OutPreset.customDataJSON = customDataJSONString;

	for (auto& kvp : CustomDataByClassName)
	{
		FCustomDataWebConvertable* s = kvp.Value.CreateStructFromJSON<FCustomDataWebConvertable>();
		s->ConvertToWebPreset(OutPreset);	
	}
}

void FBIMPresetInstance::ConvertChildPresetsToWebProperties(FBIMWebPreset& OutPreset) const
{
	TMap<EBIMPinTarget, TArray<FString>> childPresetProps;
	for (auto item : ChildPresets)
	{
		// example of an InputPins prop key is InputPins.ChildRiser
		FString pinKey = TEXT("InputPins.Child") + GetEnumValueString(item.Target);
		auto* guids = OutPreset.properties.Find(pinKey);

		if(guids == nullptr)
		{
			FBIMWebPresetProperty inputPinsProperty;
			inputPinsProperty.key = pinKey;
			OutPreset.properties.Add(pinKey, inputPinsProperty);
			guids = OutPreset.properties.Find(pinKey);
		}
		guids->value.Push(item.PresetGUID.ToString());

	}
}

TSharedPtr<FJsonValue> FBIMPresetInstance::GetCustomDataValue(const FString& DataStruct, const FString& FieldName) const
{
	const FStructDataWrapper* wrapper = CustomDataByClassName.Find(*DataStruct);

	if (!wrapper)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> jsonOb;
	wrapper->GetJsonObject(jsonOb);

	// KLUDGE:
	// In order to check against the JSON. we need to lowercase the first letter to match the UE JSON serialize standard.
	FString FixedFieldName = FieldName;
	FixedFieldName[0] = FChar::ToLower(FixedFieldName[0]);

	return jsonOb.Get()->TryGetField(FixedFieldName);
}

bool FBIMPresetInstance::TryGetCustomDataString(const FString& DataStruct, const FString& FieldName, FString& OutStr) const
{
	TSharedPtr<FJsonValue> value = GetCustomDataValue(DataStruct, FieldName);

	if (value != nullptr && value->Type == EJson::String)
	{
		OutStr = value->AsString();
		return true;
	}

	return false;
}

bool FBIMPresetInstance::TryGetCustomDataColor(const FString& DataStruct, const FString& FieldName, FString& OutStr) const
{
	TSharedPtr<FJsonValue> value = GetCustomDataValue(DataStruct, FieldName);

	if (value != nullptr && value->Type == EJson::Object)
	{
		auto obj = value->AsObject();
		if (obj->HasField("r") && obj->HasField("g") && obj->HasField("b") && obj->HasField("a"))
		{
			auto r = obj->GetIntegerField("r");
			auto g = obj->GetIntegerField("g");
			auto b = obj->GetIntegerField("b");
			auto a = obj->GetIntegerField("a");
			OutStr = FColor(r,g,b,a).ToHex();
			return true;
		}
	}

	return false;
}

bool FBIMPresetInstance::TryGetCustomDataNumber(const FString& DataStruct, const FString& FieldName, float& OutNum) const
{
	TSharedPtr<FJsonValue> value = GetCustomDataValue(DataStruct, FieldName);

	if (value != nullptr && value->Type == EJson::Number)
	{
		OutNum = value->AsNumber();
		return true;
	}

	return false;
}

bool FBIMPresetInstance::TrySetCustomDataString(const FString& DataStruct, const FString& FieldName, const FString& InStr)
{
	FStructDataWrapper* wrapper = CustomDataByClassName.Find(*DataStruct);

	if (!wrapper)
	{
		return false;
	}

	TSharedPtr<FJsonObject> jsonOb;
	wrapper->GetJsonObject(jsonOb);
	// KLUDGE: In order to check against the JSON. we need to lowercase the first letter to match the UE JSON serialize standard.
	FString FixedFieldName = FieldName;
	FixedFieldName[0] = FChar::ToLower(FixedFieldName[0]);

	if (jsonOb.IsValid() && jsonOb.Get()->HasTypedField<EJson::String>(FixedFieldName))
	{
		jsonOb.Get()->SetStringField(FixedFieldName, InStr);
		wrapper->SetJsonObject(jsonOb);
		return true;
	}

	return false;
}

bool FBIMPresetInstance::TrySetCustomDataNumber(const FString& DataStruct, const FString& FieldName, const float InNum)
{
	FStructDataWrapper* wrapper = CustomDataByClassName.Find(*DataStruct);

	if (!wrapper)
	{
		return false;
	}

	TSharedPtr<FJsonObject> jsonOb;
	wrapper->GetJsonObject(jsonOb);

	// KLUDGE: In order to check against the JSON. we need to lowercase the first letter to match the UE JSON serialize standard.
	FString FixedFieldName = FieldName;
	FixedFieldName[0] = FChar::ToLower(FixedFieldName[0]);
	if (jsonOb.IsValid() && jsonOb.Get()->HasTypedField<EJson::Number>(FixedFieldName))
	{
		jsonOb.Get()->SetNumberField(FixedFieldName, InNum);
		wrapper->SetJsonObject(jsonOb);
		return true;
	}

	return false;
}

bool FBIMPresetInstance::TrySetCustomDataColor(const FString& DataStruct, const FString& FieldName, const FString HexVal)
{
	FStructDataWrapper* wrapper = CustomDataByClassName.Find(*DataStruct);

	if (!wrapper)
	{
		return false;
	}

	TSharedPtr<FJsonObject> jsonOb;
	wrapper->GetJsonObject(jsonOb);
	
	// KLUDGE: In order to check against the JSON. we need to lowercase the first letter to match the UE JSON serialize standard.
	FString FixedFieldName = FieldName;
	FixedFieldName[0] = FChar::ToLower(FixedFieldName[0]);
	
	if (jsonOb.IsValid() && jsonOb.Get()->HasTypedField<EJson::Object>(FixedFieldName))
	{
		auto jsonColor = jsonOb.Get()->GetObjectField(FixedFieldName);
		FColor color = FColor::FromHex(HexVal);
		if (jsonColor->HasField("r") && jsonColor->HasField("g") && jsonColor->HasField("b") && jsonColor->HasField("a"))
		{
			jsonColor->SetNumberField("r", color.R);
			jsonColor->SetNumberField("g", color.G);
			jsonColor->SetNumberField("b", color.B);
			jsonColor->SetNumberField("a", color.A);

			jsonOb.Get()->SetObjectField(FixedFieldName, jsonColor);
			wrapper->SetJsonObject(jsonOb);
		}
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
