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

class FBIMAssemblySpec;
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
	TMap<int32, FBIMPresetEditorNodeWeakPtr> InstanceMap;
	TArray<FBIMPresetEditorNodeSharedPtr> InstancePool;

public:

	EBIMResult ResetInstances();
	EBIMResult InitFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID, FBIMPresetEditorNodeSharedPtr &OutRootNode);

	EBIMResult DestroyNodeInstance(const FBIMPresetEditorNodeSharedPtr& Instance, TArray<int32>& OutDestroyed);
	EBIMResult DestroyNodeInstance(int32 InstanceID, TArray<int32>& OutDestroyed);

	const TArray<FBIMPresetEditorNodeSharedPtr> &GetInstancePool() const { return InstancePool; }

	FBIMPresetEditorNodeSharedPtr CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, int32 ParentID, const FBIMKey& PresetID, int32 ParentSetIndex, int32 ParentSetPosition, const FBIMKey& SlotAssignment=FBIMKey());
	EBIMResult SetNewPresetForNode(const FBIMPresetCollection &PresetCollection, int32 InstanceID, const FBIMKey &PresetID);

	const FBIMPresetEditorNodeSharedPtr InstanceFromID(int32 InstanceID) const;
	FBIMPresetEditorNodeSharedPtr InstanceFromID(int32 InstanceID);

	bool ValidatePool() const;

	EBIMResult CreateAssemblyFromNodes(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, FBIMAssemblySpec& OutAssemblySpec);
	EBIMResult CreateAssemblyFromLayerNode(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, int32 LayerNodeID, FBIMAssemblySpec& OutAssemblySpec);
	EBIMResult ReorderChildNode(int32 ChildNode, int32 FromPosition, int32 ToPosition);
	EBIMResult FindNodeParentLineage(int32 NodeID, TArray<int32>& OutLineage);

	bool GetSortedNodeIDs(TArray<int32> &OutNodeIDs);
};
