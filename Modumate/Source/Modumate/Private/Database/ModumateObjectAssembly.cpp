// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectAssembly.h"

#include "ExpressionEvaluator.h"
#include "ModumateDocument.h"
#include "ModumateDecisionTree.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateDimensionStatics.h"
#include "ModumateCommands.h"
#include "EditModelGameState_CPP.h"
#include "ModumateObjectDatabase.h"
#include "Algo/Accumulate.h"
#include "Algo/Reverse.h"

using namespace Modumate;
using namespace Modumate::Units;

const FName FPortalConfiguration::RefPlaneNameMinX(TEXT("RXLeft"));
const FName FPortalConfiguration::RefPlaneNameMaxX(TEXT("RXRight"));
const FName FPortalConfiguration::RefPlaneNameMinZ(TEXT("RZBottom"));
const FName FPortalConfiguration::RefPlaneNameMaxZ(TEXT("RZTop"));

bool FPortalConfiguration::IsValid() const
{
	return (PortalFunction != EPortalFunction::None) && (Slots.Num() > 0) &&
		!PartSet.Key.IsNone() && (PartSet.PartsBySlotType.Num() > 0) &&
		ReferencePlanes.Contains(FPortalConfiguration::RefPlaneNameMinX) &&
		ReferencePlanes.Contains(FPortalConfiguration::RefPlaneNameMaxX) &&
		ReferencePlanes.Contains(FPortalConfiguration::RefPlaneNameMinZ) &&
		ReferencePlanes.Contains(FPortalConfiguration::RefPlaneNameMaxZ);
}

void FPortalConfiguration::CacheRefPlaneValues()
{
	TSet<FName> planeNamesToEvaluate;

	// First, cache the fixed values and clear out the evaluated values
	for (const auto &kvp : ReferencePlanes)
	{
		const Modumate::FPortalReferencePlane &refPlane = kvp.Value;
		if (refPlane.ValueExpression.IsEmpty())
		{
			CachedDimensions.Emplace(refPlane.Name, refPlane.FixedValue);
		}
		else
		{
			CachedDimensions.Remove(refPlane.Name);
			planeNamesToEvaluate.Add(refPlane.Name);
		}
	}

	while (planeNamesToEvaluate.Num() > 0)
	{
		bool bEvaluatedAnyPlane = false;

		for (const auto &kvp : ReferencePlanes)
		{
			const Modumate::FPortalReferencePlane &refPlane = kvp.Value;
			if (!refPlane.ValueExpression.IsEmpty() && !CachedDimensions.Contains(refPlane.Name))
			{
				Modumate::Units::FUnitValue evaluatedValue;
				if (Modumate::Expression::Evaluate(CachedDimensions, refPlane.ValueExpression, evaluatedValue))
				{
					CachedDimensions.Add(refPlane.Name, evaluatedValue);
					planeNamesToEvaluate.Remove(refPlane.Name);
					bEvaluatedAnyPlane = true;
				}
			}
		}

		if (!ensure(bEvaluatedAnyPlane))
		{
			for (const FName &planeNameToEvaluate : planeNamesToEvaluate)
			{
				const Modumate::FPortalReferencePlane &refPlane = ReferencePlanes[planeNameToEvaluate];
				UE_LOG(LogTemp, Error, TEXT("Failed to evaluate ref plane \"%s\" expression: \"%s\""),
					*refPlane.Name.ToString(), *refPlane.ValueExpression);
			}
		}
	}
}

void FModumateObjectAssembly::FillSpec(BIM::FModumateAssemblyPropertySpec &spec) const
{
	spec.RootProperties = Properties;
	spec.ObjectType = ObjectType;
	spec.LayerProperties.Empty(Layers.Num());
	for (auto &l : Layers)
	{
		spec.LayerProperties.Add(l.Properties);
	}
	spec.RootPreset = RootPreset;
}

bool FModumateObjectAssembly::UsesPreset_DEPRECATED(const FName &presetKey) const
{
	for (auto &l : Layers)
	{
		if (l.PresetKey == presetKey)
		{
			return true;
		}
	}
	return false;
}

void FModumateObjectAssembly::ReplacePreset_DEPRECATED(const FName &oldPreset, const FName &newPreset)
{
	for (auto &l : Layers)
	{
		if (l.PresetKey == oldPreset)
		{
			l.PresetKey = newPreset;
			l.Properties.SetProperty(BIM::EScope::Layer, BIM::Parameters::Preset, newPreset.ToString());
		}
	}
}

FString FModumateObjectAssembly::GetGenomeString() const
{
	FString result;
	static const FString geneSeparator(TEXT("|"));
	FCustomAssemblyRecord record = ToDataRecord();
	for (const FCustomAssemblyProperty &property : record.PropertySheet)
	{
		result += property.ToCompactString() + geneSeparator;
	}
	return result;
}

