// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCraftingWidget_CPP.h"
#include "ModumateCraftingTreeBuilder.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateBIMSchema.h"
#include "PlatformFunctions.h"
#include "ModumateConsoleCommand.h"
#include "ModumateObjectEnums.h"
#include "ModumateCommands.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateGameInstance.h"
#include "EditModelGameState_CPP.h"
#include "ModumateObjectDatabase.h"
#include "ModumateFunctionLibrary.h"
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

	BIM::FCraftingTreeNodeInstanceSharedPtr instance = CraftingTreeNodeInstances.InstanceFromID(InstanceID);
	if (!ensureAlways(instance.IsValid()))
	{
		return ECraftingResult::Error;
	}

	const BIM::FCraftingTreeNodePreset *currentPreset = presetCollection.Presets.Find(instance->PresetID);

	if (!ensureAlways(currentPreset != nullptr))
	{
		return ECraftingResult::Error;
	}

	const TArray<FName> *parentList = presetCollection.Lists.Find(CraftingTreeNodeInstances.GetListForParentPin(InstanceID));

	if (parentList != nullptr)
	{
		FName nodeType = NAME_None;
		for (auto &presetID : *parentList)
		{
			const BIM::FCraftingTreeNodePreset *preset = presetCollection.Presets.Find(presetID);
			if (ensureAlways(preset != nullptr))
			{
				nodeType = preset->NodeType;
				OutPresets.Add(FCraftingNode::FromPreset(*preset));
			}
		}

		const TArray<FName> *customs = presetCollection.CustomPresetsByNodeType.Find(nodeType);
		if (customs != nullptr)
		{
			for (auto &presetID : *customs)
			{
				const BIM::FCraftingTreeNodePreset *preset = presetCollection.Presets.Find(presetID);
				if (ensureAlways(preset != nullptr))
				{
					OutPresets.Add(FCraftingNode::FromPreset(*preset));
				}
			}
		}
	}
	else
	{
		for (auto &kvp : presetCollection.Presets)
		{
			if (kvp.Value.NodeType == currentPreset->NodeType)
			{
				OutPresets.Add(FCraftingNode::FromPreset(kvp.Value));
			}
		}
	}
	return ECraftingResult::Success;
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

	// TODO: DDL 1.0, deprecated
	else
	{
		AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		gameState->Document.PresetManager.LoadDecisionTreeForToolMode(GetWorld(), ToolMode);
		if (DatabaseKey == NAME_None)
		{
			gameState->Document.PresetManager.MakeNewTreeForToolMode(GetWorld(), ToolMode, CurrentCraftingTree);
		}
		else
		{
			gameState->Document.PresetManager.MakeNewTreeForAssembly(GetWorld(), ToolMode, DatabaseKey, CurrentCraftingTree);
		}
		GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.BuildAssemblyFromCraftingTree(GetWorld(), ToolMode, CurrentCraftingTree, CraftingAssembly, true);
		PreviewAssembly = CraftingAssembly;
		UpdateCraftingDecisions(ToolMode);
		return ECraftingResult::Success;
	}
	return ECraftingResult::Success;
}

/*
DDL 1.0
*/

void UModumateCraftingWidget_CPP::InitDecisionTrees()
{
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	gameState->Document.PresetManager.LoadAllDecisionTrees(GetWorld());
}

// TODO: this is only needed for assembly-level entries, make clearer
void UModumateCraftingWidget_CPP::UpdateCraftingDecisions(EToolMode mode)
{
	CraftingDecisions = CurrentCraftingTree.RootNode.GetOpenNodes();
}

void UModumateCraftingWidget_CPP::UpdateCraftingAssembly(EToolMode ToolMode)
{
	EObjectType ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);

	// As tool modes are upgraded to DDL 2.0, they'll get added here .. legacy tool modes will continue to use the DDL 1.0 crafting tree
	// NOTE: not using the ShouldUseDDL2ForObjectType wrapper because the assembly system will ensure on incomplete data
	// when data in development is ready for assembly testing, update ObjectTypeSupportsDDL2
	if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(ObjectType))
	{
		TryMakeNodeAssembly(ToolMode, CraftingTreeNodeInstances, CraftingAssembly);
		return;
	}
	GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.BuildAssemblyFromCraftingTree(GetWorld(), ToolMode, CurrentCraftingTree, CraftingAssembly, true);
}


