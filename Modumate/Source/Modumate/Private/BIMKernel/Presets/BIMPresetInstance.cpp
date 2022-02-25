// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/EnumHelpers.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"

#define LOCTEXT_NAMESPACE "BIMPresetInstance"

bool FBIMPresetPinAttachment::operator==(const FBIMPresetPinAttachment &OtherAttachment) const
{
	return ParentPinSetIndex == OtherAttachment.ParentPinSetIndex &&
		ParentPinSetPosition == OtherAttachment.ParentPinSetPosition &&
		PresetGUID == OtherAttachment.PresetGUID;
}

bool FBIMPresetInstance::operator==(const FBIMPresetInstance &RHS) const
{
	if (NodeType != RHS.NodeType)
	{
		return false;
	}

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

	if (ParentTagPaths.Num() != RHS.ParentTagPaths.Num())
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

	for (auto &ptp : ParentTagPaths)
	{
		if (!RHS.ParentTagPaths.Contains(ptp))
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

EBIMResult FBIMPresetInstance::ApplyDelta(const UModumateDocument* InDocument,const FModumateDatabase& InDB,const FBIMPresetEditorDelta& Delta)
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

		case EBIMPresetEditorField::AssetProperty:
		{
			FBIMPropertyKey propKey(Delta.FieldName);
			Properties.SetProperty(propKey.Scope, propKey.Name, Delta.NewStringRepresentation);
			FGuid guid;
			if (FGuid::Parse(Delta.NewStringRepresentation, guid))
			{
				SetMaterialChannelsForMesh(InDB, guid);
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

EBIMResult FBIMPresetInstance::GetForm(const UModumateDocument* InDocument,FBIMPresetForm& OutForm) const
{
	OutForm = TypeDefinition.FormTemplate;
	OutForm.Elements.Append(PresetForm.Elements);
	return UpdateFormElements(InDocument,OutForm);
}

/*
* When a new mesh is assigned to a preset with material channels:
* 1. Remove any current channels the new mesh does not carry
* 2. Add default mappings for any channels in the new mesh that the preset doesn't have
*/
EBIMResult FBIMPresetInstance::SetMaterialChannelsForMesh(const FModumateDatabase& InDB, const FGuid& InMeshGuid)
{
	FBIMPresetMaterialBindingSet bindingSet;

	if (!TryGetCustomData(bindingSet))
	{
		return EBIMResult::Error;
	}

	const FArchitecturalMesh* mesh = InDB.GetArchitecturalMeshByGUID(InMeshGuid);
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


	FGuid defaultMaterialGuid = InDB.GetDefaultMaterialGUID();
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

EBIMResult FBIMPresetInstance::UpgradeData(const FModumateDatabase& InDB, const FBIMPresetCollectionProxy& PresetCollection, int32 InDocVersion)
{
	// Prior to version 11, material bindings were a named member, not custom data
	if (InDocVersion < 11)
	{
		FGuid matGUID;

		// Prior to version 11, BIM forms were simple arrays of text/property pairs, upgrade to rich form
		if (FormItemToProperty_DEPRECATED.Num() > 0)
		{
			FBIMPresetMaterialBindingSet bindingSet;
			TryGetCustomData(bindingSet); // okay if this fails

			static const FName defaultChannel = TEXT("Finish1");

			// Deprecated map of form display text to property QN
			for (auto& fitp : FormItemToProperty_DEPRECATED)
			{
				FBIMPropertyKey propKey(fitp.Value);

				switch (propKey.Scope)
				{
					// Dimensions and meshes are properties
				case EBIMValueScope::Dimension:
					PresetForm.AddPropertyElement(FText::FromString(fitp.Key), fitp.Value, EBIMPresetEditorField::DimensionProperty);
					break;

				case EBIMValueScope::Mesh:
				case EBIMValueScope::Profile:
					PresetForm.AddPropertyElement(FText::FromString(fitp.Key), fitp.Value, EBIMPresetEditorField::AssetProperty);
					break;

					// Colors and materials require upgraded material bindings
				case EBIMValueScope::Color:
					if (bindingSet.MaterialBindings.Num() == 0)
					{
						bindingSet.MaterialBindings.AddDefaulted();
						bindingSet.MaterialBindings.Last().InnerMaterialGUID = InDB.GetDefaultMaterialGUID();
						bindingSet.MaterialBindings.Last().Channel = defaultChannel;
					}
					PresetForm.AddMaterialBindingElement(FText::FromString(fitp.Key), defaultChannel, EMaterialChannelFields::ColorTint);
					ensureAlways(Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, bindingSet.MaterialBindings.Last().ColorHexValue));
					break;

				case EBIMValueScope::RawMaterial:
					if (bindingSet.MaterialBindings.Num() == 0)
					{
						bindingSet.MaterialBindings.AddDefaulted();
						bindingSet.MaterialBindings.Last().ColorHexValue = FColor::White.ToString();
						bindingSet.MaterialBindings.Last().Channel = defaultChannel;
					}
					PresetForm.AddMaterialBindingElement(FText::FromString(fitp.Key), defaultChannel, EMaterialChannelFields::InnerMaterial);
					ensureAlways(Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, bindingSet.MaterialBindings.Last().InnerMaterialGUID));
					break;
				};
			}

			// If we got a material binding, set custom data for it
			if (bindingSet.MaterialBindings.Num() == 1)
			{
				SetCustomData(bindingSet);
			}
		}
	}

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

	// Prior to version 15, Presets only had a single custom data member, either material binding or edge detail
	if (InDocVersion < 15)
	{
		if (CustomData_DEPRECATED.IsValid())
		{
			FBIMPresetMaterialBindingSet bindingSet;
			if (CustomData_DEPRECATED.LoadStructData(bindingSet))
			{
				SetCustomData(bindingSet);
			}
			else
			{
				FEdgeDetailData edgeDetailData;
				if (CustomData_DEPRECATED.LoadStructData(edgeDetailData))
				{
					SetCustomData(edgeDetailData);
				}
			}
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

	return EBIMResult::Success;
}

#undef LOCTEXT_NAMESPACE