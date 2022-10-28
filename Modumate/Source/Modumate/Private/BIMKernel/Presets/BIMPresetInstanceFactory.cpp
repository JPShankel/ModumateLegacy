// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstanceFactory.h"

#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMIESProfile.h"
#include "BIMKernel/Presets/CustomData/BIMMesh.h"
#include "BIMKernel/Presets/CustomData/BIMMeshRef.h"
#include "BIMKernel/Presets/CustomData/BIMNamedDimension.h"
#include "BIMKernel/Presets/CustomData/BIMPart.h"
#include "BIMKernel/Presets/CustomData/BIMPatternRef.h"
#include "BIMKernel/Presets/CustomData/BIMPresetInfo.h"
#include "BIMKernel/Presets/CustomData/BIMProfile.h"
#include "BIMKernel/Presets/CustomData/BIMProfileRef.h"
#include "BIMKernel/Presets/CustomData/BIMRawMaterial.h"
#include "BIMKernel/Presets/CustomData/BIMSlot.h"
#include "BIMKernel/Presets/CustomData/BIMSlotConfig.h"
#include "Quantities/QuantitiesDimensions.h"
#include "ModumateCore/EnumHelpers.h"



UStruct* FBIMPresetInstanceFactory::GetPresetMatrixStruct(EPresetPropertyMatrixNames MatrixName)
{
	switch (MatrixName)
	{
	case EPresetPropertyMatrixNames::IESLight: return FLightConfiguration::StaticStruct();
	case EPresetPropertyMatrixNames::ConstructionCost: return FBIMConstructionCost::StaticStruct();
	case EPresetPropertyMatrixNames::MiterPriority: return FBIMPresetLayerPriority::StaticStruct();
	case EPresetPropertyMatrixNames::PatternRef: return FBIMPatternRef::StaticStruct();
	case EPresetPropertyMatrixNames::ProfileRef: return FBIMProfileRef::StaticStruct();
	case EPresetPropertyMatrixNames::MeshRef: return FBIMMeshRef::StaticStruct();
	case EPresetPropertyMatrixNames::Material: return FBIMPresetMaterialBindingSet::StaticStruct();
	case EPresetPropertyMatrixNames::Dimensions: return FBIMDimensions::StaticStruct();
	case EPresetPropertyMatrixNames::Dimension: return FBIMNamedDimension::StaticStruct();
	case EPresetPropertyMatrixNames::RawMaterial: return FBIMRawMaterial::StaticStruct();
	case EPresetPropertyMatrixNames::Profile: return FBIMProfile::StaticStruct();
	case EPresetPropertyMatrixNames::Mesh: return FBIMMesh::StaticStruct();
	case EPresetPropertyMatrixNames::Slot: return FBIMSlot::StaticStruct();
	case EPresetPropertyMatrixNames::IESProfile: return FBIMIESProfile::StaticStruct();
	case EPresetPropertyMatrixNames::Pattern: return FBIMPatternRef::StaticStruct();
	case EPresetPropertyMatrixNames::SlotConfig: return FBIMSlotConfig::StaticStruct();
	case EPresetPropertyMatrixNames::Part: return FBIMPartConfig::StaticStruct();
	case EPresetPropertyMatrixNames::Preset: return FBIMPresetInfo::StaticStruct();
	case EPresetPropertyMatrixNames::Slots:
	case EPresetPropertyMatrixNames::InputPins:
	case EPresetPropertyMatrixNames::Error:
	default:
		return nullptr;
	}
}

