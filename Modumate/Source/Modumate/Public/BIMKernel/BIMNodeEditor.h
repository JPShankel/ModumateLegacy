// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMDataModel.h"

namespace Modumate { namespace BIM {
/*
		Crafting trees are acyclical networks of crafting node instances
		They have their own input pin sets and properties, both of which are inherited from the preset used to create the node
		If a node's properties or child configuration are inconsistent with the base preset, the preset is considered 'dirty' and must be updated or branched
		*/
class MODUMATE_API FCraftingTreeNodeInstance : public TSharedFromThis<FCraftingTreeNodeInstance>
{
	friend class MODUMATE_API FCraftingTreeNodeInstancePool;

#if WITH_EDITOR // for debugging new/delete balance
	static int32 InstanceCount;
#endif

private:
	// Node instances are managed solely by the object node set and should not be passed around by value or constructed by anyone else
	FCraftingTreeNodeInstance() = delete;
	FCraftingTreeNodeInstance(const FCraftingTreeNodeInstance &rhs) = delete;
	FCraftingTreeNodeInstance &operator=(const FCraftingTreeNodeInstance &rhs) = delete;
	FCraftingTreeNodeInstance(int32 instanceID);


	int32 InstanceID;

	ECraftingResult GatherAllChildNodes(TArray<FCraftingTreeNodeInstanceSharedPtr> &OutChildren);

public:
	~FCraftingTreeNodeInstance()
	{
#if WITH_EDITOR // for debugging new/delete balance
		--InstanceCount;
#endif
	};

	FName PresetID;
	FCraftingTreeNodeInstanceWeakPtr ParentInstance;

	// TODO: privatize and report 'preset dirty' state
	TArray<FCraftingTreeNodePinSet> InputPins;
	FBIMPropertySheet InstanceProperties;

	// May be fixed in type definition, inherited from parent or switchable
	EConfiguratorNodeIconOrientation CurrentOrientation;

	bool CanFlipOrientation = false;

	int32 GetInstanceID() const;

	ECraftingNodePresetStatus GetPresetStatus(const FCraftingPresetCollection &PresetCollection) const;

	ECraftingResult ToDataRecord(FCustomAssemblyCraftingNodeRecord &OutRecord) const;
	ECraftingResult FromDataRecord(FCraftingTreeNodeInstancePool &InstancePool, const FCraftingPresetCollection &PresetCollection, const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets);

	bool CanRemoveChild(const FCraftingTreeNodeInstanceSharedPtrConst &Child) const;

	ECraftingResult DetachSelfFromParent();

	ECraftingResult AttachChild(const FCraftingPresetCollection &PresetCollection, const FCraftingTreeNodeInstanceSharedPtr &Child);
	ECraftingResult FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition);
	ECraftingResult FindChildOrder(int32 ChildID, int32 &Order);
};

/*
A Crafting Tree Node Set contains all the context information for node crafting.
Node descriptors, preset definitions and preset lists are read in via ParseScriptFile
The crafting widget and preset manager use this node set to implement the crafting interface

TODO: this functionality to be merged into the crafting widget which is the only client of node instances
*/

class MODUMATE_API FCraftingTreeNodeInstancePool
{
private:
	int32 NextInstanceID = 1;
	TMap<int32, FCraftingTreeNodeInstanceWeakPtr> InstanceMap;
	TArray<FCraftingTreeNodeInstanceSharedPtr> InstancePool;

	// Does not create a fully resolved node, used in FromDataRecord only
	FCraftingTreeNodeInstanceSharedPtr CreateNodeInstanceFromDataRecord(const FCraftingPresetCollection &PresetCollection, const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets);

public:

	const TArray<FCraftingTreeNodeInstanceSharedPtr> &GetInstancePool() const { return InstancePool; }

	ECraftingResult ResetInstances();

	FCraftingTreeNodeInstanceSharedPtr CreateNodeInstanceFromPreset(const FCraftingPresetCollection &PresetCollection, int32 ParentID, const FName &PresetID, bool CreateDefaultReadOnlyChildren);
	ECraftingResult SetNewPresetForNode(const FCraftingPresetCollection &PresetCollection, int32 InstanceID, const FName &PresetID);

	ECraftingResult DestroyNodeInstance(const FCraftingTreeNodeInstanceSharedPtr &Instance, TArray<int32> &OutDestroyed);
	ECraftingResult DestroyNodeInstance(int32 InstanceID, TArray<int32> &OutDestroyed);

	ECraftingResult PresetToSpec(const FName &PresetID, const FCraftingPresetCollection &PresetCollection, FModumateAssemblyPropertySpec &OutPropertySpec) const;

	const FCraftingTreeNodeInstanceSharedPtr InstanceFromID(int32 InstanceID) const;
	FCraftingTreeNodeInstanceSharedPtr InstanceFromID(int32 InstanceID);
	FCraftingTreeNodeInstanceSharedPtr FindInstanceByPredicate(const std::function<bool(const FCraftingTreeNodeInstanceSharedPtr &Instance)> &Predicate);

	bool FromDataRecord(const FCraftingPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords, bool RecursePresets);
	bool ToDataRecord(TArray<FCustomAssemblyCraftingNodeRecord> &OutDataRecords) const;

	bool ValidatePool() const;
};
} }