FShoppingItem FModumateObjectAssembly::AsShoppingItem() const
{
	FShoppingItem ret;
	ret.DisplayName = GetProperty(BIM::Parameters::Name);

	ret.Key = DatabaseKey;
	ret.Format = ELayerFormat::None;

	// TODO: was once determined by customization state, deprecated
	ret.Locked = false;

	if (Layers.Num() > 0)
	{
		if (Layers[0].Modules.Num() > 0)
		{
			ret.EngineMaterial = Layers[0].Material.EngineMaterial;
		}
		else
		{
			ret.EngineMaterial = nullptr;
		}
		ret.EngineMesh = Layers[0].Mesh.EngineMesh;
	}
	else
	{
		ret.Icon = nullptr;
		ret.EngineMaterial = nullptr;
		ret.EngineMesh = nullptr;
	}

	ret.Thickness = CalculateThickness().AsWorldCentimeters();
	if (PortalConfiguration.IsValid())
	{
		ret.FormattedKey = GetProperty(BIM::Parameters::Code);
	}
	else
	{
		ret.FormattedKey = Algo::TransformAccumulate(
			Layers,
			[](const FModumateObjectAssemblyLayer &l)
			{
				return l.CodeName;
			},
			FString()
		);
	}

	return ret;
}

FShoppingItem FModumateObjectAssemblyLayer::AsShoppingItem() const
{
	FShoppingItem ret;
	ret.DisplayName = DisplayName;
	ret.Key = DatabaseKey;
	ret.Format = Format;
	ret.Function = Function;
	ret.Thickness = Thickness.AsWorldCentimeters();
	ret.EngineMesh = Mesh.EngineMesh;
	ret.EngineMaterial = Material.EngineMaterial;
	ret.MaterialCode = CodeName;
	// TODO: was once bound to customization state, deprecated
	ret.Locked = false;
	ret.Type = TEXT("Unknown Type");

	if (Modules.Num() > 0)
	{
		ret.ModuleDisplayName = Modules[0].DisplayName;
		ret.Extents = Modules[0].ModuleExtents;
	}

	//TODO: Pattern metadata fields currently read from table will be migrated to presets
	ret.PatternDisplayName = Pattern.DisplayName;
	ret.GapDisplayName = Gap.DisplayName;

	if (!Gap.Key.IsNone())
	{
		ret.GapDisplayName = Gap.DisplayName;
	}
	else
	{
		ret.GapDisplayName = FText::GetEmpty();
	}

	if (SubcategoryDisplayNames.Num() > 0)
	{
		ret.C2Subcategory = SubcategoryDisplayNames[0];
		if (SubcategoryDisplayNames.Num() > 1)
		{
			ret.C4Subcategory = SubcategoryDisplayNames[1];
		}
		else
		{
			ret.C4Subcategory = ret.C2Subcategory;
		}
	}
	else
	{
		ret.C2Subcategory.Empty();
		ret.C4Subcategory.Empty();
	}

	return ret;
}

Modumate::FModumateCommandParameter FModumateObjectAssemblyLayer::GetProperty(const Modumate::BIM::EScope &scope, const Modumate::BIM::FNameType &name) const
{
	return Properties.GetProperty(scope, name);
}

Modumate::Units::FUnitValue FModumateObjectAssembly::CalculateThickness() const
{
	return Modumate::Units::FUnitValue::WorldCentimeters(Algo::TransformAccumulate(
		Layers,
		[](const FModumateObjectAssemblyLayer &l)
		{
			return l.Thickness.AsWorldCentimeters();
		},
		0.0f,
		[](float lhs, float rhs)
		{
			return lhs + rhs;
		}
	));
}

Modumate::FModumateCommandParameter FModumateObjectAssembly::GetProperty(const Modumate::BIM::FNameType &name) const
{
	return Properties.GetProperty(BIM::EScope::Assembly, name);
}

void FModumateObjectAssembly::SetProperty(const Modumate::BIM::FNameType &name, const Modumate::FModumateCommandParameter &val)
{
	Properties.SetProperty(BIM::EScope::Assembly, name, val);
}

bool FModumateObjectAssembly::HasProperty(const Modumate::BIM::FNameType &name) const
{
	return Properties.HasProperty(BIM::EScope::Assembly, name);
}


FCustomAssemblyRecord FModumateObjectAssembly::ToDataRecord() const
{
	FCustomAssemblyRecord ar;
	ar.ObjectType = ObjectType;
	ar.DisplayName = GetProperty(BIM::Parameters::Name);
	ar.DatabaseKey = DatabaseKey.ToString();

	BIM::FModumateAssemblyPropertySpec spec;
	FillSpec(spec);

	ar.RootPreset = spec.RootPreset;

	spec.RootProperties.ForEachProperty([&ar](const FString &name, const FModumateCommandParameter &param)
	{
		FCustomAssemblyProperty &prop = ar.PropertySheet.AddDefaulted_GetRef();
		prop.LayerID = -1;
		prop.PropertyName = name;
		prop.PropertyValueJSON = param.AsJSON();
	});

	for (int32 i = 0; i < spec.LayerProperties.Num(); ++i)
	{
		auto &l = spec.LayerProperties[i];
		l.ForEachProperty([&ar,i](const FString &name, const FModumateCommandParameter &param) {
			FCustomAssemblyProperty &prop = ar.PropertySheet.AddDefaulted_GetRef();
			prop.LayerID = i;
			prop.PropertyName = name;
			prop.PropertyValueJSON = param.AsJSON();
		});
	}

	return ar;
}

