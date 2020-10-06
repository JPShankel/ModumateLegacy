// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMPresets.h"

/*
	Crafting trees are acyclical networks of crafting node instances
	They have their own input pin sets and properties, both of which are inherited from the preset used to create the node
	If a node's properties or child configuration are inconsistent with the base preset, the preset is considered 'dirty' and must be updated or branched
*/

class FBIMAssemblySpec;
class FBIMCraftingTreeNode;
class FModumateDatabase;

typedef TSharedPtr<FBIMCraftingTreeNode> FBIMCraftingTreeNodeSharedPtr;
typedef TWeakPtr<FBIMCraftingTreeNode> FBIMCraftingTreeNodeWeakPtr;

typedef TSharedPtr<const FBIMCraftingTreeNode> FBIMCraftingTreeNodeSharedPtrConst;
typedef TWeakPtr<const FBIMCraftingTreeNode> FBIMCraftingTreeNodeWeakPtrConst;

class MODUMATE_API FBIMCraftingTreeNode : public TSharedFromThis<FBIMCraftingTreeNode>
{
	friend class FBIMCraftingTreeNodePool;

#if WITH_EDITOR // for debugging new/delete balance
	static int32 InstanceCount;
#endif

private:
	// Node instances are managed solely by the object node set and should not be passed around by value or constructed by anyone else
	FBIMCraftingTreeNode() = delete;
	FBIMCraftingTreeNode(const FBIMCraftingTreeNode &rhs) = delete;
	FBIMCraftingTreeNode &operator=(const FBIMCraftingTreeNode &rhs) = delete;
	FBIMCraftingTreeNode(int32 instanceID);

	int32 InstanceID;

public:
	~FBIMCraftingTreeNode()
	{
#if WITH_EDITOR // for debugging new/delete balance
		--InstanceCount;
#endif
	};

	struct FAttachedChildGroup
	{
		FChildAttachmentType SetType;
		FBIMPreset::FPartSlot PartSlot;
		bool IsPart() const { return !PartSlot.PartPreset.IsNone(); }
		TArray<FBIMCraftingTreeNodeWeakPtr> Children;
	};

	FBIMKey PresetID;
	FBIMCraftingTreeNodeWeakPtr ParentInstance;
	FString CategoryTitle = TEXT("Unknown Category");

	TArray<FAttachedChildGroup> AttachedChildren;
	FBIMPropertySheet InstanceProperties;

	// May be fixed in type definition, inherited from parent or switchable
	EConfiguratorNodeIconOrientation CurrentOrientation;

	int32 GetInstanceID() const;

	EBIMPresetEditorNodeStatus GetPresetStatus(const FBIMPresetCollection &PresetCollection) const;

	ECraftingResult ToPreset(const FBIMPresetCollection& PresetCollection, FBIMPreset& OutPreset) const;

	ECraftingResult ToDataRecord(FCustomAssemblyCraftingNodeRecord &OutRecord) const;
	ECraftingResult FromDataRecord(FBIMCraftingTreeNodePool &InstancePool, const FBIMPresetCollection &PresetCollection, const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets);

	bool CanRemoveChild(const FBIMCraftingTreeNodeSharedPtrConst &Child) const;
	bool CanReorderChild(const FBIMCraftingTreeNodeSharedPtrConst &Child) const;

	ECraftingResult DetachSelfFromParent();

	ECraftingResult AttachChild(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child);
	ECraftingResult AttachChildAt(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child, int32 PinSetIndex, int32 PinSetPosition);
	ECraftingResult AttachPartChild(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child, const FName& PartName);
	ECraftingResult FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition);
	ECraftingResult FindOtherChildrenOnPin(TArray<int32> &OutChildIDs);
	ECraftingResult GatherChildrenInOrder(TArray<int32> &OutChildIDs);
	ECraftingResult GatherAllChildNodes(TArray<FBIMCraftingTreeNodeSharedPtr> &OutChildren);
};

/*
	A Crafting Tree Node Set contains all the context information for node crafting.
	Node descriptors, preset definitions and preset lists are read in via ParseScriptFile
	The crafting widget and preset manager use this node set to implement the crafting interface

	TODO: this functionality to be merged into the crafting widget which is the only client of node instances
*/

class MODUMATE_API FBIMCraftingTreeNodePool
{
private:
	int32 NextInstanceID = 1;
	TMap<int32, FBIMCraftingTreeNodeWeakPtr> InstanceMap;
	TArray<FBIMCraftingTreeNodeSharedPtr> InstancePool;

	// Does not create a fully resolved node, used in FromDataRecord only
	FBIMCraftingTreeNodeSharedPtr CreateNodeInstanceFromDataRecord(const FBIMPresetCollection &PresetCollection, const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets);

public:

	ECraftingResult ResetInstances();
	ECraftingResult InitFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID, FBIMCraftingTreeNodeSharedPtr &OutRootNode);

	ECraftingResult DestroyNodeInstance(const FBIMCraftingTreeNodeSharedPtr& Instance, TArray<int32>& OutDestroyed);
	ECraftingResult DestroyNodeInstance(int32 InstanceID, TArray<int32>& OutDestroyed);

	const TArray<FBIMCraftingTreeNodeSharedPtr> &GetInstancePool() const { return InstancePool; }

	FBIMCraftingTreeNodeSharedPtr CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, int32 ParentID, const FBIMKey& PresetID, int32 ParentSetIndex, int32 ParentSetPosition);
	ECraftingResult SetNewPresetForNode(const FBIMPresetCollection &PresetCollection, int32 InstanceID, const FBIMKey &PresetID);

	const FBIMCraftingTreeNodeSharedPtr InstanceFromID(int32 InstanceID) const;
	FBIMCraftingTreeNodeSharedPtr InstanceFromID(int32 InstanceID);
	FBIMCraftingTreeNodeSharedPtr FindInstanceByPredicate(const std::function<bool(const FBIMCraftingTreeNodeSharedPtr &Instance)> &Predicate);

	bool FromDataRecord(const FBIMPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords, bool RecursePresets);
	bool ToDataRecord(TArray<FCustomAssemblyCraftingNodeRecord> &OutDataRecords) const;

	bool ValidatePool() const;

	ECraftingResult CreateAssemblyFromNodes(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, FBIMAssemblySpec& OutAssemblySpec);
	ECraftingResult CreateAssemblyFromLayerNode(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, int32 LayerNodeID, FBIMAssemblySpec& OutAssemblySpec);
	ECraftingResult ReorderChildNode(int32 ChildNode, int32 FromPosition, int32 ToPosition);

	bool GetSortedNodeIDs(TArray<int32> &OutNodeIDs);
};
