// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstanceFactory.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"
#include "BIMKernel/Presets/CustomData/BIMIESProfile.h"
#include "BIMKernel/Presets/CustomData/BIMMesh.h"
#include "BIMKernel/Presets/CustomData/BIMMeshRef.h"
#include "BIMKernel/Presets/CustomData/BIMNamedDimension.h"
#include "BIMKernel/Presets/CustomData/BIMPart.h"
#include "BIMKernel/Presets/CustomData/BIMPattern.h"
#include "BIMKernel/Presets/CustomData/BIMPatternRef.h"
#include "BIMKernel/Presets/CustomData/BIMPresetInfo.h"
#include "BIMKernel/Presets/CustomData/BIMProfile.h"
#include "BIMKernel/Presets/CustomData/BIMProfileRef.h"
#include "BIMKernel/Presets/CustomData/BIMRawMaterial.h"
#include "BIMKernel/Presets/CustomData/BIMSlot.h"
#include "BIMKernel/Presets/CustomData/BIMSlotConfig.h"
#include "ModumateCore/EnumHelpers.h"
#include "Quantities/QuantitiesDimensions.h"


//KLUDGE: The call to SetCustomData MUST be a value-type of the struct we're trying to save
// This is because it uses UE4 reflection to save the data, but UE4 reflection doesn't support
// polymorphism on USTRUCTs. -JN