bool FModumateObjectAssembly::ToParameterSet_DEPRECATED(Modumate::FModumateFunctionParameterSet &params) const
{
	BIM::FModumateAssemblyPropertySpec spec;
	FillSpec(spec);

	spec.RootProperties.ForEachProperty([&params](const FString &name, const FModumateCommandParameter &param)
	{
		params.SetValue(FString::Printf(TEXT("-1:%s"), *name), param);
	});

	for (int32 i = 0; i < spec.LayerProperties.Num(); ++i)
	{
		auto &l = spec.LayerProperties[i];
		l.ForEachProperty([i, &params](const FString &name, const FModumateCommandParameter &param)
		{
			params.SetValue(FString::Printf(TEXT("%d:%s"), i, *name), param);
		});
	}

	FName objectTypeName = FindEnumValueFullName<EObjectType>(TEXT("EObjectType"), ObjectType);
	params.SetValue(Parameters::kObjectType, objectTypeName);
	params.SetValue(Parameters::kAssembly, DatabaseKey);
	params.SetValue(Parameters::kPresetKey, spec.RootPreset);
	return true;
}

class MODUMATE_API FLayerMaker
{
public:
	TArray<FName> Subcategories;
	FString CodeName;

	ELayerFunction FunctionEnum = ELayerFunction::None;
	FText FunctionName;

	ELayerFormat FormatEnum = ELayerFormat::None;
	FText FormatName;

	FUnitValue Thickness;

	FString LayerMaterialKey;
	FString LayerColorKey;

	FLayerPattern Pattern;
	FLayerPatternModule Module;
	FString ModuleMaterialKey;
	FString ModuleColorKey;

	FLayerPatternGap Gap;
	FString GapMaterialKey;
	FString GapColorKey;

	FName ProfileKey;

	FCraftingPreset Preset;

	BIM::FBIMPropertySheet Properties;
	TMap<Modumate::BIM::FNameType, FModumateFormattedDimension> Dimensions;

	void SetValue(
		const BIM::FValueSpec &var,
		const FModumateCommandParameter &value)
	{
		auto convertDimension = [this](const FString &dimStr,const BIM::FNameType &property)
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
			Dimensions.Add(property, dim);
			return FUnitValue::WorldCentimeters(dim.Centimeters);
		};

		Properties.SetValue(var.QN().ToString(), value);