/*
Either updating the current assembly with a clicked choice or the preview assembly with a hovered choice
*/
void UModumateCraftingWidget_CPP::UpdateAssemblyWithCraftingDecision(
	EToolMode mode,
	FCraftingDecisionTree &craftingTree,
	const FName &nodeGUID,
	const FName &decisionGUID,
	FModumateObjectAssembly &outAssembly,
	bool usePresets)
{
	FCraftingDecisionTreeNode *node = craftingTree.FindNodeByGUID(nodeGUID);

	if (node != nullptr && node->Data.Type != EDecisionType::DynamicList)
	{
		craftingTree.SetNameValueByGUID(nodeGUID, decisionGUID);
	}
	else
	{
		craftingTree.SetDynamicListElementNameValueByGUID(
			craftingTree.RootNode.Data.DynamicList,
			craftingTree.RootNode.Data.DynamicListElementIndex,
			nodeGUID,
			decisionGUID);
	}

	FPresetManager &presetManager = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager;
	presetManager.FillInMissingPresets(GetWorld(), mode, craftingTree);
	presetManager.BuildAssemblyFromCraftingTree(GetWorld(), mode, craftingTree, outAssembly, usePresets);
}

void UModumateCraftingWidget_CPP::UpdatePreviewAssemblyWithCraftingDecision(EToolMode mode, const FName &nodeGUID, const FName &decisionGUID)
{
	//Make a copy of the crafting tree because this is a temporary preview
	FCraftingDecisionTree craftingTree = CurrentCraftingTree;
	UpdateAssemblyWithCraftingDecision(mode, craftingTree, nodeGUID, decisionGUID, PreviewAssembly, false);
}

void UModumateCraftingWidget_CPP::SetValueForCraftingDecision(EToolMode mode, const FName &nodeGUID, const FName &decisionGUID)
{
	//Get a reference to the tree because this is a mutating operation
	UpdateAssemblyWithCraftingDecision(mode, CurrentCraftingTree, nodeGUID, decisionGUID, CraftingAssembly, true);
}

/*
Preset event handlers: Either update the current tree with a clicked preset or the preview tree with a hovered preset
*/
void UModumateCraftingWidget_CPP::UpdateAssemblyWithPreset(
	EToolMode mode,
	Modumate::FCraftingDecisionTree &craftingTree,
	const FName &formGUID,
	const FName &presetKey,
	FModumateObjectAssembly &outAssembly,
	bool usePresets,
	const int32 showOnlyLayerID)
{
	AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	state->Document.PresetManager.UpdateCraftingTreeWithPreset(craftingTree, formGUID, presetKey);
	state->Document.PresetManager.BuildAssemblyFromCraftingTree(GetWorld(), mode, craftingTree, outAssembly, usePresets, showOnlyLayerID);
}

void UModumateCraftingWidget_CPP::UpdatePreviewAssemblyWithPreset(EToolMode mode, const FName &formGUID, const FName &presetKey, const int32 showOnlyLayerID)
{
	// Make a copy of the crafting tree because this is a preview operation
	FCraftingDecisionTree craftingTree = CurrentCraftingTree;
	UpdateAssemblyWithPreset(mode, craftingTree, formGUID, presetKey, PreviewAssembly, false, showOnlyLayerID);
}

void UModumateCraftingWidget_CPP::OnPresetSelected(EToolMode mode, const FName &formGUID, const FName &presetKey)
{
	FPresetManager &presetManager = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager;
	if (presetManager.IsBuiltin(presetKey))
	{
		presetManager.UpdateCraftingTreeWithBuiltin(GetWorld(), mode, CurrentCraftingTree, formGUID, presetKey);
		UpdateCraftingAssembly(mode);
	}
	else
	{
		// use a reference to the crafting tree because this is a mutator
		// TODO: Portal based toolmode uses different route for assembly compare to layer based toolmode
		// Portal related toolmode should set bUsePreset = false until refactor
		bool bUsePreset = true;
		if(mode == EToolMode::VE_CABINET)
		{
			bUsePreset = false;
		}
		UpdateAssemblyWithPreset(mode, CurrentCraftingTree, formGUID, presetKey, CraftingAssembly, bUsePreset);
	}
}

/*
Preset data managers
*/