bool FBIMPresetInstanceFactory::DeserializeCustomData(EPresetPropertyMatrixNames MatrixName,
                                                               FBIMPresetInstance& OutPreset, const FBIMWebPreset& WebPreset)
{
	switch (MatrixName)
	{
	case EPresetPropertyMatrixNames::IESLight:
		{
			FLightConfiguration data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::ConstructionCost:
		{
			FBIMConstructionCost data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::MiterPriority:
		{
			FBIMPresetLayerPriority data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::PatternRef:
		{
			FBIMPatternRef data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::ProfileRef:
		{
			FBIMProfileRef data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::MeshRef:
		{
			FBIMMeshRef data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Material:
		{
			FBIMPresetMaterialBindingSet data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Dimensions:
		{
			FBIMDimensions data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Dimension:
		{
			FBIMNamedDimension data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::RawMaterial:
		{
			FBIMRawMaterial data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Profile:
		{
			FBIMProfile data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Mesh:
		{
			FBIMMesh data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Slot:
		{
			FBIMSlot data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::IESProfile:
		{
			FBIMIESProfile data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Pattern:
		{
			FBIMPatternConfig data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::SlotConfig:
		{
			FBIMSlotConfig data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Part:
		{
			FBIMPartConfig data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Preset:
		{
			FBIMPresetInfo data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::Symbol:
		{
			FBIMSymbolPresetData data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	case EPresetPropertyMatrixNames::EdgeDetail:
		{
			FEdgeDetailData data;
			data.ConvertFromWebPreset(WebPreset);
			OutPreset.SetCustomData(data);
			return true;
		}
	default:
		break;
	}

	return false;
}

void FBIMPresetInstanceFactory::SetBlankCustomDataByPropertyName(EPresetPropertyMatrixNames MatrixName, FBIMPresetInstance& OutPreset)
{
	switch (MatrixName)
	{
	case EPresetPropertyMatrixNames::None: break;
	case EPresetPropertyMatrixNames::IESLight: OutPreset.SetCustomData(FLightConfiguration()); break;
	case EPresetPropertyMatrixNames::ConstructionCost: OutPreset.SetCustomData(FBIMConstructionCost()); break;
	case EPresetPropertyMatrixNames::MiterPriority: OutPreset.SetCustomData(FBIMPresetLayerPriority()); break;
	case EPresetPropertyMatrixNames::PatternRef: OutPreset.SetCustomData(FBIMPatternRef()); break;
	case EPresetPropertyMatrixNames::ProfileRef: OutPreset.SetCustomData(FBIMProfileRef()); break;
	case EPresetPropertyMatrixNames::MeshRef: OutPreset.SetCustomData(FBIMMeshRef()); break;
	case EPresetPropertyMatrixNames::Material: OutPreset.SetCustomData(FBIMPresetMaterialBindingSet()); break;
	case EPresetPropertyMatrixNames::Dimensions: OutPreset.SetCustomData(FBIMDimensions()); break;
	case EPresetPropertyMatrixNames::Dimension: OutPreset.SetCustomData(FBIMNamedDimension()); break;
	case EPresetPropertyMatrixNames::RawMaterial: OutPreset.SetCustomData(FBIMRawMaterial()); break;
	case EPresetPropertyMatrixNames::Profile: OutPreset.SetCustomData(FBIMProfile()); break;
	case EPresetPropertyMatrixNames::Mesh: OutPreset.SetCustomData(FBIMMesh()); break;
	case EPresetPropertyMatrixNames::Slot: OutPreset.SetCustomData(FBIMSlot()); break;
	case EPresetPropertyMatrixNames::IESProfile: OutPreset.SetCustomData(FBIMIESProfile()); break;
	case EPresetPropertyMatrixNames::Pattern: OutPreset.SetCustomData(FBIMPatternConfig()); break;
	case EPresetPropertyMatrixNames::SlotConfig: OutPreset.SetCustomData(FBIMSlotConfig()); break;
	case EPresetPropertyMatrixNames::Part: OutPreset.SetCustomData(FBIMPartConfig()); break;
	case EPresetPropertyMatrixNames::Preset: OutPreset.SetCustomData(FBIMPresetInfo()); break;
	default: ;
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
			UStruct* CData;
			if(GetCustomDataStaticStruct(matrixName, CData))
			{
				if (!Preset.HasCustomDataByName(FName(CData->GetName())))
				{
					SetBlankCustomDataByPropertyName(matrixName, Preset);
				}
			}

		}
	}
	return true;
}

bool FBIMPresetInstanceFactory::GetCustomDataStaticStruct(EPresetPropertyMatrixNames MatrixName, UStruct*& OutStruct)
{
	const TMap<EPresetPropertyMatrixNames, UStruct*> MatrixMap = {
		{EPresetPropertyMatrixNames::IESLight, FLightConfiguration::StaticStruct() },
		{EPresetPropertyMatrixNames::ConstructionCost,   FBIMConstructionCost::StaticStruct()},
		{EPresetPropertyMatrixNames::MiterPriority,  FBIMPresetLayerPriority::StaticStruct()},
		{EPresetPropertyMatrixNames::PatternRef,   FBIMPatternRef::StaticStruct()},
		{EPresetPropertyMatrixNames::ProfileRef,  FBIMProfileRef::StaticStruct()},
		{EPresetPropertyMatrixNames::MeshRef,   FBIMMeshRef::StaticStruct()},
		{EPresetPropertyMatrixNames::Material,  FBIMPresetMaterialBindingSet::StaticStruct()},
		{EPresetPropertyMatrixNames::Dimensions,   FBIMDimensions::StaticStruct()},
		{EPresetPropertyMatrixNames::Dimension,  FBIMNamedDimension::StaticStruct()},
		{EPresetPropertyMatrixNames::RawMaterial,  FBIMRawMaterial::StaticStruct()},
		{EPresetPropertyMatrixNames::Profile, FBIMProfile::StaticStruct()},
		{EPresetPropertyMatrixNames::Mesh,  FBIMMesh::StaticStruct()},
		{EPresetPropertyMatrixNames::Slot,  FBIMSlot::StaticStruct()},
		{EPresetPropertyMatrixNames::IESProfile, FBIMIESProfile::StaticStruct()},
		{EPresetPropertyMatrixNames::Pattern,  FBIMPatternConfig::StaticStruct()},
		{EPresetPropertyMatrixNames::SlotConfig,  FBIMSlotConfig::StaticStruct()},
		{EPresetPropertyMatrixNames::Part,   FBIMPartConfig::StaticStruct()},
		{EPresetPropertyMatrixNames::Preset,  FBIMPresetInfo::StaticStruct()},
	};
	
	if(MatrixMap.Contains(MatrixName))
	{
		OutStruct = MatrixMap[MatrixName];
		return true;
	}

	return false;
}