void FBIMPresetInstanceFactory::SetBlankCustomDataByPropertyName(EPresetPropertyMatrixNames MatrixName, FBIMPresetInstance& OutPreset)
{
	switch (MatrixName)
	{
	case EPresetPropertyMatrixNames::IESLight: OutPreset.SetCustomData(FLightConfiguration());
		break;
	case EPresetPropertyMatrixNames::ConstructionCost: OutPreset.SetCustomData(FBIMConstructionCost());
		break;
	case EPresetPropertyMatrixNames::MiterPriority: OutPreset.SetCustomData(FBIMPresetLayerPriority());
		break;
	case EPresetPropertyMatrixNames::PatternRef: OutPreset.SetCustomData(FBIMPatternRef());
		break;
	case EPresetPropertyMatrixNames::ProfileRef: OutPreset.SetCustomData(FBIMProfileRef());
		break;
	case EPresetPropertyMatrixNames::MeshRef: OutPreset.SetCustomData(FBIMMeshRef());
		break;
	case EPresetPropertyMatrixNames::Material: OutPreset.SetCustomData(FBIMPresetMaterialBindingSet());
		break;
	case EPresetPropertyMatrixNames::Dimensions: OutPreset.SetCustomData(FBIMDimensions());
		break;
	case EPresetPropertyMatrixNames::Dimension: OutPreset.SetCustomData(FBIMNamedDimension());
		break;
	case EPresetPropertyMatrixNames::RawMaterial: OutPreset.SetCustomData(FBIMRawMaterial());
		break;
	case EPresetPropertyMatrixNames::Profile: OutPreset.SetCustomData(FBIMProfile());
		break;
	case EPresetPropertyMatrixNames::Mesh: OutPreset.SetCustomData(FBIMMesh());
		break;
	case EPresetPropertyMatrixNames::Slot: OutPreset.SetCustomData(FBIMSlot());
		break;
	case EPresetPropertyMatrixNames::IESProfile: OutPreset.SetCustomData(FBIMIESProfile());
		break;
	case EPresetPropertyMatrixNames::Pattern: OutPreset.SetCustomData(FBIMPatternRef());
		break;
	case EPresetPropertyMatrixNames::SlotConfig: OutPreset.SetCustomData(FBIMSlotConfig());
		break;
	case EPresetPropertyMatrixNames::Part: OutPreset.SetCustomData(FBIMPartConfig());
		break;
	case EPresetPropertyMatrixNames::Preset: OutPreset.SetCustomData(FBIMPresetInfo());
		break;
	case EPresetPropertyMatrixNames::Slots:
	case EPresetPropertyMatrixNames::InputPins:
	case EPresetPropertyMatrixNames::Error:
	default:
		break;
	}
}


bool FBIMPresetInstanceFactory::CreateBlankPreset(FBIMTagPath& TagPath, const FBIMPresetNCPTaxonomy& Taxonomy, FBIMPresetInstance& OutPreset)
{

	OutPreset.MyTagPath = TagPath;
	OutPreset.Origination = EPresetOrigination::Invented;
	OutPreset.GUID.Invalidate();
	
	TMap<FString, FBIMWebPresetPropertyDef> propTypes;
	Taxonomy.GetPropertiesForTaxonomyNode(TagPath, propTypes);
	
	// Populate default values, such as assetType, ObjectType
	if (Taxonomy.PopulateDefaults(OutPreset) == EBIMResult::Error)
	{
		return false;
	}

	TSet<FString> usedCustomData;
	for (auto& propDef : propTypes)
	{
		if (usedCustomData.Contains(propDef.Key))
		{
			continue;
		}
		FString StructName = TEXT("");
		FString PropertyName = TEXT("");
		EPresetPropertyMatrixNames matrixName = EPresetPropertyMatrixNames::Error;
		propDef.Key.Split(TEXT("."), &StructName, &PropertyName);
	
		if (FindEnumValueByName(FName(StructName), matrixName))
		{
			SetBlankCustomDataByPropertyName(matrixName, OutPreset);
		}
	}
	return true;
}

bool FBIMPresetInstanceFactory::AddMissingCustomDataToPreset(FBIMPresetInstance& Preset, const FBIMPresetNCPTaxonomy& Taxonomy)
{
	TMap<FString, FBIMWebPresetPropertyDef> propTypes;
	Taxonomy.GetPropertiesForTaxonomyNode(Preset.MyTagPath, propTypes);

	TSet<FString> checkedCustomData;
	for (auto& propDef : propTypes)
	{
		if (checkedCustomData.Contains(propDef.Key))
		{
			continue;
		}
		FString StructName = TEXT("");
		FString PropertyName = TEXT("");
		EPresetPropertyMatrixNames matrixName = EPresetPropertyMatrixNames::Error;
		propDef.Key.Split(TEXT("."), &StructName, &PropertyName);
	
		if (FindEnumValueByName(FName(StructName), matrixName))
		{
			UStruct* staticStruct = GetPresetMatrixStruct(matrixName);
			if (staticStruct != nullptr && !Preset.HasCustomDataByName(FName(staticStruct->GetName())))
			{
				SetBlankCustomDataByPropertyName(matrixName, Preset);
			}
		}
	}
	return true;
}