void UModumateCraftingWidget_CPP::AddPreset(EToolMode mode, const FName &formGUID)
{
	OverwritePreset(mode, formGUID, NAME_None);
}

void UModumateCraftingWidget_CPP::OverwritePreset(EToolMode mode, const FName &formGUID, const FName &presetKey)
{
	FPresetManager &presetManager = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager;

	FCraftingPreset newPreset;
	presetManager.MakeNewPresetFromCraftingTreeAndFormGUID(mode, CurrentCraftingTree, formGUID, newPreset);

	FName newPresetKey;
	presetManager.DispatchCreateOrUpdatePresetCommand(GetWorld(), mode, newPreset.Properties, newPreset.ChildPresets, presetKey, newPresetKey);

	// If we added a new preset, set it as selected on the form node
	if (presetKey == NAME_None)
	{
		FCraftingDecisionTreeNode *node = FPresetManager::GetFormInActiveLayerByGUID(CurrentCraftingTree, formGUID);
		const FCraftingPreset *newPresetPtr = presetManager.GetPresetByKey(newPresetKey);

		if (ensureAlways(node != nullptr && newPresetPtr != nullptr))
		{
			node->Data.SelectedPreset = newPresetKey;
			node->Data.Value = newPresetPtr->SequenceCode.ToString();
		}
	}

	UpdateCraftingAssembly(mode);
}

bool UModumateCraftingWidget_CPP::GetBuiltinsForForm(EToolMode mode, const FName &formGUID, TArray<FCraftingItem> &outPresets)
{
	return GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.GetBuiltinsForForm(mode, CurrentCraftingTree, formGUID, outPresets);
}

bool UModumateCraftingWidget_CPP::GetPresetsForForm(EToolMode mode, const FName &formGUID, TArray<FCraftingItem> &outPresets)
{
	return GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.GetPresetsForForm(mode, CurrentCraftingTree, formGUID, outPresets);
}

FName UModumateCraftingWidget_CPP::GetSelectedPresetForForm(EToolMode mode, const FName &formGUID)
{
	const FCraftingDecisionTreeNode *node = FPresetManager::GetFormInActiveLayerByGUID(CurrentCraftingTree, formGUID);

	if (ensureAlways(node != nullptr))
	{
		AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		const FCraftingPreset* preset = state->Document.PresetManager.GetPresetByKey(node->Data.SelectedPreset);

		if (preset != nullptr)
		{
			return preset->Key;
		}
	}
	return NAME_None;
}

// Remove preset
bool UModumateCraftingWidget_CPP::CheckRemoveFormPreset(EToolMode mode, const FName &formGUID, const FName &presetKey, TArray<FName> &affectedAssemblies)
{
	AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FCraftingPreset *preset = state->Document.PresetManager.GetPresetByKey(presetKey);
	if (ensureAlways(preset != nullptr))
	{
		TArray<FCraftingPreset> presets;
		state->Document.PresetManager.GetPresetsForForm(preset->Subcategory, preset->FormName, presets);

		TArray<FName> assemblies;
		state->Document.PresetManager.GetAssembyKeysForPreset(mode, presetKey, assemblies);
		if (assemblies.Num() > 0)
		{
			affectedAssemblies = assemblies;
			return true;
		}
	}
	return false;
}

bool UModumateCraftingWidget_CPP::DoReplaceFormPreset(EToolMode mode, const FName &formGUID, const FName &presetKey, const TArray<FName> &replaceAssemblies, const FName &replacementKey)
{
	AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	state->Document.PresetManager.DispatchRemovePresetCommand(GetWorld(), mode, presetKey, replacementKey);

	FCraftingDecisionTree::FDecisionNodeArray *instances = CurrentCraftingTree.DynamicListElementInstances.Find(CurrentCraftingTree.RootNode.Data.DynamicList);

	if (instances != nullptr)
	{
		for (auto &instance : *instances)
		{
			instance.WalkDepthFirst([presetKey, replacementKey](FCraftingDecisionTreeNode &node) {
				if (node.Data.SelectedPreset == presetKey)
				{
					node.Data.SelectedPreset = replacementKey;
				}
				return true;
			});
		}
	}

	UpdateCraftingAssembly(mode);
	return false;
}