		//TODO: deprecated DDL 1.0 .. DDL 2 tracks all layer values in the Module or Layer scope
		if (var.Scope == BIM::EScope::MaterialColor)
		{
			if (var.Name == BIM::Parameters::Color)
			{
				LayerColorKey = value;
			}
			else if (var.Name == BIM::Parameters::Material)
			{
				LayerMaterialKey = value;
			}
		}
		else if (var.Scope == BIM::EScope::Layer)
		{
			if (var.Name == BIM::Parameters::Color)
			{
				LayerColorKey = value;
			}
			else if (var.Name == BIM::Parameters::Material)
			{
				LayerMaterialKey = value;
			}
			else if (var.Name == BIM::Parameters::TrimProfile)
			{
				ProfileKey = value;
			}
			else if (var.Name == BIM::Parameters::Thickness)
			{
				Thickness = convertDimension(value,var.QN());
			}
			else if (var.Name == BIM::Parameters::Pattern)
			{
				Pattern.Key = *value.AsString();
			}
			else if (var.Name == BIM::Parameters::Function)
			{
				if (!ensureAlways(TryFindEnumValueByName<ELayerFunction>(TEXT("ELayerFunction"), value.AsName(), FunctionEnum)))
				{
					FunctionEnum = ELayerFunction::None;
				}
			}
		}
		else if (var.Scope == BIM::EScope::Module)
		{
			if (var.Name == BIM::Parameters::Material)
			{
				ModuleMaterialKey = value;
				LayerMaterialKey = ModuleMaterialKey;
			}
			else if (var.Name == BIM::Parameters::Color)
			{
				ModuleColorKey = value;
				LayerColorKey = ModuleColorKey;
			}
			else if (var.Name == BIM::Parameters::Height || var.Name == BIM::Parameters::ZExtents)
			{
				Module.ModuleExtents.Z = convertDimension(value, var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::Width || var.Name == BIM::Parameters::YExtents || var.Name == BIM::Parameters::Depth)
			{
				Module.ModuleExtents.Y = convertDimension(value, var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::Length || var.Name == BIM::Parameters::XExtents)
			{
				Module.ModuleExtents.X = convertDimension(value, var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::BevelWidth)
			{
				Module.BevelWidth = convertDimension(value, var.QN());
			}
		}
		else if (var.Scope == BIM::EScope::Gap)
		{
			if (var.Name == BIM::Parameters::Color)
			{
				GapColorKey = value;
			}
			else if (var.Name == BIM::Parameters::Material)
			{
				GapMaterialKey = value;
			}
			else if (var.Name == BIM::Parameters::Name)
			{
				Gap.DisplayName = FText::FromString(value);
			}
			else if (var.Name == BIM::Parameters::Width || var.Name == BIM::Parameters::XExtents)
			{
				Gap.GapExtents.X = convertDimension(value,var.QN()).AsWorldCentimeters();
			}
			else if (var.Name == BIM::Parameters::Depth || var.Name == BIM::Parameters::YExtents)
			{
				Gap.GapExtents.Y = convertDimension(value,var.QN()).AsWorldCentimeters();
			}
		}
		else if (var.Scope == BIM::EScope::Pattern)
		{
			if (var.Name == BIM::Parameters::Name)
			{
				Pattern.DisplayName = FText::FromString(value.AsString());
			}
			else if (var.Name == BIM::Parameters::ModuleCount)
			{
				Pattern.ModuleCount = value;
			}
			else if (var.Name == BIM::Parameters::Extents)
			{
				Pattern.ParameterizedExtents = value;
			}
		}
	}

	FModumateObjectAssemblyLayer Make(const Modumate::ModumateObjectDatabase &db)
	{
		FModumateObjectAssemblyLayer ret;

		ret.Format = FormatEnum;
		ret.FormatDisplayName = FormatName;
		ret.Function = FunctionEnum;
		ret.FunctionDisplayName = FunctionName;

		ret.Properties = Properties;
		ret.Thickness = Thickness;

		ret.DisplayName = CodeName;

		ret.CodeName = CodeName;
		ret.PresetSequence = Preset.SequenceCode.ToString();
		ret.PresetKey = Preset.Key;

		ret.Modules.Add(Module);
		ret.Gap = Gap;
		ret.Pattern = Pattern;

		for (auto &sc : Subcategories)
		{
			const Modumate::FCraftingSubcategoryData *scd = db.CraftingSubcategoryData.GetData(sc);
			if (ensureAlways(scd != nullptr))
			{
				ret.SubcategoryDisplayNames.Add(scd->DisplayName.ToString());
			}
		}

		if (!ProfileKey.IsNone())
		{
			ret.Function = ELayerFunction::Finish;
			ret.Format = ELayerFormat::Board;

			//TODO: legacy trim were keyed based on their display name, so check against both name and key

			const FSimpleMeshRef *trimMesh = nullptr;
			for (auto &kvp : db.ProfileOptionSets.DataMap)
			{
				for (auto &option : kvp.Value.Options)
				{
					if (*option.DisplayName.ToString() == ProfileKey || option.Key == ProfileKey)
					{
						trimMesh = &option.ProfileMesh;
					}
				}
			}

			if (ensureAlways(trimMesh != nullptr))
			{
				ret.SimpleMeshes.Add(*trimMesh);
			}
		}
		if (!LayerMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *mat = db.GetArchitecturalMaterialByKey(*LayerMaterialKey);
			ensureAlways(mat != nullptr);
			if (mat != nullptr)
			{
				ret.Material = *mat;
				ensureAlways(ret.Material.EngineMaterial != nullptr);
			}
		}

		if (!ModuleMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *modMat = db.GetArchitecturalMaterialByKey(*ModuleMaterialKey);
			ensureAlways(modMat != nullptr);
			ensureAlways(ret.Modules.Num() > 0);
			for (auto &m : ret.Modules)
			{
				m.Material = *modMat;
				ensureAlways(m.Material.EngineMaterial != nullptr);
			}
		}

		if (!GapMaterialKey.IsEmpty())
		{
			const FArchitecturalMaterial *gapMat = db.GetArchitecturalMaterialByKey(*GapMaterialKey);
			ensureAlways(gapMat != nullptr);
			if (gapMat != nullptr)
			{
				ret.Gap.Material = *gapMat;
				ensureAlways(ret.Gap.Material.EngineMaterial != nullptr);
			}
		}

		auto tryAssignColor = [&db](FCustomColor &target, const FString &key)
		{
			const FCustomColor *color = db.GetCustomColorByKey(*key);
			ensureAlways(color != nullptr);
			if (color != nullptr)
			{
				target = *color;
			}
		};

		if (!LayerColorKey.IsEmpty())
		{
			tryAssignColor(ret.BaseColor, LayerColorKey);
			tryAssignColor(ret.Material.DefaultBaseColor, LayerColorKey);
		}

		if (!ModuleColorKey.IsEmpty())
		{
			for (auto &m : ret.Modules)
			{
				tryAssignColor(m.Material.DefaultBaseColor, ModuleColorKey);
			}
		}

		if (!GapColorKey.IsEmpty())
		{
			tryAssignColor(ret.Gap.BaseColor, GapColorKey);
		}

		if (ret.Material.EngineMaterial == nullptr &&
			ret.Modules.Num() > 0 &&
			ret.Modules[0].Material.EngineMaterial != nullptr)
		{
			ret.Material = ret.Modules[0].Material;
		}

		if (ret.Pattern.Key.IsNone())
		{
			ret.Pattern.Key = ret.Modules.Num() == 0 ? TEXT("Continuous") : TEXT("PATKEY");
		}
		else
		{
		// TODO: DDL 2 will define patterns for itself, but in the interim patterns are tied to DDL 1.0 subcategory keyed data
			bool found = false;
			for (const auto &optionSet : db.PatternOptionSets.DataMap)
			{
				for (const auto &option : optionSet.Value.Options)
				{
					if (option.Key == ret.Pattern.Key)
					{
						ret.Pattern.InitFromCraftingParameters(option.CraftingParameters);
						found = true;
						break;
					}
				}
				if (found)
				{
					break;
				}
			}
		}


		if (ret.Gap.Key.IsNone())
		{
			ret.Gap.Key = TEXT("GAPKEY");
		}

		for (auto &m : ret.Modules)
		{
			if (m.Key.IsNone())
			{
				m.Key = TEXT("MODKEY");
			}
		}
		if (ret.BaseColor.Key.IsNone())
		{
			ret.BaseColor.Key = TEXT("COLORKEY");
		}

		ret.Pattern.DefaultGap = ret.Gap;
		ret.Pattern.DefaultModules = ret.Modules;

		ensureAlways(ret.Pattern.ModuleCount == ret.Modules.Num());

		return ret;
	}
};

// TODO: break out parameter validation and assembly construction to each object type rather than relying on the assembly class to know everyone's business
bool FModumateObjectAssembly::FromCraftingProperties_DEPRECATED(
	EObjectType ot,
	const Modumate::ModumateObjectDatabase &db,
	const Modumate::FPresetManager &presetManager,
	const BIM::FModumateAssemblyPropertySpec &spec,
	FModumateObjectAssembly &outMOA,
	const int32 showOnlyLayerID)
{
	outMOA = FModumateObjectAssembly();
	outMOA.ObjectType = ot;

	outMOA.Properties = spec.RootProperties;
	outMOA.RootPreset = spec.RootPreset;

	auto assignSubcategory = [&db](FLayerMaker &layerMaker,const FString &catKey)
	{
		layerMaker.Subcategories.Add(*catKey);
		const Modumate::FCraftingSubcategoryData *subCat = db.CraftingSubcategoryData.GetData(*catKey);
		if (subCat != nullptr)
		{
			if (!subCat->IDCodeLine1.IsEmpty())
			{
				layerMaker.CodeName = subCat->IDCodeLine1;
			}
			if (subCat->LayerFunction != ELayerFunction::None)
			{
				layerMaker.FunctionEnum = subCat->LayerFunction;
				layerMaker.FunctionName = subCat->DisplayName;
			}
			if (subCat->LayerFormat != ELayerFormat::None)
			{
				layerMaker.FormatEnum = subCat->LayerFormat;
				layerMaker.FormatName = subCat->DisplayName;
			}
		}
	};

	for (const auto &l : spec.LayerProperties)
	{
		FString portalSub;

		// If this is a portal, fill in the portal configuration data and bail...no need to add any layers
		if (l.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::Subcategory, portalSub))
		{
			// portals have all their data in a single layer sheet
			ensureAlways(spec.LayerProperties.Num() == 1);

			//TODO: portal assemblies need a 'layer' so their properties can be serialized in the same format as the crafting tree stores them
			//to be refactored when the correspondence between slots and layers is resolved
			FModumateObjectAssemblyLayer &layer = outMOA.Layers.AddDefaulted_GetRef();
			layer.Properties = spec.LayerProperties[0];

			const FPortalAssemblyConfigurationOptionSet *configSet = db.PortalConfigurationOptionSets.GetData(*portalSub);
			FString configKey;
			if (ensureAlways(configSet != nullptr && l.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::Configuration,configKey)))
			{
				const FPortalAssemblyConfigurationOption *config = nullptr;
				for (const auto &configOption : configSet->Options)
				{
					if (configOption.Key.ToString() == configKey)
					{
						config = &configOption;
						break;
					}
				}

				// Function and default reference planes
				if (ensureAlways(config != nullptr))
				{
					outMOA.PortalConfiguration.DisplayName = config->DisplayName;
					outMOA.PortalConfiguration.PortalFunction = config->PortalFunction;
					for (auto &refPlaneSet: config->ReferencePlanes)
					{
						for (auto &rp : refPlaneSet)
						{
							outMOA.PortalConfiguration.ReferencePlanes.Add(rp.Name, rp);
						}
					}
					outMOA.PortalConfiguration.Slots = config->Slots;
				}

				// Width and height are overridden in corresponding reference planes
				FString widthKey, heightKey;
				if (l.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::Width, widthKey) 
					&& l.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::Height, heightKey))
				{
					auto setDimension = [&outMOA](const TArray<FPortalConfigDimensionSet> &dims, const FString &dimKey)
					{
						for (auto &dim : dims)
						{
							if (dim.Key.ToString() == dimKey)
							{
								for (auto &kvp : dim.DimensionMap)
								{
									FPortalReferencePlane *prp = outMOA.PortalConfiguration.ReferencePlanes.Find(kvp.Key);
									if (ensureAlways(prp != nullptr))
									{
										prp->FixedValue = kvp.Value;
									}
								}
								return true;
							}
						}
						return false;
					};

					ensureAlways(setDimension(config->SupportedHeights, heightKey));
					ensureAlways(setDimension(config->SupportedWidths, widthKey));
				}

				// Get the select part set and update any reference planes that are affected
				FString partKey;

				//TODO: portal part sets to be refactored, add convenience function in database if this format survives
				const FCraftingPortalPartOptionSet *partSet = db.PortalPartOptionSets.GetData(*portalSub);
				if (ensureAlways(partSet != nullptr && l.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::PartSet, partKey)))
				{
					for (const auto &partOption : partSet->Options)
					{
						if (partOption.Key.ToString() == partKey)
						{
							outMOA.PortalConfiguration.PartSet = partOption;
							break;
						}
					}

					// TODO: borrowed/stolen from previous version (makeBasicPortal) which is deprecated
					auto cacheSlotDimension = [&outMOA](const FString &slotTypeString, const FName &key, const Modumate::Units::FUnitValue &value)
					{
						FName dimensionKey(*FString::Printf(TEXT("%s.%s"), *slotTypeString, *key.ToString()));
						if (!outMOA.PortalConfiguration.CachedDimensions.Contains(dimensionKey))
						{
							outMOA.PortalConfiguration.CachedDimensions.Add(dimensionKey, value);
						}
					};

					for (int32 slotIdx = 0; slotIdx < outMOA.PortalConfiguration.Slots.Num(); ++slotIdx)
					{
						Modumate::FPortalAssemblyConfigurationSlot &slot = outMOA.PortalConfiguration.Slots[slotIdx];
						FName *partForSlot = outMOA.PortalConfiguration.PartSet.PartsBySlotType.Find(slot.Type);
						const FPortalPart *portalPart = (partForSlot && !partForSlot->IsNone()) ?
							db.GetPortalPartByKey(*partForSlot) : nullptr;

						if (portalPart)
						{
							// Also, cache configuration dimensions based on the first type of slot that specifies them.
							FString slotTypeString = EnumValueString(EPortalSlotType, slot.Type);
							for (auto &kvp : portalPart->ConfigurationDimensions)
							{
								cacheSlotDimension(slotTypeString, kvp.Key, kvp.Value);
							}

							static const FName sizeNameX(TEXT("NativeSizeX"));
							static const FName sizeNameY(TEXT("NativeSizeY"));
							static const FName sizeNameZ(TEXT("NativeSizeZ"));
							cacheSlotDimension(slotTypeString, sizeNameX, portalPart->NativeSizeX);
							cacheSlotDimension(slotTypeString, sizeNameY, portalPart->NativeSizeY);
							cacheSlotDimension(slotTypeString, sizeNameZ, portalPart->NativeSizeZ);

							// TODO: support cabinet frames once we need to actually bore holes in cabinet boxes and animate doors.
							if ((outMOA.ObjectType != EObjectType::OTCabinet) || (slot.Type != EPortalSlotType::Frame))
							{
								outMOA.PortalParts.Add(slotIdx, *portalPart);
							}
						}
					}
					outMOA.PortalConfiguration.CacheRefPlaneValues();
				}

				// Cabinet toe-kicks
				FString toekickHeightStr, toekickDepthStr;
				if (l.TryGetProperty(BIM::EScope::ToeKick, BIM::Parameters::Height, toekickHeightStr))
				{
					outMOA.Properties.SetProperty(BIM::EScope::ToeKick, BIM::Parameters::Height, toekickHeightStr);
				}
				if (l.TryGetProperty(BIM::EScope::ToeKick, BIM::Parameters::Depth, toekickDepthStr))
				{
					outMOA.Properties.SetProperty(BIM::EScope::ToeKick, BIM::Parameters::Depth, toekickDepthStr);
				}

				// TODO: Material channels are to be reviewed, in the meantime they are each a scope

				const TArray<BIM::EScope> finishEnums = 
				{
					BIM::EScope::Interior_Finish,
					BIM::EScope::Exterior_Finish,
					BIM::EScope::Glass_Finish,
					BIM::EScope::Frame_Finish,
					BIM::EScope::Hardware_Finish,
					BIM::EScope::Cabinet_Interior_Finish,
					BIM::EScope::Cabinet_Exterior_Finish,
					BIM::EScope::Cabinet_Glass_Finish,
					BIM::EScope::Cabinet_Hardware_Finish
				};

				for (auto &finishEnum : finishEnums)
				{
					FString colorName, materialName;
					if (l.TryGetProperty(finishEnum, BIM::Parameters::Color, colorName) &&
						l.TryGetProperty(finishEnum, BIM::Parameters::Material, materialName))
					{
						const FArchitecturalMaterial *channelMat = db.GetArchitecturalMaterialByKey(*materialName);
						const FCustomColor *channelColor = db.GetCustomColorByKey(*colorName);
						if (ensureAlways(channelMat && channelColor))
						{
							FArchitecturalMaterial customChannelMat = *channelMat;
							customChannelMat.DefaultBaseColor = *channelColor;
							outMOA.PortalConfiguration.MaterialsPerChannel.Add(BIM::NameFromScope(finishEnum), MoveTemp(customChannelMat));
						}
					}
				}
			}
		}
		else
		{
			// Get new maker and reset params
			FLayerMaker layerMaker;

			BIM::FBIMPropertySheet props;

			// If the property sheet specifies a preset, use its values
			FString presetName;
			if (l.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Preset, presetName))
			{
				if (presetManager.TryGetPresetPropertiesRecurse(*presetName, props))
				{
					const FCraftingPreset *pPreset = presetManager.GetPresetByKey(*presetName);
					props.SetProperty(BIM::EScope::Layer, BIM::Parameters::Preset, *presetName);
					if (ensureAlways(pPreset != nullptr))
					{
						layerMaker.Preset = *pPreset;
					}
				}
				else
				{
					props = l;
				}
			}
			else
			{
				props = l;
			}


			FString subCatKey;
			if (props.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Category, subCatKey))
			{
				assignSubcategory(layerMaker, subCatKey);
			}

			if (props.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Subcategory, subCatKey))
			{
				assignSubcategory(layerMaker, subCatKey);
			}

			// Set pattern data
			if (props.HasProperty(BIM::EScope::Pattern, BIM::Parameters::ModuleCount))
			{
				layerMaker.Pattern.InitFromCraftingParameters(props);
			}

			props.ForEachProperty([&layerMaker](const FString &propName, const FModumateCommandParameter &val)
			{
				if (!propName.IsEmpty())
				{
					layerMaker.SetValue(BIM::FValueSpec(*propName), val);
				}
			});

			FModumateObjectAssemblyLayer &layer = outMOA.Layers.Add_GetRef(layerMaker.Make(db));
		}
	}
	if (showOnlyLayerID != -1 && outMOA.Layers.Num() > showOnlyLayerID)
	{
		FModumateObjectAssembly filteredMOA;
		filteredMOA.Layers.Add_GetRef(outMOA.Layers[showOnlyLayerID]);
		outMOA = filteredMOA;
	}
	return true;
}

bool FModumateObjectAssembly::FromDataRecord_DEPRECATED(
	const FCustomAssemblyRecord &record,
	const Modumate::ModumateObjectDatabase &objectDB,
	const FPresetManager &presetManager,
	FModumateObjectAssembly &outMOA)
{
	if (record.ObjectType != EObjectType::OTFurniture)
	{
		/*
		TODO: explicit property sheets in data records are deprecated (DDL 1.0)
		DLL 2.0 assemblies are identified solely by their root preset
		*/
		if (!record.RootPreset.IsNone())
		{
			BIM::FModumateAssemblyPropertySpec spec;
			presetManager.PresetToSpec(record.RootPreset, spec);
			if (UModumateObjectAssemblyStatics::DoMakeAssembly(objectDB, presetManager, spec, outMOA) == ECraftingResult::Success)
			{
				outMOA.DatabaseKey = *record.DatabaseKey;
				return true;
			}
			return false;
		}
		else
		if (ensureAlways(record.PropertySheet.Num() > 0))
		{
			BIM::FModumateAssemblyPropertySpec spec;
			spec.ObjectType = record.ObjectType;
			for (auto &ps : record.PropertySheet)
			{
				FModumateCommandParameter param;
				param.FromJSON(ps.PropertyValueJSON);
				if (ps.LayerID == -1)
				{
					spec.RootProperties.SetValue(ps.PropertyName, param);
				}
				else
				{
					while (ps.LayerID >= spec.LayerProperties.Num())
					{
						spec.LayerProperties.AddDefaulted();
					}
					spec.LayerProperties[ps.LayerID].SetValue(ps.PropertyName, param);
				}
			}
			if (UModumateObjectAssemblyStatics::DoMakeAssembly(objectDB, presetManager, spec, outMOA) == ECraftingResult::Success)
			{
				outMOA.DatabaseKey = *record.DatabaseKey;
				return true;
			}
			return false;
		}
		return false;
	}
	else
	{
		const DataCollection<FModumateObjectAssembly> *ffeDB = presetManager.AssemblyDBs_DEPRECATED.Find(EToolMode::VE_PLACEOBJECT);
		const FModumateObjectAssembly *originalAsm = ffeDB->GetData(*record.DatabaseKey);
		if (originalAsm == nullptr)
		{
			originalAsm = objectDB.PresetManager.GetAssemblyByKey(EToolMode::VE_PLACEOBJECT,*record.DatabaseKey);
		}
		if (ensureAlways(originalAsm))
		{
			// TODO: fix layered assembly code names here, using a better scheme than custom assembly properties
			outMOA = *originalAsm;
			return true;
		}
		return false;
	}
}

void FModumateObjectAssembly::GatherPresets_DEPRECATED(const Modumate::FPresetManager &presetManager, TArray<FName> &presetKeys) const
{
	BIM::FModumateAssemblyPropertySpec spec;
	FillSpec(spec);

	auto gatherPresets = [&presetKeys, presetManager](const BIM::FBIMPropertySheet &ps)
	{
		FName presetKey;
		if (ps.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Preset, presetKey))
		{
			presetKeys.AddUnique(presetKey);
			presetManager.GetChildPresets(presetKey, presetKeys);
		}
	};

	gatherPresets(spec.RootProperties);
	for (auto &lp : spec.LayerProperties)
	{
		gatherPresets(lp);
	}
}

void FModumateObjectAssembly::InvertLayers()
{
	Algo::Reverse(Layers);
}

bool UModumateObjectAssemblyStatics::CheckCanMakeAssembly(
	EObjectType OT,
	const Modumate::ModumateObjectDatabase &InDB,
	const BIM::FModumateAssemblyPropertySpec &InSpec)
{
	if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(OT))
	{
		// TODO: Generalize required property specification and move check functions to tools or object types (TBD)
		return !InSpec.RootPreset.IsNone();
	}
	return true;
}

ECraftingResult UModumateObjectAssemblyStatics::MakeLayeredAssembly(
	const Modumate::ModumateObjectDatabase &InDB,
	const Modumate::BIM::FModumateAssemblyPropertySpec &InSpec,
	FModumateObjectAssembly &OutMOA)
{
	OutMOA = FModumateObjectAssembly();

	OutMOA.ObjectType = InSpec.ObjectType;
	OutMOA.RootPreset = InSpec.RootPreset;
	OutMOA.Properties = InSpec.RootProperties;

	for (auto &layerProperties : InSpec.LayerProperties)
	{
		FLayerMaker layerMaker;

		layerProperties.ForEachProperty([&layerMaker](const FString &propName, const FModumateCommandParameter &val)
		{
			if (!propName.IsEmpty())
			{
				layerMaker.SetValue(BIM::FValueSpec(*propName), val);
			}
		});

		FModumateObjectAssemblyLayer &layer = OutMOA.Layers.Add_GetRef(layerMaker.Make(InDB));
	}

	return ECraftingResult::Success;
}

