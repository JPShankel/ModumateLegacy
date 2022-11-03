// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"

#define LOCTEXT_NAMESPACE "ModumatePresetMaterialBindings"

EBIMResult FBIMPresetMaterialBindingSet::SetFormElements(FBIMPresetForm& RefForm) const
{
	for (int32 i=0;i<MaterialBindings.Num();++i)
	{
		auto& binding = MaterialBindings[i];

		if (binding.InnerMaterialGUID.IsValid())
		{
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("InnerMaterial", "Inner Material {0}"), i + 1), binding.Channel, EMaterialChannelFields::InnerMaterial);
		}

		if (binding.SurfaceMaterialGUID.IsValid())
		{
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("SurfaceMaterial", "Surface Material {0}"), i + 1),binding.Channel, EMaterialChannelFields::SurfaceMaterial);
		}

		if (!binding.ColorHexValue.IsEmpty())
		{
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("Tint", "Tint {0}"), i + 1), binding.Channel, EMaterialChannelFields::ColorTint);
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("TintVariation", "Tint Variation {0}"), i + 1), binding.Channel, EMaterialChannelFields::ColorTintVariation);
		}
	}

	return EBIMResult::Success;
}



void FBIMPresetMaterialBindingSet::ConvertToWebPreset(FBIMWebPreset& OutPreset)
{
	const FString PropertyKey = GetEnumValueString(EPresetPropertyMatrixNames::Material) + TEXT(".");
	TArray<FString> Channel, Inner, Surface, ColorTint, ColorTintVariation;
	for(const auto& material : MaterialBindings)
	{
		Channel.Add(material.Channel.ToString());
		Inner.Add(material.InnerMaterialGUID.ToString());
		Surface.Add(material.SurfaceMaterialGUID.ToString());
		ColorTint.Add(material.ColorHexValue);
		ColorTintVariation.Add(FString::SanitizeFloat(material.ColorTintVariationPercent));
	}
	
	OutPreset.properties.Add(PropertyKey + GetEnumValueString(EMaterialChannelFields::AppliesToChannel),
					{PropertyKey + GetEnumValueString(EMaterialChannelFields::AppliesToChannel), Channel});

	OutPreset.properties.Add(PropertyKey + GetEnumValueString(EMaterialChannelFields::InnerMaterial),
					{PropertyKey + GetEnumValueString(EMaterialChannelFields::InnerMaterial), Inner});

	OutPreset.properties.Add(PropertyKey + GetEnumValueString(EMaterialChannelFields::SurfaceMaterial),
					{PropertyKey + GetEnumValueString(EMaterialChannelFields::SurfaceMaterial), Surface});

	OutPreset.properties.Add(PropertyKey + GetEnumValueString(EMaterialChannelFields::ColorTint),
					{PropertyKey + GetEnumValueString(EMaterialChannelFields::ColorTint), ColorTint});

	OutPreset.properties.Add(PropertyKey + GetEnumValueString(EMaterialChannelFields::ColorTintVariation),
					{PropertyKey + GetEnumValueString(EMaterialChannelFields::ColorTintVariation), ColorTintVariation});
}

void FBIMPresetMaterialBindingSet::ConvertFromWebPreset(const FBIMWebPreset& InPreset)
{

	/***
	 * AppliesToChannel => [Name1][Name2][Name3]
	 * InnerMaterial    => [Material1][Material2][Material3]
	 * SurfaceMaterial  => [Material1][Material2][Material3]
	 * ColorTint        => [Color1][Color2][Color3]
	 * etc...
	 */
	const FString PropertyKey = GetEnumValueString(EPresetPropertyMatrixNames::Material) + TEXT(".");
	const FString AppliesToChannel = TEXT("AppliesToChannel");
	const FString ChannelEntry = PropertyKey + AppliesToChannel;
	
	TArray<FString> channels;
	if(InPreset.properties.Contains(ChannelEntry))
	{
		channels.Append(InPreset.properties[ChannelEntry].value);
	}

	for (int32 i = 0; i < channels.Num(); ++i)
	{
		FBIMPresetMaterialBinding materialBinding;
		for (auto propEntry : InPreset.properties)
		{
			FString propName;
			FString structName;
			
			propEntry.Key.Split(TEXT("."), &structName, &propName);
			if(!structName.Equals(GetEnumValueString(EPresetPropertyMatrixNames::Material)))
			{
				continue;
			}

			if(propEntry.Value.value.Num() <= i)
			{
				UE_LOG(LogTemp, Warning, TEXT("Invalid MaterialBinding properties, missing entry for: %s"), *propEntry.Key);
				continue;
			}
			
			EMaterialChannelFields fieldEnum = GetEnumValueByString<EMaterialChannelFields>(propName);
			
			FString value = propEntry.Value.value[i];
			
			switch (fieldEnum)
			{
			case EMaterialChannelFields::AppliesToChannel:
				materialBinding.Channel = FName(value);
				break;
			case EMaterialChannelFields::InnerMaterial:
				{
					FGuid guid;
					if (FGuid::Parse(value, guid))
					{
						materialBinding.InnerMaterialGUID = guid;
					}	
				}
				break;
			case EMaterialChannelFields::SurfaceMaterial:
				{
					FGuid guid;
					if (FGuid::Parse(value, guid))
					{
						materialBinding.SurfaceMaterialGUID = guid;
					}
				}
				break;
			case EMaterialChannelFields::ColorTint:
				{
					if (!value.IsEmpty())
					{
						materialBinding.ColorHexValue = value;
					}
				}
				break;
			case EMaterialChannelFields::ColorTintVariation:
				{
					LexTryParseString(materialBinding.ColorTintVariationPercent, *value);
				}
				break;
			default:
				break;
			}
		}

		if (!materialBinding.Channel.IsNone())
		{
			MaterialBindings.Add(materialBinding);
		}
	}
		
}

UStruct* FBIMPresetMaterialBindingSet::VirtualizedStaticStruct()
{
	return FBIMPresetMaterialBindingSet::StaticStruct();
}


bool FBIMPresetMaterialBinding::operator==(const FBIMPresetMaterialBinding& RHS) const
{
	return Channel == RHS.Channel &&
		InnerMaterialGUID == RHS.InnerMaterialGUID &&
		SurfaceMaterialGUID == RHS.SurfaceMaterialGUID &&
		ColorHexValue == RHS.ColorHexValue &&
		FMath::IsNearlyEqual(ColorTintVariationPercent,RHS.ColorTintVariationPercent);
}

bool FBIMPresetMaterialBinding::operator!=(const FBIMPresetMaterialBinding& RHS) const
{
	return !(*this == RHS);
}

EBIMResult FBIMPresetMaterialBinding::GetEngineMaterial(const FBIMPresetCollectionProxy& PresetCollection, FArchitecturalMaterial& OutMaterial) const
{
	FGuid matGuid = SurfaceMaterialGUID.IsValid() ? SurfaceMaterialGUID : InnerMaterialGUID;
	if (!matGuid.IsValid())
	{
		return EBIMResult::Error;
	}

	const FArchitecturalMaterial* material = PresetCollection.GetArchitecturalMaterialByGUID(matGuid);
	if (material == nullptr)
	{
		return EBIMResult::Error;
	}

	OutMaterial = *material;
	OutMaterial.Color = ColorHexValue.IsEmpty() ? FColor::White : FColor::FromHex(ColorHexValue);
	return EBIMResult::Success;
}

#undef LOCTEXT_NAMESPACE