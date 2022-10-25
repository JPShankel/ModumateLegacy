// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"

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

	if (Properties != RHS.Properties)
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
	FBIMPropertySheet newProps = Properties;
	Properties.ForEachProperty([&](const FBIMPropertyKey& PropKey,const FString& Value) {
		FGuid guid;
		if (FGuid::Parse(Value, guid) && guid.IsValid() && guid == OldGuid)
		{
			rtn = true;
			newProps.SetProperty(PropKey.Scope, PropKey.Name, NewGuid.ToString());
		}
	});
	Properties = newProps;

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

EBIMResult FBIMPresetInstance::HandleConstructionCostLaborDelta(const FBIMPresetEditorDelta& Delta)
{
	FBIMConstructionCost constructionCost;
	float newValue;

	if (ensureAlways(TryGetCustomData(constructionCost) &&
		LexTryParseString(newValue, *Delta.NewStringRepresentation)))
	{
		constructionCost.LaborCostRate = newValue;
		SetCustomData(constructionCost);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::HandleConstructionCostMaterialDelta(const FBIMPresetEditorDelta& Delta)
{
	FBIMConstructionCost constructionCost;
	float newValue;

	if (ensureAlways(TryGetCustomData(constructionCost) &&
		LexTryParseString(newValue, *Delta.NewStringRepresentation)))
	{
		constructionCost.MaterialCostRate = newValue;
		SetCustomData(constructionCost);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::HandleLightIsSpotDelta(const FBIMPresetEditorDelta& Delta)
{
	FLightConfiguration lightConfig;
	if (ensure(TryGetCustomData(lightConfig)))
	{
		lightConfig.bAsSpotLight = Delta.NewStringRepresentation.Equals(TEXT("true"));
		SetCustomData(lightConfig);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::HandleLightColorDelta(const FBIMPresetEditorDelta& Delta)
{
	FLightConfiguration lightConfig;
	if (ensure(TryGetCustomData(lightConfig)))
	{
		lightConfig.LightColor = FColor::FromHex(Delta.NewStringRepresentation);
		SetCustomData(lightConfig);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::HandleLightIntensityDelta(const FBIMPresetEditorDelta& Delta)
{
	FLightConfiguration lightConfig;
	if (ensure(TryGetCustomData(lightConfig)))
	{
		lightConfig.LightIntensity = FCString::Atof(*Delta.NewStringRepresentation);
		SetCustomData(lightConfig);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::HandleLightRadiusDelta(const FBIMPresetEditorDelta& Delta)
{
	FLightConfiguration lightConfig;
	if (ensure(TryGetCustomData(lightConfig)))
	{
		lightConfig.SourceRadius = FCString::Atof(*Delta.NewStringRepresentation);
		SetCustomData(lightConfig);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::HandleLightProfileDelta(const FBIMPresetEditorDelta& Delta)
{
	FLightConfiguration lightConfig;
	if (ensure(TryGetCustomData(lightConfig)))
	{
		FGuid::Parse(*Delta.NewStringRepresentation, lightConfig.IESProfileGUID);
		SetCustomData(lightConfig);
		
		Properties.SetProperty(EBIMValueScope::IESProfile, BIMPropertyNames::AssetID, lightConfig.IESProfileGUID.ToString());
	}
	return EBIMResult::Success;
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
		if (binding.Channel.IsEqual(Delta.FieldName))
		{
			switch (Delta.MaterialChannelSubField)
			{
			case EMaterialChannelFields::InnerMaterial:
			{
				FGuid::Parse(Delta.NewStringRepresentation, binding.InnerMaterialGUID);
				SetCustomData(bindingSet);
				//If we don't have a surface material, the inner material is visible
				if (!binding.SurfaceMaterialGUID.IsValid())
				{
					Properties.SetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, Delta.NewStringRepresentation);
				}
				return EBIMResult::Success;
			}
			break;

			case EMaterialChannelFields::SurfaceMaterial:
			{
				FGuid::Parse(Delta.NewStringRepresentation, binding.SurfaceMaterialGUID);
				// TODO: material and color properties still used in icon generation...remove when icongen is refactored
				Properties.SetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, Delta.NewStringRepresentation);
				SetCustomData(bindingSet);
				return EBIMResult::Success;
			}
			break;

			case EMaterialChannelFields::ColorTint:
			{
				binding.ColorHexValue = Delta.NewStringRepresentation;
				// TODO: material and color properties still used in icon generation...remove when icongen is refactored
				Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, binding.ColorHexValue);
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

EBIMResult FBIMPresetInstance::HandleLayerPriorityGroupDelta(const FBIMPresetEditorDelta& Delta)
{
	FBIMPresetLayerPriority layerPriority;
	int64 newValue;

	if (ensureAlways(TryGetCustomData(layerPriority) && 
		LexTryParseString(newValue,*Delta.NewStringRepresentation)) && 
		static_cast<int64>(layerPriority.PriorityGroup) != newValue)
	{
		layerPriority.PriorityGroup = static_cast<EBIMPresetLayerPriorityGroup>(newValue);
		SetCustomData(layerPriority);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::HandleLayerPriorityValueDelta(const FBIMPresetEditorDelta& Delta)
{
	FBIMPresetLayerPriority layerPriority;
	if (TryGetCustomData(layerPriority) && LexTryParseString(layerPriority.PriorityValue, *Delta.NewStringRepresentation))
	{
		SetCustomData(layerPriority);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetInstance::ApplyDelta(const UModumateDocument* InDocument,const FBIMPresetEditorDelta& Delta)
{
	switch (Delta.FieldType)
	{
		case EBIMPresetEditorField::LayerPriorityGroup:
		{
			return HandleLayerPriorityGroupDelta(Delta);
		}

		case EBIMPresetEditorField::LayerPriorityValue:
		{
			return HandleLayerPriorityValueDelta(Delta);
		}

		case EBIMPresetEditorField::MaterialBinding:
		{
			return HandleMaterialBindingDelta(Delta);
		}

		case EBIMPresetEditorField::ConstructionCostLabor:
		{
			return HandleConstructionCostLaborDelta(Delta);
		}

		case EBIMPresetEditorField::ConstructionCostMaterial:
		{
			return HandleConstructionCostMaterialDelta(Delta);
		}

		case EBIMPresetEditorField::LightIntensity:
		{
			return HandleLightIntensityDelta(Delta);
		}

		case EBIMPresetEditorField::LightColor:
		{
			return HandleLightColorDelta(Delta);
		}

		case EBIMPresetEditorField::LightRadius:
		{
			return HandleLightRadiusDelta(Delta);
		}

		case EBIMPresetEditorField::LightProfile:
		{
			auto rtn = HandleLightProfileDelta(Delta);
			FGuid guid;
			if (ensure(FGuid::Parse(Delta.NewStringRepresentation, guid)))
			{
				FLightConfiguration lightConfig;
				if (TryGetCustomData(lightConfig))
				{
					lightConfig.IESProfileGUID = guid;
					if (!InDocument)
					{
						break;
					}
					const FBIMPresetInstance* profilePreset = InDocument->GetPresetCollection().PresetFromGUID(lightConfig.IESProfileGUID);
					FString assetPath = profilePreset->GetScopedProperty<FString>(EBIMValueScope::IESProfile, BIMPropertyNames::AssetPath);
					if (assetPath.IsEmpty())
					{
						break;
					}
					FSoftObjectPath referencePath = FString(TEXT("TextureLightProfile'")).Append(assetPath).Append(TEXT("'"));
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

		case EBIMPresetEditorField::LightIsSpot:
		{
			return HandleLightIsSpotDelta(Delta);
		}

		case EBIMPresetEditorField::NameProperty:
		{
			DisplayName = FText::FromString(Delta.NewStringRepresentation);
			return EBIMResult::Success;
		}

		case EBIMPresetEditorField::AssetProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			Properties.SetProperty(propKey.Scope, propKey.Name, Delta.NewStringRepresentation);
			FGuid guid;
			if (FGuid::Parse(Delta.NewStringRepresentation, guid))
			{
				SetMaterialChannelsForMesh(InDocument->GetPresetCollection(),guid);
			}
			return EBIMResult::Success;
		}
		break;
		case EBIMPresetEditorField::TextProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			Properties.SetProperty(propKey.Scope, propKey.Name, Delta.NewStringRepresentation);
			return EBIMResult::Success;
		}
		break;

		case EBIMPresetEditorField::NumberProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			float v;
			if (LexTryParseString(v, *Delta.NewStringRepresentation))
			{
				Properties.SetProperty(propKey.Scope, propKey.Name, v);
				return EBIMResult::Success;
			}
		}
		break;

		case EBIMPresetEditorField::DimensionProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			const auto& settings = InDocument->GetCurrentSettings();
			auto dimension = UModumateDimensionStatics::StringToFormattedDimension(Delta.NewStringRepresentation, settings.DimensionType,settings.DimensionUnit);
			if (dimension.Format != EDimensionFormat::Error)
			{
				Properties.SetProperty(propKey.Scope, propKey.Name, static_cast<float>(dimension.Centimeters));
				return EBIMResult::Success;
			}
		}
		break;

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
	OutDelta.MaterialChannelSubField = FormElement.MaterialChannelSubField;

	switch (FormElement.FieldType)
	{
	case EBIMPresetEditorField::ConstructionCostLabor:
		{
			FBIMConstructionCost constructionCost;
			if (ensure(TryGetCustomData(constructionCost)))
			{
				OutDelta.OldStringRepresentation = FString::Printf(TEXT("%0.2f"),constructionCost.LaborCostRate);
				return EBIMResult::Success;
			}
		}
		break;
	case EBIMPresetEditorField::ConstructionCostMaterial:
	{
		FBIMConstructionCost constructionCost;
		if (ensure(TryGetCustomData(constructionCost)))
		{
			OutDelta.OldStringRepresentation = FString::Printf(TEXT("%0.2f"), constructionCost.MaterialCostRate);
			return EBIMResult::Success;
		}
	}
	break;
	case EBIMPresetEditorField::LayerPriorityGroup:
		{
			FBIMPresetLayerPriority layerPriority;

			if (ensureAlways(TryGetCustomData(layerPriority)))
			{
				OutDelta.OldStringRepresentation = FString::FromInt(static_cast<int32>(layerPriority.PriorityGroup));
				return EBIMResult::Success;
			}
		}
		break;

		case EBIMPresetEditorField::LayerPriorityValue:
		{
			FBIMPresetLayerPriority layerPriority;
			if (TryGetCustomData(layerPriority))
			{
				OutDelta.OldStringRepresentation = FString::FromInt(layerPriority.PriorityValue);
				return EBIMResult::Success;
			}
		}
		break;

		case EBIMPresetEditorField::MaterialBinding:
		{
			return MakeMaterialBindingDelta(FormElement, OutDelta);
		}
		case EBIMPresetEditorField::DimensionProperty:
		{
			float v;
			FBIMPropertyKey propKey(*FormElement.FieldName);
			if (ensureAlways(Properties.TryGetProperty(propKey.Scope, propKey.Name, v)))
			{
				OutDelta.OldStringRepresentation = UModumateDimensionStatics::CentimetersToDisplayText(v).ToString();
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::NumberProperty:
		{
			float v;
			FBIMPropertyKey propKey(*FormElement.FieldName);
			if (Properties.TryGetProperty(propKey.Scope, propKey.Name, v))
			{
				OutDelta.OldStringRepresentation = FString::SanitizeFloat(v);
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::TextProperty:
		case EBIMPresetEditorField::AssetProperty:
		{
			FBIMPropertyKey propKey(*FormElement.FieldName);
			if (ensureAlways(Properties.TryGetProperty(propKey.Scope, propKey.Name, OutDelta.OldStringRepresentation)))
			{
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::LightColor:
		{
			FLightConfiguration lightConfig;
			if (TryGetCustomData(lightConfig))
			{
				OutDelta.OldStringRepresentation = lightConfig.LightColor.ToHex();
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::LightIntensity:
		{
			FLightConfiguration lightConfig;
			if (TryGetCustomData(lightConfig))
			{
				OutDelta.OldStringRepresentation = FString::Printf(TEXT("%0.2f"), lightConfig.LightIntensity);
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::LightRadius:
		{
			FLightConfiguration lightConfig;
			if (TryGetCustomData(lightConfig)) 
			{
				OutDelta.OldStringRepresentation = FString::Printf(TEXT("%0.2f"), lightConfig.SourceRadius);
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::LightProfile:
		{
			FLightConfiguration lightConfig;
			if (TryGetCustomData(lightConfig))
			{
				OutDelta.OldStringRepresentation = lightConfig.IESProfileGUID.ToString();
				return EBIMResult::Success;
			}
		}
		break;
		case EBIMPresetEditorField::LightIsSpot:
		{
			FLightConfiguration lightConfig;
			if (TryGetCustomData(lightConfig))
			{
				OutDelta.OldStringRepresentation = lightConfig.bAsSpotLight ? TEXT("true") : TEXT("false");
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
		case EBIMPresetEditorField::LightColor:
		{
			FLightConfiguration lightConfig;
			if (ensure(TryGetCustomData(lightConfig)))
			{
				element.StringRepresentation = lightConfig.LightColor.ToHex();
			}
		}
			break;
		case EBIMPresetEditorField::LightIntensity:
		{
			FLightConfiguration lightConfig;
			if (ensure(TryGetCustomData(lightConfig)))
			{
				element.StringRepresentation = FString::Printf(TEXT("%.2f"), lightConfig.LightIntensity);
			}
		}
			break;
		case EBIMPresetEditorField::LightRadius:
		{
			FLightConfiguration lightConfig;
			if (ensure(TryGetCustomData(lightConfig)))
			{
				element.StringRepresentation = FString::Printf(TEXT("%.2f"), lightConfig.SourceRadius);
			}
		}
		break;
		case EBIMPresetEditorField::LightProfile:
		{
			FLightConfiguration lightConfig;
			if (ensure(TryGetCustomData(lightConfig)))
			{
				element.StringRepresentation = lightConfig.IESProfileGUID.ToString();
			}
		}
		break;
		case EBIMPresetEditorField::LightIsSpot:
		{
			FLightConfiguration lightConfig;
			if (ensure(TryGetCustomData(lightConfig)))
			{
				element.StringRepresentation = lightConfig.bAsSpotLight ? TEXT("true") : TEXT("false");
			}
		}
		break;
		case EBIMPresetEditorField::NameProperty:
		{
			element.StringRepresentation = DisplayName.ToString();
		}
		break;
		case EBIMPresetEditorField::ConstructionCostMaterial:
		{
			FBIMConstructionCost constructionCost;
			if (ensure(TryGetCustomData(constructionCost)))
			{
				element.StringRepresentation = FString::Printf(TEXT("%0.2f"), constructionCost.MaterialCostRate);
			}
		}
		break;
		case EBIMPresetEditorField::ConstructionCostLabor:
		{
			FBIMConstructionCost constructionCost;
			if (ensure(TryGetCustomData(constructionCost)))
			{
				element.StringRepresentation = FString::Printf(TEXT("%0.2f"), constructionCost.LaborCostRate);
			}
		}
		break;
		case EBIMPresetEditorField::LayerPriorityValue:
		{
			FBIMPresetLayerPriority layerPriority;
			if (ensureAlways(TryGetCustomData(layerPriority)))
			{
				element.StringRepresentation = FString::FromInt(layerPriority.PriorityValue);
			}
		}
		break;

		case EBIMPresetEditorField::LayerPriorityGroup:
		{
			FBIMPresetLayerPriority layerPriority;
			if (ensureAlways(TryGetCustomData(layerPriority)))
			{
				element.StringRepresentation = FString::FromInt(static_cast<int32>(layerPriority.PriorityGroup));
			}
		}
		break;

		case EBIMPresetEditorField::DimensionProperty:
		{
			float v;
			FBIMPropertyKey propKey(*element.FieldName);
			if (ensureAlways(Properties.TryGetProperty<float>(propKey.Scope, propKey.Name, v)))
			{
				const auto& settings = InDocument->GetCurrentSettings();				
				element.StringRepresentation = UModumateDimensionStatics::CentimetersToDisplayText(v,1, settings.DimensionType, settings.DimensionUnit).ToString();
			}
		}
		break;

		case EBIMPresetEditorField::NumberProperty:
		{
			float v;
			FBIMPropertyKey propKey(*element.FieldName);
			if (ensureAlways(Properties.TryGetProperty<float>(propKey.Scope, propKey.Name, v)))
			{
				element.StringRepresentation = FString::SanitizeFloat(v);
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

EBIMResult FBIMPresetInstance::GetForm(const UModumateDocument* InDocument, FBIMPresetForm& OutForm) const
{
	// TODO: remove FormTemplate, given we get all the required information from the ncp taxonomy,
	// should be able to just get elements from PresetForm, which should be dynamically genereated from
	// web presets.
	auto World = InDocument->GetWorld();
	AEditModelGameState* gameState = World ? Cast<AEditModelGameState>(World->GetGameState()) : nullptr;
	
	if (gameState == nullptr)
	{
		return EBIMResult::Error;
	}
	
	FBIMPresetCollection collection = gameState->Document->GetPresetCollection();

	// Add a default DisplayName field, it should always be added first
	FBIMPresetFormElement& nameElem = OutForm.Elements.AddDefaulted_GetRef();
    nameElem.DisplayName = LOCTEXT("BIMPresetInstance","Name");
    nameElem.FieldType = EBIMPresetEditorField::NameProperty;
    nameElem.FieldName = TEXT("DisplayName");
    nameElem.FormElementWidgetType = EBIMFormElementWidget::TextEntry;

	// Add the elements from the NCP,
	// TODO: deprecate in favor of everything being custom data
	OutForm.Elements.Append(collection.PresetTaxonomy.GetFormTemplate(MyTagPath).Elements);

	// Add the PresetForm items from the preset itself
	OutForm.Elements.Append(PresetForm.Elements);
	
	return UpdateFormElements(InDocument,OutForm);
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

	return bindingSet.SetFormElements(PresetForm);
}

EBIMResult FBIMPresetInstance::GetModularDimensions(FVector& OutDimensions, float& OutBevelWidth, float& OutThickness) const
{
	static const FBIMTagPath planarModule(TEXT("Part_0FlexDims3Fixed_ModulePlanar"));
	static const FBIMTagPath studModule(TEXT("Part_1FlexDim2Fixed_ModuleLinear"));
	static const FBIMTagPath brickModule(TEXT("Part_0FlexDims3Fixed_ModuleVolumetric"));
	static const FBIMTagPath continuousLayer(TEXT("Assembly_2FlexDims1Fixed"));
	static const FBIMTagPath gapModule2D(TEXT("Part_1FlexDim2Fixed_Gap2D"));
	static const FBIMTagPath gapModule1D(TEXT("Part_1FlexDim2Fixed_Gap1D"));

	if (!Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::BevelWidth, OutBevelWidth))
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
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, OutDimensions.X) &&
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Recess, OutDimensions.Y)
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
			if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, OutDimensions.X) &&
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, OutDimensions.Y) &&
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, OutDimensions.Z))
			{
				OutThickness = OutDimensions.Y;
				return EBIMResult::Success;
			}
		}
		else if (studModule.MatchesPartial(MyTagPath))
		{
			OutDimensions.Y = 0.0f;
			if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, OutDimensions.X))
			{
				if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, OutDimensions.Z) ||
					Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, OutDimensions.Z))
				{
					OutThickness = OutDimensions.Z;
					return EBIMResult::Success;
				}
			}
		}
		else if (brickModule.MatchesPartial(MyTagPath))
		{
			if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, OutDimensions.X) &&
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, OutDimensions.Y) &&
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, OutDimensions.Z))
			{
				OutThickness = OutDimensions.Y;
				return EBIMResult::Success;
			}
		}
		else if (continuousLayer.MatchesPartial(MyTagPath))
		{
			OutDimensions.X = OutDimensions.Z = 0;
			if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, OutDimensions.Y))
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
				Properties.SetProperty(EBIMValueScope::Pattern, BIMPropertyNames::AssetID, child->GUID.ToString());
				PresetForm.AddPropertyElement(LOCTEXT("BIMPattern", "Pattern"), FBIMPropertyKey(EBIMValueScope::Pattern, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);
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
			if (Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, extrusionWidth) &&
				Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, extrusionDepth))
			{
				Properties.SetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, extrusionDepth);
				Properties.SetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, extrusionWidth);
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
		Properties.SetProperty(propertyKey.Scope, propertyKey.Name, FString());
		PresetForm.AddPropertyElement(LOCTEXT("BIMMark", "Mark"), propertyKey.QN(), EBIMPresetEditorField::TextProperty);

		propertyKey = FBIMPropertyKey(EBIMValueScope::Preset, BIMPropertyNames::Comments);
		Properties.SetProperty(propertyKey.Scope, propertyKey.Name, FString());
		PresetForm.AddPropertyElement(LOCTEXT("BIMComments", "Comments"), propertyKey.QN(), EBIMPresetEditorField::TextProperty);
	}
	
	// With version 24, Pattern->PatternRef etc. DisplayName is also a field, not a property
	if (InDocVersion < 24)
	{
		FString patternVal;
		if (Properties.TryGetProperty(EBIMValueScope::Pattern, BIMPropertyNames::AssetID, patternVal))
		{
			Properties.DeleteProperty<FString>(EBIMValueScope::Pattern, BIMPropertyNames::AssetID);
			Properties.SetProperty<FString>(EBIMValueScope::PatternRef, BIMPropertyNames::AssetID, patternVal);
		}

		FString profileRef;
		if (Properties.TryGetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID, profileRef))
		{
			Properties.DeleteProperty<FString>(EBIMValueScope::Profile, BIMPropertyNames::AssetID);
			Properties.SetProperty<FString>(EBIMValueScope::ProfileRef, BIMPropertyNames::AssetID, profileRef);
			
		}

		FString meshVal;
		if (Properties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshVal))
		{
			Properties.DeleteProperty<FString>(EBIMValueScope::Mesh, BIMPropertyNames::AssetID);
			Properties.SetProperty<FString>(EBIMValueScope::MeshRef, BIMPropertyNames::AssetID, meshVal);
		}

		// We have deprecated the name property in the bim editor
		for (auto& formElement : PresetForm.Elements)
		{
			if (formElement.FieldName == TEXT("Mesh.AssetID"))
			{
				formElement.FieldName = TEXT("MeshRef.AssetID");
			}
			else if (formElement.FieldName == TEXT("Profile.AssetID"))
			{
				formElement.FieldName = TEXT("ProfileRef.AssetID");
			}
			else if (formElement.FieldName == TEXT("Pattern.AssetID"))
			{
				formElement.FieldName = TEXT("PatternRef.AssetID");
			}
		}

		// name property deprecated in the bim editor
		PresetForm.Elements = PresetForm.Elements.FilterByPredicate([this](const FBIMPresetFormElement& elem)
		{
			return elem.FieldName != FBIMPropertyKey(NodeScope, FName(TEXT("Name"))).QN().ToString();
		});

		FString presetName;
		if (Properties.TryGetProperty(NodeScope, FName(TEXT("Name")), presetName))
		{
			DisplayName = FText::FromString(presetName);
			Properties.DeleteProperty<FString>(NodeScope, FName(TEXT("Name")));
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
	
	// get the property definitions
	TMap<FString, FBIMWebPresetPropertyDef> propTypes;
	collection.PresetTaxonomy.GetPropertiesForTaxonomyNode(ncpNode.tagPath, propTypes);
	
	ConvertWebPropertiesToCustomData(InPreset, World);
	
	for (auto& property : InPreset.properties)
	{
		// we require a type definition for a property in order to understand it's type and other meta data
		// if it does not exist, just skip it
		FBIMWebPresetPropertyDef* typeDef = propTypes.Find(property.Key);
		if (!typeDef)
		{
			continue;
		}
		
		TArray<FString> splitKey;
		property.Key.ParseIntoArray(splitKey, TEXT("."));
		if (ensure(splitKey.Num() == 2))
		{
			EBIMValueScope scope;
			if (FindEnumValueByString(splitKey[0],scope))
			{
				if (typeDef->type == EBIMWebPresetPropertyType::number)
				{
					SetScopedProperty(scope, *splitKey[1], FCString::Atof(*property.Value.value[0]));
				}
				else
				{
					SetScopedProperty(scope, *splitKey[1], property.Value.value[0]);
				}
			}
		}
	}

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

	// TODO: need scheme for identifying color properties
	const static TMap<EJson, EBIMWebPresetPropertyType> typeMap =
	{
		{EJson::Boolean,EBIMWebPresetPropertyType::boolean},
		{EJson::Number,EBIMWebPresetPropertyType::number},
		{EJson::String,EBIMWebPresetPropertyType::string}
	};

	for (auto& kvp : CustomDataByClassName)
	{
		TSharedPtr<FJsonObject> jsonObject;
		kvp.Value.GetJsonObject(jsonObject);
		for (auto& val : jsonObject->Values)
		{
			const EBIMWebPresetPropertyType* type = typeMap.Find(val.Value->Type);
			if (type != nullptr)
			{
				FBIMWebPresetProperty webProperty;
				webProperty.key = kvp.Key.ToString() + TEXT(".") + val.Key;
				webProperty.value.Add(val.Value->AsString());
				properties.Add(webProperty.key, webProperty);
			}
		}
	}

	Properties.ForEachProperty([&properties](const FBIMPropertyKey& Key, const FString& Value)
		{
			FBIMWebPresetProperty property;
			property.key = Key.QN().ToString();
			property.value.Add(Value);
			properties.Add(property.key, property);
		}
	);

	Properties.ForEachProperty([&properties](const FBIMPropertyKey& Key, float Value)
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
		slotNameProperty.key = TEXT("Slot.SlotName");
	
		FBIMWebPresetProperty partPresetsProperty;
		partPresetsProperty.key = TEXT("Slot.PartPreset");

		FBIMWebPresetProperty slotConfigProperty;
		slotConfigProperty.key = TEXT("Slot.SlotConfig");
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
	ConvertWebPropertiesFromChildPresets(OutPreset);

	// typeMark is deprecated
	const FBIMPropertyKey propertyKey(EBIMValueScope::Preset, BIMPropertyNames::Mark);
	const FString typeMark = Properties.GetProperty<FString>(propertyKey.Scope, propertyKey.Name);
	OutPreset.typeMark = typeMark;

	return EBIMResult::Success;
}

void FBIMPresetInstance::ConvertWebPropertiesToCustomData(const FBIMWebPreset& InPreset, UWorld* World)
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
		switch (presetProp.Key)
		{
		case EPresetPropertyMatrixNames::IESLight:
			{
				FLightConfiguration lightConfig;

				FGuid profileGuid;
				FGuid::Parse(presetProp.Value[TEXT("IESProfile")][0], profileGuid);
			
				lightConfig.IESProfileGUID = profileGuid;
				lightConfig.LightIntensity = FCString::Atof(*presetProp.Value[TEXT("Intensity")][0]);
				lightConfig.LightColor = FColor::FromHex(presetProp.Value[TEXT("ColorTint")][0]);
				lightConfig.Location.X = FCString::Atof(*presetProp.Value[TEXT("LocationX")][0]);
				lightConfig.Location.Y = FCString::Atof(*presetProp.Value[TEXT("LocationY")][0]);
				lightConfig.Location.Z = FCString::Atof(*presetProp.Value[TEXT("LocationZ")][0]);
				lightConfig.Scale.X = FCString::Atof(*presetProp.Value[TEXT("ScaleX")][0]);
				lightConfig.Scale.Y = FCString::Atof(*presetProp.Value[TEXT("ScaleY")][0]);
				lightConfig.Scale.Z = FCString::Atof(*presetProp.Value[TEXT("ScaleZ")][0]);
				lightConfig.Rotation.Roll = FCString::Atof(*presetProp.Value[TEXT("RotationX")][0]);
				lightConfig.Rotation.Pitch = FCString::Atof(*presetProp.Value[TEXT("RotationY")][0]);
				lightConfig.Rotation.Yaw = FCString::Atof(*presetProp.Value[TEXT("RotationZ")][0]);
				lightConfig.SourceRadius = FCString::Atof(*presetProp.Value[TEXT("SourceRadius")][0]);

				SetScopedProperty(EBIMValueScope::IESProfile, BIMPropertyNames::AssetID, profileGuid.ToString());
				PresetForm.AddLightConfigElements();
				SetCustomData(lightConfig);
			}
			break;
		case EPresetPropertyMatrixNames::ConstructionCost:
			{
				FBIMConstructionCost constructionCost;
				constructionCost.LaborCostRate = FCString::Atof(*presetProp.Value[TEXT("LaborCostRate")][0]);
				constructionCost.MaterialCostRate = FCString::Atof(*presetProp.Value[TEXT("MaterialCostRate")][0]);
				PresetForm.AddConstructionCostElements();
				SetCustomData(constructionCost);
			}
			break;
		case EPresetPropertyMatrixNames::MiterPriority:
			{
				FBIMPresetLayerPriority miterPriority;
				FString group = presetProp.Value[TEXT("Function")][0];
				FString priority = presetProp.Value[TEXT("Priority")][0];

				if (FindEnumValueByString(group, miterPriority.PriorityGroup) && LexTryParseString(miterPriority.PriorityValue, *priority))
				{
					miterPriority.SetFormElements(PresetForm);
					SetCustomData(miterPriority);
				}
			}
			break;
		case EPresetPropertyMatrixNames::PatternRef:
			{
				FString guid = presetProp.Value[TEXT("Source")][0];
				if (!guid.IsEmpty())
				{
					Properties.SetProperty(EBIMValueScope::PatternRef, BIMPropertyNames::AssetID, guid);
					PresetForm.AddPropertyElement(LOCTEXT("BIMPattern","Pattern"), FBIMPropertyKey(EBIMValueScope::PatternRef, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);
				}
			}
			break;
		case EPresetPropertyMatrixNames::ProfileRef:
			{
				FString guid = presetProp.Value[TEXT("Source")][0];

				if (!guid.IsEmpty())
				{
					Properties.SetProperty(EBIMValueScope::ProfileRef, BIMPropertyNames::AssetID, guid);
					PresetForm.AddPropertyElement(LOCTEXT("BIMProfile","Profile"), FBIMPropertyKey(EBIMValueScope::ProfileRef, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);
				}
			}
			break;
		case EPresetPropertyMatrixNames::MeshRef:
			{
				FGuid guid;
				
				if (FGuid::Parse(presetProp.Value[TEXT("Source")][0], guid))
				{
					Properties.SetProperty(EBIMValueScope::MeshRef, BIMPropertyNames::AssetID, guid.ToString());
					PresetForm.AddPropertyElement(FText::FromString(TEXT("Mesh")), FBIMPropertyKey(EBIMValueScope::MeshRef, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);
				}
			}
			break;
		case EPresetPropertyMatrixNames::Material:
			{
				TArray<FString> channels;
				// Check how many channels, to see how many materials we need to add
				if (presetProp.Value.Contains(TEXT("AppliesToChannel")))
				{
					channels.Append(presetProp.Value[TEXT("AppliesToChannel")]);	
				}
				
				// loop through the channels and add the material binding for each
				for (int32 i = 0; i < channels.Num(); ++i)
				{
					FBIMPresetMaterialBinding materialBinding;
					for (auto prop : presetProp.Value)
					{
						EMaterialChannelFields fieldEnum = GetEnumValueByString<EMaterialChannelFields>(prop.Key);
						switch (fieldEnum)
						{
						case EMaterialChannelFields::AppliesToChannel:
							materialBinding.Channel = FName(prop.Value[i]);
							break;
						case EMaterialChannelFields::InnerMaterial:
							{
								if (prop.Value.Num() > i)
								{
									FGuid guid;
                                	if (FGuid::Parse(prop.Value[i], guid))
                                	{
                                		materialBinding.InnerMaterialGUID = guid;
                                	}	
								}
							}
							break;
						case EMaterialChannelFields::SurfaceMaterial:
							{
								if (prop.Value.Num() > i)
								{
									FGuid guid;
									if (FGuid::Parse(prop.Value[i], guid))
									{
										materialBinding.SurfaceMaterialGUID = guid;
									}
								}
							}
							break;
						case EMaterialChannelFields::ColorTint:
							{
								if (prop.Value.Num() > i)
								{
									FString hexValue = prop.Value[i];
									if (!hexValue.IsEmpty())
									{
										Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, hexValue);
										materialBinding.ColorHexValue = hexValue;
									}
								}
							}
							break;
						case EMaterialChannelFields::ColorTintVariation:
							if (prop.Value.Num() > i)
							{
								LexTryParseString(materialBinding.ColorTintVariationPercent, *prop.Value[i]);
							}
							break;
						default:
							ensure(false);
						}
					}

					if (!materialBinding.Channel.IsNone())
					{
						FBIMPresetMaterialBindingSet bindingSet;
						TryGetCustomData(bindingSet);
						bindingSet.MaterialBindings.Add(materialBinding);
						SetCustomData(bindingSet);

						FGuid material = materialBinding.SurfaceMaterialGUID.IsValid() ? materialBinding.SurfaceMaterialGUID : materialBinding.InnerMaterialGUID;

						// The RawMaterial.AssetID is used in the DynamicIconGenerator, otherwise it should be deprecated
						if (ensure(material.IsValid()))
						{
							Properties.SetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, material.ToString());
						}
						
						Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, materialBinding.ColorHexValue.IsEmpty() ? FColor::White.ToHex() : materialBinding.ColorHexValue);
						bindingSet.SetFormElements(PresetForm);
					}
				}
			}
			break;
		case EPresetPropertyMatrixNames::Dimensions:
			{
				TArray<FString> dimensions = presetProp.Value["Dimension"];
				TArray<FString> values = presetProp.Value["Value"];

				// Dimensions and values are a key-to-value pair and need to be the same size
				if (ensure(dimensions.Num() == values.Num())) {
					for (int i = 0; i < dimensions.Num(); i++)
					{
						if (!dimensions[i].IsEmpty())
						{
							float value = FCString::Atof(*values[i]);
							Properties.SetProperty(EBIMValueScope::Dimension, *dimensions[i], value);
							PresetForm.AddPropertyElement(FText::FromString(dimensions[i]), FBIMPropertyKey(EBIMValueScope::Dimension, *dimensions[i]).QN(), EBIMPresetEditorField::DimensionProperty);		
						}
					}
				}
			}
			break;
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
				for (auto prop : presetProp.Value)
				{
					FName pinName = FName(prop.Key);
					bool found = false;
					
					for (auto propValue : prop.Value)
					{
						if (propValue.IsEmpty())
						{
							continue;;
						}
				
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
		case EPresetPropertyMatrixNames::EdgeDetail:
			// PRESET_INTEGRATION_TODO: Edge Detail does not even contain any data, lets move this out of custom data parsing and
			// into a processing part. -NG
			SetCustomData(FEdgeDetailData(FEdgeDetailData::CurrentVersion));
			break;
		}
	}
}

void FBIMPresetInstance::ConvertCustomDataToWebProperties(FBIMWebPreset& OutPreset, UWorld* World) const
{
	// TODO: make this the reverse of ConvertWebPropertiesToCustomData 
	FString customDataJSONString;
	FPresetCustomDataWrapper presetCustomData;
	presetCustomData.CustomDataWrapper = CustomDataByClassName;
	WriteJsonGeneric<FPresetCustomDataWrapper>(customDataJSONString, &presetCustomData);
	OutPreset.customDataJSON = customDataJSONString;
}

void FBIMPresetInstance::ConvertWebPropertiesFromChildPresets(FBIMWebPreset& OutPreset) const
{
	TMap<EBIMPinTarget, TArray<FString>> childPresetProps;
	for (auto item : ChildPresets)
	{
		// example of an InputPins prop key is InputPins.ChildRiser
		FString key = FString::Printf(TEXT("InputPins.Child")) + GetEnumValueString(item.Target);
		if (OutPreset.properties.Contains(key))
		{
			auto guids = OutPreset.properties.Find(key);
			guids->value.Push(item.PresetGUID.ToString());
		} else
		{
			FBIMWebPresetProperty inputPinsProperty;
			inputPinsProperty.key = key;
			inputPinsProperty.value.Add(item.PresetGUID.ToString());
			OutPreset.properties.Add(key, inputPinsProperty);
		}
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

	return jsonOb.Get()->TryGetField(FieldName);
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

	if (jsonOb.IsValid() && jsonOb.Get()->HasTypedField<EJson::String>(FieldName))
	{
		jsonOb.Get()->SetStringField(FieldName, InStr);
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
	if (jsonOb.IsValid() && jsonOb.Get()->HasTypedField<EJson::Number>(FieldName))
	{
		jsonOb.Get()->SetNumberField(FieldName, InNum);
		wrapper->SetJsonObject(jsonOb);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE