// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateCraftingWidget_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "BIMKernel/BIMProperties.h"
#include "ModumateCore/PlatformFunctions.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "Database/ModumateObjectEnums.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Algo/Transform.h"

using namespace Modumate;
/*
DDL 2.0 APIs all return ECraftingResult .. other APIs to be deprecated as 2.0 comes online
*/

const FPresetManager &UModumateCraftingWidget_CPP::GetDocumentPresetManager() const
{
	const AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	return gameState->Document.PresetManager;
}

bool UModumateCraftingWidget_CPP::ShouldUseDDL2ForObjectType(EObjectType ObjectType) const
{
	if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(ObjectType))
	{
		return true;
	}

	// debug flag set when testing new ddl2 data
	AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
	return playerState->bDevelopDDL2Data;
}

ECraftingResult UModumateCraftingWidget_CPP::SetNewPresetForNode(EToolMode ToolMode, int32 InstanceID, const FName &PresetID)
{
	const BIM::FCraftingPresetCollection &presetCollection = GetDocumentPresetManager().CraftingNodePresets;

	ECraftingResult result = CraftingTreeNodeInstances.SetNewPresetForNode(presetCollection, InstanceID, PresetID);
	if (result == ECraftingResult::Success)
	{
		UpdateCraftingAssembly(ToolMode);
	}
	return result;
}

ECraftingResult UModumateCraftingWidget_CPP::GetPropertiesForPresetID(EToolMode ToolMode, FName PresetID, TMap<FString, FString> &OutPropertyMap)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();

	BIM::FModumateAssemblyPropertySpec presetSpec;
	presetManager.PresetToSpec(PresetID, presetSpec);

	FModumateFunctionParameterSet params;
	presetSpec.RootProperties.ForEachProperty([&params, &OutPropertyMap](const FString &name, const FModumateCommandParameter &param)
	{
		OutPropertyMap.Add(name, param.AsString());
	});

	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingWidget_CPP::GetPropertyTipsForPresetByIconType(EConfiguratorNodeIconType IconType, const FName &PresetID, TArray<FString> &OutTips)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const ModumateObjectDatabase *db = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;

	BIM::FModumateAssemblyPropertySpec presetSpec;
	presetManager.PresetToSpec(PresetID, presetSpec);
	return UModumateCraftingNodeWidgetStatics::GetPropertyTipsByIconType(*db, IconType, presetSpec, OutTips);
}

ECraftingResult UModumateCraftingWidget_CPP::GetAvailableStarterNodes(EToolMode ToolMode, TArray<FCraftingNode> &OutStarterNodes) const
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	// the starter nodes for a given object type are contained in a list of that object type's name
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
	const TArray<FName> *starters = presetCollection.Lists.Find(*EnumValueString(EObjectType, objectType));

	if (starters == nullptr)
	{
		return ECraftingResult::Error;
	}

	for (auto &starter : *starters)
	{
		const BIM::FCraftingTreeNodePreset *preset = presetCollection.Presets.Find(starter);
		if (preset != nullptr)
		{
			OutStarterNodes.Add(FCraftingNode::FromPreset(*preset));
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingWidget_CPP::CreateNewNodeInstanceFromPreset(EToolMode ToolMode, int32 ParentID, const FName &PresetID, FCraftingNode &OutNode)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	ECraftingResult result = UModumateCraftingNodeWidgetStatics::CreateNewNodeInstanceFromPreset(CraftingTreeNodeInstances, presetCollection, ParentID, PresetID, OutNode);
	if (result == ECraftingResult::Success)
	{
		EObjectType objectType = presetManager.CraftingNodePresets.GetPresetObjectType(PresetID);
		if (objectType != EObjectType::OTNone)
		{
			const FModumateObjectAssembly *assembly;
			OutNode.CanAddToProject = !presetManager.TryGetProjectAssemblyForPreset(objectType, PresetID, assembly);
		}
		UpdateCraftingAssembly(ToolMode);
	}
	return result;
}

ECraftingResult UModumateCraftingWidget_CPP::GetInstantiatedNodes(EToolMode ToolMode, TArray<FCraftingNode> &OutNodes)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	if (UModumateCraftingNodeWidgetStatics::GetInstantiatedNodes(CraftingTreeNodeInstances, presetManager.CraftingNodePresets, OutNodes) == ECraftingResult::Success)
	{
		for (auto &node : OutNodes)
		{
			EObjectType objectType = presetManager.CraftingNodePresets.GetPresetObjectType(node.PresetID);
			if (objectType != EObjectType::OTNone)
			{
				const FModumateObjectAssembly *assembly;
				node.CanAddToProject = !presetManager.TryGetProjectAssemblyForPreset(objectType, node.PresetID, assembly);
			}
		}
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingWidget_CPP::TryMakeNodeAssembly(EToolMode ToolMode, const Modumate::BIM::FCraftingTreeNodeInstancePool &InNodes, FModumateObjectAssembly &OutAssembly)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	BIM::FCraftingTreeNodeInstanceSharedPtr starterNode;

	for (auto &inst : InNodes.GetInstancePool())
	{
		if (presetCollection.GetPresetObjectType(inst->PresetID) != EObjectType::OTNone)
		{
			starterNode = inst;
			break;
		}
	}

	BIM::FModumateAssemblyPropertySpec spec;

	if (ensureAlways(starterNode != nullptr))
	{
		ECraftingResult result = InNodes.PresetToSpec(starterNode->PresetID, presetManager.CraftingNodePresets, spec);
		if (result != ECraftingResult::Success)
		{
			return result;
		}
		spec.RootPreset = starterNode->PresetID;
	}

	const ModumateObjectDatabase *db = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;

	EObjectType ot = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);

	if (ot != EObjectType::OTUnknown)
	{
		if (!UModumateObjectAssemblyStatics::CheckCanMakeAssembly(ot, *db, spec))
		{
			OutAssembly.ObjectType = EObjectType::OTUnknown;
			return ECraftingResult::Error;
		}

		return UModumateObjectAssemblyStatics::DoMakeAssembly(*db, presetManager, spec, OutAssembly);
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingWidget_CPP::GetPinGroupsForNode(EToolMode ToolMode, int32 NodeID, TArray<FCraftingNodePinGroup> &OutPins)
{
	return UModumateCraftingNodeWidgetStatics::GetPinGroupsForNode(CraftingTreeNodeInstances, GetDocumentPresetManager().CraftingNodePresets,NodeID,OutPins);
}

ECraftingResult UModumateCraftingWidget_CPP::DragDropPinForNode(EToolMode ToolMode, int32 NodeID, const FName &PinGroup, int32 From, int32 To)
{
	ECraftingResult result = UModumateCraftingNodeWidgetStatics::DragMovePinChild(CraftingTreeNodeInstances, NodeID, PinGroup, From, To);
	if (result == ECraftingResult::Success)
	{
		UpdateCraftingAssembly(ToolMode);
	}
	return result;
}

ECraftingResult UModumateCraftingWidget_CPP::IsCraftingAssemblyValid(EToolMode ToolMode, bool& OutIsValid)
{
	OutIsValid = CraftingAssembly.ObjectType != EObjectType::OTUnknown;
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingWidget_CPP::RemoveNodeInstance(EToolMode ToolMode, int32 ParentID, int32 InstanceID)
{
	ECraftingResult result = UModumateCraftingNodeWidgetStatics::RemoveNodeInstance(CraftingTreeNodeInstances,ParentID,InstanceID);
	UpdateCraftingAssembly(ToolMode);
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingWidget_CPP::GetPinAttachmentToParentForNode(EToolMode ToolMode, int32 InstanceID, int32 &OutPinGroupIndex, int32 &OutPinIndex)
{
	return UModumateCraftingNodeWidgetStatics::GetPinAttachmentToParentForNode(CraftingTreeNodeInstances,InstanceID,OutPinGroupIndex,OutPinIndex);
}

ECraftingResult UModumateCraftingWidget_CPP::GetFormItemsForCraftingNode(EToolMode ToolMode, int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm)
{
	return UModumateCraftingNodeWidgetStatics::GetFormItemsForCraftingNode(CraftingTreeNodeInstances,GetDocumentPresetManager().CraftingNodePresets,InstanceID, OutForm);
}

ECraftingResult UModumateCraftingWidget_CPP::GetFormItemsForPresetID(EToolMode ToolMode, const FName &PresetID, TArray<FCraftingNodeFormItem> &OutForm)
{
	return UModumateCraftingNodeWidgetStatics::GetFormItemsForPreset(GetDocumentPresetManager().CraftingNodePresets, PresetID, OutForm);
}

ECraftingResult UModumateCraftingWidget_CPP::SetValueForFormItem(EToolMode ToolMode, const FCraftingNodeFormItem &FormItem, const FString &Value)
{
	ECraftingResult result = UModumateCraftingNodeWidgetStatics::SetValueForFormItem(CraftingTreeNodeInstances,FormItem,Value);
	UpdateCraftingAssembly(ToolMode);
	return result;
}

ECraftingResult UModumateCraftingWidget_CPP::SetValueForPreset(EToolMode ToolMode, const FName &PresetID, const FString &Value)
{
	ECraftingResult result = UModumateCraftingNodeWidgetStatics::SetValueForPreset(PresetID, Value);
	UpdateCraftingAssembly(ToolMode);
	return result;
}

ECraftingResult UModumateCraftingWidget_CPP::SaveOrCreateCraftingPreset(EToolMode ToolMode, int32 InstanceID, const FName &PresetKey, FName &OutKey)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;
	BIM::FCraftingTreeNodeInstanceSharedPtr instance = CraftingTreeNodeInstances.InstanceFromID(InstanceID);

	if (!ensureAlways(instance.IsValid()))
	{
		return ECraftingResult::Error;
	}

	BIM::FCraftingTreeNodePreset preset;
	ECraftingResult result = presetCollection.GetInstanceDataAsPreset(instance, preset);

	if (result != ECraftingResult::Success)
	{
		return result;
	}

	preset.PresetID = PresetKey;
	FModumateFunctionParameterSet params;
	result = preset.ToParameterSet(params);
	if (result != ECraftingResult::Success)
	{
		return result;
	}

	params.SetValue(Modumate::Parameters::kBaseKeyName, instance->PresetID);

	FModumateCommand cmd(Commands::kUpdateCraftingPreset);
	cmd.SetParameterSet(params);

	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());

	FModumateFunctionParameterSet commandResult = gameInstance->DoModumateCommand(cmd);
	OutKey = commandResult.GetValue(Modumate::Parameters::kPresetKey);

	if (!ensureAlways(!OutKey.IsNone()))
	{
		return ECraftingResult::Error;
	}

	if (OutKey != PresetKey)
	{
		instance->PresetID = OutKey;
	}

	return ECraftingResult::Success;
}

/*
Cancel pending edit on a crafting node by rebuilding it from the stored preset (ie set its preset back to itself)
*/
ECraftingResult UModumateCraftingWidget_CPP::CancelCraftingPresetEdit(EToolMode ToolMode, int32 InstanceID)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	BIM::FCraftingTreeNodeInstanceSharedPtr instance = CraftingTreeNodeInstances.InstanceFromID(InstanceID);
	if (ensureAlways(instance.IsValid()))
	{
		return SetNewPresetForNode(ToolMode, InstanceID, instance->PresetID);
	}

	return ECraftingResult::Error;
}

/*
When prompted to save over a preset, we want to show all presets of the same type that are currently used in the project
*/
ECraftingResult UModumateCraftingWidget_CPP::GetEligiblePresetsForSave(EToolMode ToolMode, int32 InstanceID, TArray<FCraftingNode> &OutPresets)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);

	BIM::FCraftingTreeNodeInstanceSharedPtr instance = CraftingTreeNodeInstances.InstanceFromID(InstanceID);
	if (!instance.IsValid())
	{
		return ECraftingResult::Error;
	}

	const BIM::FCraftingTreeNodePreset *instancePreset = presetCollection.Presets.Find(instance->PresetID);
	if (instancePreset == nullptr)
	{
		return ECraftingResult::Error;
	}

	// Retrieve all presets used for this tool mode in available assemblies
	TSet<FName> presets;
	ECraftingResult fetchResult = presetManager.FetchAllPresetsForObjectType(objectType, presets);
	if (fetchResult != ECraftingResult::Success)
	{
		return fetchResult;
	}

	// Return each preset used in the project that is also of this node type
	Algo::TransformIf(presets, OutPresets, 
		[&presetCollection, instancePreset](const FName &PresetID)
		{
			const BIM::FCraftingTreeNodePreset *preset = presetCollection.Presets.Find(PresetID);
			if (preset == nullptr)
			{
				return false;
			}
			return (instancePreset->NodeType == preset->NodeType);
		},		
		[&presetCollection](const FName &PresetID)
		{
			// the predicate above ensures that preset will not be null
			const BIM::FCraftingTreeNodePreset *preset = presetCollection.Presets.Find(PresetID);
			return FCraftingNode::FromPreset(*preset);
		}
	);

	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingWidget_CPP::GetEligiblePresetsForSwap(EToolMode ToolMode, int32 InstanceID, TArray<FCraftingNode> &OutPresets)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;
	return UModumateCraftingNodeWidgetStatics::GetEligiblePresetsForSwap(CraftingTreeNodeInstances, presetCollection, InstanceID, OutPresets);
}

ECraftingResult UModumateCraftingWidget_CPP::UpdateNodePreviewAssemblyWithPreset(EToolMode ToolMode, int32 InstanceID, const FName &PresetKey)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();	
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	// Serialize instances
	TArray<FCustomAssemblyCraftingNodeRecord> tempDataRecord;
	CraftingTreeNodeInstances.ToDataRecord(tempDataRecord);

	BIM::FCraftingTreeNodeInstancePool tempPool;

	// Make preset change
	tempPool.FromDataRecord(presetCollection,tempDataRecord,false);

	ECraftingResult result = tempPool.SetNewPresetForNode(presetCollection, InstanceID, PresetKey);

	if (ensureAlways(result != ECraftingResult::Error))
	{
		result = TryMakeNodeAssembly(ToolMode,tempPool,PreviewAssembly);
	}

	return result;
}

ECraftingResult UModumateCraftingWidget_CPP::GetLayerIDFromNodeInstanceID(int32 InstanceID, int32 &LayerID, int32 &NumberOfLayers)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;
	return UModumateCraftingNodeWidgetStatics::GetLayerIDFromNodeInstanceID(CraftingTreeNodeInstances, presetCollection, InstanceID, LayerID, NumberOfLayers);
}

FCraftingNode FCraftingNode::FromPreset(const BIM::FCraftingTreeNodePreset &Preset)
{
	FCraftingNode returnNode;
	returnNode.InstanceID = 0;
	returnNode.ParentInstanceID = 0;
	returnNode.PresetID = Preset.PresetID;
	returnNode.PresetStatus = ECraftingNodePresetStatus::None;
	returnNode.Label = FText::FromString(Preset.GetDisplayName());
	return returnNode;
}

ECraftingResult UModumateCraftingWidget_CPP::BeginCraftingWithToolModeAndAssembly(EToolMode ToolMode, const FName& DatabaseKey, TArray<FCraftingNode> &OutNodes)
{
	const FPresetManager &presetManager = GetDocumentPresetManager();
	const BIM::FCraftingPresetCollection &presetCollection = presetManager.CraftingNodePresets;

	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);

	// DDL 2.0
	// either fully supported per UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2 or the debug flag is on in player state for data development
	if (ShouldUseDDL2ForObjectType(objectType))
	{
		CraftingTreeNodeInstances.ResetInstances();

		// If it's not a pre-existing assembly, start up with our object type
		TArray<FCraftingNode> starters;
		ECraftingResult result = GetAvailableStarterNodes(ToolMode, starters);

		if (result != ECraftingResult::Success)
		{
			return result;
		}

		if (starters.Num() > 0)
		{
			// outNode ends up in CraftingTreeNodeInstances so we don't need to hold on to it
			FCraftingNode outNode;
			result = CreateNewNodeInstanceFromPreset(ToolMode, 0, starters[0].PresetID, outNode);
		}
		else
		{
			return ECraftingResult::Error;
		}

		if (!DatabaseKey.IsNone())
		{
			const FModumateObjectAssembly *assembly = nullptr;
			if (ensureAlways(presetManager.TryGetProjectAssemblyForPreset(objectType, DatabaseKey, assembly)))
			{
				for (auto &inst : CraftingTreeNodeInstances.GetInstancePool())
				{
					EObjectType instanceObjectType = presetCollection.GetPresetObjectType(inst->PresetID);
					if (instanceObjectType != EObjectType::OTNone)
					{
						// TODO: root preset may be null in older saved games
						// Can remove check when RootPreset and DatabaseKey merge
						if (!assembly->RootPreset.IsNone())
						{
							SetNewPresetForNode(ToolMode, inst->GetInstanceID(), assembly->RootPreset);
						}
						break;
					}
				}
			}
			else
			{
				return ECraftingResult::Error;
			}
		}
		return GetInstantiatedNodes(ToolMode, OutNodes);
	}

	
	return ECraftingResult::Success;
}

/*
DDL 1.0
*/


void UModumateCraftingWidget_CPP::UpdateCraftingAssembly(EToolMode ToolMode)
{
	EObjectType ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);

	// As tool modes are upgraded to DDL 2.0, they'll get added here .. legacy tool modes will continue to use the DDL 1.0 crafting tree
	// NOTE: not using the ShouldUseDDL2ForObjectType wrapper because the assembly system will ensure on incomplete data
	// when data in development is ready for assembly testing, update ObjectTypeSupportsDDL2
	if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(ObjectType))
	{
		TryMakeNodeAssembly(ToolMode, CraftingTreeNodeInstances, CraftingAssembly);
	}
}

/*
Blueprint utilities
*/

TArray<float> UModumateCraftingWidget_CPP::GetCraftingAssemblyLayerThickness()
{
	TArray<float> layersThickness;
	for (int32 i = 0; i < CraftingAssembly.Layers.Num(); ++i)
	{
		layersThickness.Add(CraftingAssembly.Layers[i].Thickness.AsWorldCentimeters());
	}
	return layersThickness;
}

bool UModumateCraftingWidget_CPP::ApplyMaterialsToCraftingAssemblyMeshes(
	const TArray<UProceduralMeshComponent*> &ProceduralSubLayers,
	EToolMode toolMode,
	const TArray<UMaterialInterface*> &TilingMaterials,
	UMaterialInterface *MasterPBRMaterial,
	const FString &AssemblyKey)
{
	if (AssemblyKey == TEXT("Preview"))
	{
		return UModumateFunctionLibrary::UpdateMaterialsFromAssembly(ProceduralSubLayers, PreviewAssembly, TilingMaterials, MasterPBRMaterial);
	}
	return UModumateFunctionLibrary::UpdateMaterialsFromAssembly(ProceduralSubLayers, CraftingAssembly, TilingMaterials, MasterPBRMaterial);
}
