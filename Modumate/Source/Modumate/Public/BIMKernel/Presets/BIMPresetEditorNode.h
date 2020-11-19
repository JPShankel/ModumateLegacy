// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

UENUM()
enum class EBIMPresetEditorNodeStatus : uint8
{
	None = 0,
	UpToDate,
	Dirty,
	Pending
};

class FBIMPresetEditorNode;

typedef TSharedPtr<FBIMPresetEditorNode> FBIMPresetEditorNodeSharedPtr;
typedef TWeakPtr<FBIMPresetEditorNode> FBIMPresetEditorNodeWeakPtr;

typedef TSharedPtr<const FBIMPresetEditorNode> FBIMPresetEditorNodeSharedPtrConst;
typedef TWeakPtr<const FBIMPresetEditorNode> FBIMPresetEditorNodeWeakPtrConst;

class MODUMATE_API FBIMPresetEditorNode : public TSharedFromThis<FBIMPresetEditorNode>
{
	friend class FBIMPresetEditor;

#if WITH_EDITOR // for debugging new/delete balance
	static int32 InstanceCount;
#endif

private:
	// Node instances are managed solely by FBIMPresetEditor and should not be passed around by value or constructed by anyone else
	FBIMPresetEditorNode() = delete;
	FBIMPresetEditorNode(const FBIMPresetEditorNode& rhs) = delete;
	FBIMPresetEditorNode& operator=(const FBIMPresetEditorNode& rhs) = delete;
	FBIMPresetEditorNode(int32 instanceID,int32 InPinSetIndex,int32 InPinSetPosition,const FBIMPresetInstance& InPreset);

	int32 InstanceID;

	EBIMResult SortChildren();
	EBIMResult UpdateAddButtons();
	int32 GetNumChildrenOnPin(int32 InPin) const;
	EBIMResult DetachSelfFromParent();

	bool ValidateNode() const;

public:
	~FBIMPresetEditorNode()
	{
#if WITH_EDITOR // for debugging new/delete balance
		--InstanceCount;
#endif
	};

	FBIMPresetEditorNodeWeakPtr ParentInstance;
	FText CategoryTitle = FText::FromString(TEXT("Unknown Category"));

	FBIMPresetInstance OriginalPresetCopy, WorkingPresetCopy;
	int32 MyParentPinSetIndex = INDEX_NONE;
	int32 MyParentPinSetPosition = INDEX_NONE;
	TArray<FBIMPresetEditorNodeWeakPtr> ChildNodes;

	FBIMKey MyParentPartSlot;
	TArray<FBIMPresetEditorNodeWeakPtr> PartNodes;

	bool bWantAddButton = false;

	int32 GetInstanceID() const;
	EBIMResult NodeIamEmbeddedIn(int32& OutNodeId) const;
	EBIMResult NodesEmbeddedInMe(TArray<int32>& OutNodeIds) const;

	EBIMPresetEditorNodeStatus GetPresetStatus() const;

	bool CanRemoveChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const;
	bool CanReorderChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const;

	EBIMResult AddChildPreset(const FBIMPresetEditorNodeSharedPtr& Child, int32 PinSetIndex, int32 PinSetPosition);

	EBIMResult GetPartSlots(TArray<FBIMPresetPartSlot>& OutPartSlots) const;
	EBIMResult SetPartSlotPreset(const FBIMKey& SlotPreset, const FBIMKey& PartPreset);
	EBIMResult ClearPartSlot(const FBIMKey& SlotPreset);

	EBIMResult FindChild(int32 ChildID, int32& OutPinSetIndex, int32& OutPinSetPosition) const;
	EBIMResult FindOtherChildrenOnPin(TArray<int32>& OutChildIDs) const;
	EBIMResult GatherChildrenInOrder(TArray<int32>& OutChildIDs) const;
	EBIMResult GatherAllChildNodes(TArray<FBIMPresetEditorNodeSharedPtr>& OutChildren);
};