ECraftingResult UModumateObjectAssemblyStatics::MakeStructureLineAssembly(
	const Modumate::ModumateObjectDatabase &InDB,
	const Modumate::BIM::FModumateAssemblyPropertySpec &InSpec,
	FModumateObjectAssembly &OutMOA)
{
	OutMOA = FModumateObjectAssembly();
	OutMOA.ObjectType = EObjectType::OTStructureLine;

	OutMOA.RootPreset = InSpec.RootPreset;
	OutMOA.Properties = InSpec.RootProperties;


	FLayerMaker layerMaker;
	layerMaker.CodeName = OutMOA.GetProperty(BIM::Parameters::Name);

	FString diameterString;
	FModumateFormattedDimension xDim, yDim;
	if (OutMOA.Properties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Diameter, diameterString))
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(BIM::EScope::Layer, BIM::Parameters::Diameter));
		yDim = xDim;
	}
	else
	{
		xDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(BIM::EScope::Layer, BIM::Parameters::XExtents));
		yDim = UModumateDimensionStatics::StringToFormattedDimension(OutMOA.Properties.GetProperty(BIM::EScope::Layer, BIM::Parameters::YExtents));
	}

	if (ensureAlways(xDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIM::Parameters::XExtents, xDim);
	}

	if (ensureAlways(yDim.Format != EDimensionFormat::Error))
	{
		layerMaker.Dimensions.Add(BIM::Parameters::YExtents, yDim);
	}

	layerMaker.FormatEnum = ELayerFormat::None;
	layerMaker.FunctionEnum = ELayerFunction::Structure;
	layerMaker.LayerMaterialKey = OutMOA.Properties.GetProperty(BIM::EScope::Layer, BIM::Parameters::Material);

	layerMaker.ProfileKey = OutMOA.Properties.GetProperty(BIM::EScope::Mesh, BIM::Parameters::AssetID);
	layerMaker.Subcategories.Add(TEXT("SubcategoryNone"));


	FModumateObjectAssemblyLayer &layer = OutMOA.Layers.Add_GetRef(layerMaker.Make(InDB));

	FString colorHex = OutMOA.Properties.GetProperty(BIM::EScope::MaterialColor, BIM::Parameters::HexValue);
	layer.Material.DefaultBaseColor.Color = FColor::FromHex(colorHex);
	layer.Material.DefaultBaseColor.bValid = true;

	OutMOA.SetProperty(BIM::Parameters::Name, OutMOA.Properties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name));

	// TODO: re-orient column meshes so width is along X instead of depth
	FVector profileSize(xDim.Centimeters, yDim.Centimeters, 1);

	if (ensureAlways(layer.SimpleMeshes.Num() > 0 && layer.SimpleMeshes[0].Asset.Get()->Polygons.Num() > 0))
	{
		FSimplePolygon &polygon = layer.SimpleMeshes[0].Asset.Get()->Polygons[0];
		FVector polyExtents(polygon.Extents.Max.X - polygon.Extents.Min.X, polygon.Extents.Max.Y - polygon.Extents.Min.Y, 1);
		OutMOA.SetProperty(BIM::Parameters::Scale, profileSize / polyExtents);
	}

	return ECraftingResult::Success; 
}


ECraftingResult UModumateObjectAssemblyStatics::DoMakeAssembly(
	const Modumate::ModumateObjectDatabase &InDB,
	const Modumate::FPresetManager &PresetManager,
	const BIM::FModumateAssemblyPropertySpec &InSpec,
	FModumateObjectAssembly &OutMOA,
	const int32 InShowOnlyLayerID)
{
	// TODO: move assembly synthesis to each tool mode or MOI implementation (TBD)
	if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(InSpec.ObjectType))
	{
		switch(InSpec.ObjectType)
		{

		case EObjectType::OTStructureLine:
			return MakeStructureLineAssembly(InDB,InSpec,OutMOA);

		case EObjectType::OTFloorSegment:
		case EObjectType::OTWallSegment:
		case EObjectType::OTRoof:
		case EObjectType::OTFinish:
			return MakeLayeredAssembly(InDB,InSpec,OutMOA);

		default:
			ensureAlways(false);
		}
		return ECraftingResult::Error;
	}
	else
	{
		if (FModumateObjectAssembly::FromCraftingProperties_DEPRECATED(InSpec.ObjectType, InDB, PresetManager, InSpec, OutMOA, InShowOnlyLayerID))
		{
			return ECraftingResult::Success;
		}
	}

	return ECraftingResult::Error;
}

// TODO: remove after DDL2 migration
#define DDL_WORK_IN_PROGRESS 0

// Compiler warning in editor, refuse to package
#if DDL_WORK_IN_PROGRESS
#if WITH_EDITOR
#pragma message("WARNING: DDL_WORK_IN_PROGRESS IS ENABLED, DISABLE BEFORE SUBMITTING!!!")
#else
#error CANNOT PACKAGE BUILD WITH DDL_WORK_IN_PROGRESS
#endif
#endif

bool UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(EObjectType OT)
{
	switch (OT) {
	case EObjectType::OTStructureLine :
#if DDL_WORK_IN_PROGRESS
	case EObjectType::OTWallSegment :
	case EObjectType::OTFloorSegment :
	case EObjectType::OTRoof:
	case EObjectType::OTFinish:
#endif
		return true;
	};

	return false;
}