void UModumateCraftingWidget_CPP::DoRemoveFormPreset(EToolMode mode, const FName &formGUID, const FName &presetKey)
{
	DoReplaceFormPreset(mode, formGUID, presetKey, TArray<FName>(), NAME_None);
}

// Save over preset
bool UModumateCraftingWidget_CPP::CheckSaveFormPreset(EToolMode mode, const FName &formGUID, TArray<FName> &affectedAssemblies)
{
	const FCraftingDecisionTreeNode *node = FPresetManager::GetFormInActiveLayerByGUID(CurrentCraftingTree, formGUID);
	if (ensureAlways(node->Data.HasPreset) && node->Data.SelectedPreset != NAME_None)
	{
		TArray<FName> assemblies;
		AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		state->Document.PresetManager.GetAssembyKeysForPreset(mode, node->Data.SelectedPreset, assemblies);

		if (assemblies.Num() > 0)
		{
			affectedAssemblies = assemblies;
			return true;
		}
	}
	return false;
}

void UModumateCraftingWidget_CPP::DoSaveFormPreset(EToolMode mode, const FName &formGUID)
{
	const FCraftingDecisionTreeNode *node = FPresetManager::GetFormInActiveLayerByGUID(CurrentCraftingTree, formGUID);
	AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	if (ensureAlways(node->Data.HasPreset) && node->Data.SelectedPreset != NAME_None)
	{
		TArray<FName> assemblies;
		state->Document.PresetManager.GetAssembyKeysForPreset(mode, node->Data.SelectedPreset, assemblies);

		OverwritePreset(mode, formGUID, node->Data.SelectedPreset);

		for (auto &assembly : assemblies)
		{
			state->Document.PresetManager.RefreshExistingAssembly(GetWorld(), mode, assembly);
		}

		UpdateCraftingAssembly(mode);
	}
}

void UModumateCraftingWidget_CPP::CancelFormPresetEdit(EToolMode mode, const FName &formGUID)
{
	const FCraftingDecisionTreeNode *node = FPresetManager::GetFormInActiveLayerByGUID(CurrentCraftingTree, formGUID);
	if (ensureAlways(node->Data.HasPreset) && node->Data.SelectedPreset != NAME_None)
	{
		OnPresetSelected(mode, formGUID, node->Data.SelectedPreset);
	}
}

/*
Manage assembly database (in preset manager)
*/

bool UModumateCraftingWidget_CPP::CommitCraftingAssembly(EToolMode toolMode)
{
	return OverwriteExistingAssembly(toolMode, NAME_None);
}

bool UModumateCraftingWidget_CPP::OverwriteExistingAssembly(EToolMode toolMode, const FName& databaseKey)
{
	if (CraftingAssembly.ObjectType == EObjectType::OTUnknown)
	{
		return false;
	}

	FModumateFunctionParameterSet  params;
	FName oldCraftingKey = CraftingAssembly.DatabaseKey;
	CraftingAssembly.DatabaseKey = databaseKey;

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();

	if (!CraftingAssembly.ToParameterSet_DEPRECATED(params))
	{
		CraftingAssembly.DatabaseKey = oldCraftingKey;
		return false;
	}

	CraftingAssembly.DatabaseKey = oldCraftingKey;

	FModumateCommand cmd(databaseKey.IsNone() ? Commands::kCreateNewAssembly_DEPRECATED : Commands::kOverwriteAssembly_DEPRECATED, false, 3);
	cmd.SetParameterSet(params);

	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	gameInstance->DoModumateCommand(cmd);
	return true;
}

bool UModumateCraftingWidget_CPP::GetAllPropertiesFromAssembly(EToolMode mode, const FName& databaseKey, TArray<FString> &properties, TArray<FString> &values, int32 specificLayer /*= -1*/)
{
	AEditModelGameState_CPP *state = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	if (state == nullptr)
	{
		return false;
	}
	const FModumateObjectAssembly *obAsm = state->GetAssemblyByKey_DEPRECATED(mode, databaseKey);
	if (obAsm == nullptr)
	{
		return false;
	}

	BIM::FModumateAssemblyPropertySpec spec;
	obAsm->FillSpec(spec);
	FModumateFunctionParameterSet params;

	if (specificLayer >= 0  && specificLayer < spec.LayerProperties.Num())
	{
		auto &l = spec.LayerProperties[specificLayer];
		l.ForEachProperty([&params, &properties, &values](const FString &name, const FModumateCommandParameter &param)
		{
			properties.Add(name);
			values.Add(param.AsString());
		});
	}
	else
	{
		spec.RootProperties.ForEachProperty([&params, &properties, &values](const FString &name, const FModumateCommandParameter &param)
		{
			properties.Add(name);
			values.Add(param.AsString());
		});
	}
	return true;
}

/*
Dynamic List management - add/remove/reorder layers
*/
static TArray<FCraftingItem> itemArrayFromNodeArray(const TArray<Modumate::FCraftingDecisionTreeNode> &na)
{
	TArray<FCraftingItem> itemArray;
	Algo::Transform(na, itemArray, [](const Modumate::FCraftingDecisionTreeNode &n) {return n.Data; });
	return itemArray;
}

TArray<FCraftingItem> UModumateCraftingWidget_CPP::GetElementsOfDynamicList(EToolMode mode, const FCraftingItem &dynamicList)
{
	if (ensureAlways(dynamicList.Type == EDecisionType::DynamicList))
	{
		TArray<FCraftingDecisionTreeNode> *elems = CurrentCraftingTree.DynamicListElementInstances.Find(dynamicList.GUID);
		if (ensureAlways(elems != nullptr))
		{
			return itemArrayFromNodeArray(*elems);
		}
	}
	return TArray<FCraftingItem>();
}

TArray<FCraftingItem> UModumateCraftingWidget_CPP::GetDecisionsForDynamicListElement(EToolMode mode, const FName &GUID, int32 i)
{
	CurrentCraftingTree.RootNode.Data.DynamicList = GUID;
	CurrentCraftingTree.RootNode.Data.DynamicListElementIndex = i;
	GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.FillInMissingPresets(GetWorld(), mode, CurrentCraftingTree);

	TArray<Modumate::FCraftingDecisionTreeNode> *elems = CurrentCraftingTree.DynamicListElementInstances.Find(GUID);
	if (ensureAlways(elems != nullptr) && ensureAlways(i < elems->Num()))
	{
		TArray<FCraftingItem> ret = (*elems)[i].GetOpenNodes();

		// If this is a layered tool, we're done
		switch (mode)
		{
		case EToolMode::VE_WINDOW:
		case EToolMode::VE_DOOR:
		case EToolMode::VE_CABINET:
			break;
		default: return ret;
		};

		// Otherwise, for slotted assemblies (cabinets and portals) make sure we only show the color channels for parts needed
		// by the part configuration...this two-factor set intersection is not directly supported by the DDL so we handle it
		// by filtering out options for Blueprint
		const ModumateObjectDatabase *db = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;


		// First, examine the open nodes and gather all the present slot types from the portal configuration AND all the available parts from the part slot map
		TArray<EPortalSlotType> neededParts;
		TMap<EPortalSlotType, FName> partsBySlotType;

		for (auto &openNode : ret)
		{
			// Ignore terminal nodes or form nodes, we only care about values that were assimilated into a select node by having been selected
			if (openNode.Type != EDecisionType::Select)
			{
				continue;
			}

			// We're a select node and we have portal configuration slots, that means we selected this portal configuration and we want to know what slots it has
			if (openNode.PortalConfigurationOption.Slots.Num() > 0)
			{
				// Sanity - we should only encounter one valid portal configuration option
				ensureAlways(neededParts.Num() == 0);
				for (auto &slot : openNode.PortalConfigurationOption.Slots)
				{
					if (slot.Type != EPortalSlotType::None)
					{
						neededParts.AddUnique(slot.Type);
					}
				}
			}

			// If we're the part set select, get the slot-to-part map .. we should only encounter this condition once in the return list .. only one part set
			if (openNode.PortalPartOption.PartsBySlotType.Num() > 0 && ensureAlways(partsBySlotType.Num() == 0))
			{
				partsBySlotType = openNode.PortalPartOption.PartsBySlotType;
			}
		}

		// Now, iterate over all of the parts the configuration reported needing, find the corresponding part in the selected part set ..
		// and add all of that part's material channels to the supported channels list
		TArray<FName> supportedChannels;
		for (auto &neededPart : neededParts)
		{
			FName *partName = partsBySlotType.Find(neededPart);
			if (partName != nullptr)
			{
				const FPortalPart *part = db->GetPortalPartByKey(*partName);
				if (ensureAlways(part != nullptr))
				{
					for (auto &channel : part->SupportedMaterialChannels)
					{
						supportedChannels.AddUnique(channel);
					}
				}
			}
		}

		// supportedChannels now contains all of the color channels this configuration/part set combination requires
		// remove all other color channels from the return list .. the crafting tree ensures that all channels available for 
		// ANY of the parts in the part set are in the node, so it's only a matter of eliminating unused values not needed by the configuration
		ret = ret.FilterByPredicate([&supportedChannels](const FCraftingItem &node)
		{
			// All material channels are select nodes of options, so if we're not a select, we're good
			if (node.Type != EDecisionType::Select)
			{
				return true;
			}

			// All and only material channel nodes have the MaterialColor property
			// They also have a corresponding <channel>.Material property where <channel> is one of the material channel types (ie Finish_Interior)
			// TODO: The slotted/portal material channels are currently defined as scopes in the BIM system .. when slotted assemblies are 
			// refactored these scopes will be retired
			if (node.Properties.HasProperty(BIM::EScope::MaterialColor, BIM::Parameters::MaterialKey))
			{
				// If we are a MaterialColor node, iterate over all the supported channels and see if we have the corresponding property
				for (auto &sc : supportedChannels)
				{
					EBIMValueScope channelScope;
					if (TryFindEnumValueByName<EBIMValueScope>(TEXT("EBIMValueScope"), sc, channelScope))
					{
						if (node.Properties.HasProperty(channelScope, BIM::Parameters::MaterialKey))
						{
							return true;
						}
					}
				}
				// Couldn't find this node's material property in the supported channel set .. reject it
				return false;
			}
			return true;
		});

		return ret;
	}
	return TArray<FCraftingItem>();
}

