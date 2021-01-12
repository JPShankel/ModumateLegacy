// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"

/*
	Crafting trees are acyclical networks of crafting node instances
	They have their own input pin sets and properties, both of which are inherited from the preset used to create the node
	If a node's properties or child configuration are inconsistent with the base preset, the preset is considered 'dirty' and must be updated or branched
*/

struct FBIMAssemblySpec;
class FBIMPresetEditorNode;
class FModumateDatabase;

/*
	FBIMPresetEditor contains all the context information for editing presets.
	Node descriptors, preset definitions and preset lists are read in via BIMCSVReader
	The crafting widget and preset manager use this node set to implement the crafting interface
*/

class MODUMATE_API FBIMPresetEditor
{
private:
	int32 NextInstanceID = 1;
	TMap<FBIMEditorNodeIDType, FBIMPresetEditorNodeWeakPtr> InstanceMap;
	TArray<FBIMPresetEditorNodeSharedPtr> InstancePool;

public:

	EBIMResult ResetInstances();
	EBIMResult InitFromPreset(const FBIMPresetCollection& PresetCollection, const FGuid& PresetGUID, FBIMPresetEditorNodeSharedPtr &OutRootNode);

	EBIMResult DestroyNodeInstance(const FBIMPresetEditorNodeSharedPtr& Instance, TArray<FBIMEditorNodeIDType>& OutDestroyed);
	EBIMResult DestroyNodeInstance(const FBIMEditorNodeIDType& InstanceID, TArray<FBIMEditorNodeIDType>& OutDestroyed);

	FBIMPresetEditorNodeSharedPtr CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& ParentID, const FGuid& PresetID, int32 ParentSetIndex, int32 ParentSetPosition, const FGuid& SlotAssignment);
	FBIMPresetEditorNodeSharedPtr CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& ParentID, const FGuid& PresetID, int32 ParentSetIndex, int32 ParentSetPosition);

	const TArray<FBIMPresetEditorNodeSharedPtr> &GetInstancePool() const { return InstancePool; }

	EBIMResult SetNewPresetForNode(const FBIMPresetCollection &PresetCollection, const FBIMEditorNodeIDType& InstanceID, const FGuid &PresetID);

	EBIMResult SetPartPreset(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& ParentID, int32 SlotID, const FGuid& PartPreset);
	EBIMResult ClearPartPreset(const FBIMEditorNodeIDType& ParentID, int32 SlotID);

	const FBIMPresetEditorNodeSharedPtr InstanceFromID(const FBIMEditorNodeIDType& InstanceID) const;
	FBIMPresetEditorNodeSharedPtr InstanceFromID(const FBIMEditorNodeIDType& InstanceID);

	const FBIMPresetEditorNodeSharedPtr GetRootInstance() const;
	FBIMPresetEditorNodeSharedPtr GetRootInstance();

	EBIMResult SortAndValidate() const;

	bool ValidatePool() const;

	EBIMResult CreateAssemblyFromNodes(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, FBIMAssemblySpec& OutAssemblySpec);
	EBIMResult CreateAssemblyFromLayerNode(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, const FBIMEditorNodeIDType& LayerNodeID, FBIMAssemblySpec& OutAssemblySpec);
	EBIMResult ReorderChildNode(const FBIMEditorNodeIDType& ChildNode, int32 FromPosition, int32 ToPosition);
	EBIMResult FindNodeParentLineage(const FBIMEditorNodeIDType& NodeID, TArray<FBIMEditorNodeIDType>& OutLineage);

	bool GetSortedNodeIDs(TArray<FBIMEditorNodeIDType> &OutNodeIDs);

	EBIMResult GetBIMPinTargetForChildNode(const FBIMEditorNodeIDType& ChildID, EBIMPinTarget& OutTarget) const;
};