FCraftingItem UModumateCraftingWidget_CPP::AddElementToDynamicListAt(EToolMode mode, const FCraftingItem &dynamicList, int32 i)
{
	if (ensureAlways(dynamicList.Type == EDecisionType::DynamicList))
	{
		const TDecisionTreeNode<FCraftingItem> *ret = CurrentCraftingTree.MakeDynamicListElementAt(dynamicList.GUID, i);

		CurrentCraftingTree.RootNode.Data.DynamicListElementIndex = i;
		GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.FillInMissingPresets(GetWorld(), mode, CurrentCraftingTree);
		UpdateCraftingAssembly(mode);
		return ret->Data;
	}
	return dynamicList.AsError();
}

TArray<FCraftingItem> UModumateCraftingWidget_CPP::RemoveElementFromDynamicListAt(EToolMode mode, const FCraftingItem &dynamicList, int32 i)
{
	TArray<Modumate::FCraftingDecisionTreeNode> *elems = CurrentCraftingTree.DynamicListElementInstances.Find(dynamicList.GUID);
	if (ensureAlways(elems != nullptr))
	{
		if (ensureAlways(i < elems->Num()))
		{
			// if we have deleted an element that appears in the list before our current selection, decrement the current selection
			if (i < CurrentCraftingTree.RootNode.Data.DynamicListElementIndex)
			{
				--CurrentCraftingTree.RootNode.Data.DynamicListElementIndex;
				GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.PresetManager.FillInMissingPresets(GetWorld(), mode, CurrentCraftingTree);
			}

			elems->RemoveAt(i);
			UpdateCraftingAssembly(mode);
			return itemArrayFromNodeArray(*elems);
		}
	}
	return TArray<FCraftingItem>();
}

TArray<FCraftingItem> UModumateCraftingWidget_CPP::MoveElementInDynamicList(EToolMode mode, const FCraftingItem &dynamicList, int32 from, int32 to)
{
	if (ensureAlways(dynamicList.Type == EDecisionType::DynamicList))
	{
		TArray<Modumate::FCraftingDecisionTreeNode> *elems = CurrentCraftingTree.DynamicListElementInstances.Find(dynamicList.GUID);
		if (ensureAlways(elems != nullptr))
		{
			if (ensureAlways(from < elems->Num() && to < elems->Num()))
			{
				elems->Swap(from, to);
				UpdateCraftingAssembly(mode);
				return itemArrayFromNodeArray(*elems);
			}
		}
	}
	return TArray<FCraftingItem>();
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
