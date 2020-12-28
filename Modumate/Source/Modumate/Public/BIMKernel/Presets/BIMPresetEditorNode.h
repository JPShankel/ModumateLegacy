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

using FBIMPresetEditorNodeSharedPtr = TSharedPtr<FBIMPresetEditorNode>;
using FBIMPresetEditorNodeWeakPtr = TWeakPtr<FBIMPresetEditorNode>;

using FBIMPresetEditorNodeSharedPtrConst = TSharedPtr<const FBIMPresetEditorNode>;
using FBIMPresetEditorNodeWeakPtrConst = TWeakPtr<const FBIMPresetEditorNode>;

using FBIMEditorNodeIDType = FName;
static const FBIMEditorNodeIDType BIM_ID_NONE = NAME_None;

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
	FBIMPresetEditorNode(const FBIMEditorNodeIDType& InInstanceID,int32 InPinSetIndex,int32 InPinSetPosition,const FBIMPresetInstance& InPreset);

	FBIMEditorNodeIDType InstanceID;

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

	FGuid MyParentPartSlot;
	TArray<FBIMPresetEditorNodeWeakPtr> PartNodes;

	bool bWantAddButton = false;

	FBIMEditorNodeIDType GetInstanceID() const;

	EBIMPresetEditorNodeStatus GetPresetStatus() const;

	bool CanRemoveChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const;
	bool CanReorderChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const;

	EBIMResult AddChildPreset(const FBIMPresetEditorNodeSharedPtr& Child, int32 PinSetIndex, int32 PinSetPosition);

	EBIMResult GetPartSlots(TArray<FBIMPresetPartSlot>& OutPartSlots) const;

	EBIMResult FindChild(const FBIMEditorNodeIDType& ChildID, int32& OutPinSetIndex, int32& OutPinSetPosition) const;
	EBIMResult FindNodeIDConnectedToSlot(const FGuid& SlotPreset, FBIMEditorNodeIDType& OutChildID) const;
	EBIMResult FindOtherChildrenOnPin(TArray<FBIMEditorNodeIDType>& OutChildIDs) const;
	EBIMResult GatherChildrenInOrder(TArray<FBIMEditorNodeIDType>& OutChildIDs) const;
	EBIMResult GatherAllChildNodes(TArray<FBIMPresetEditorNodeSharedPtr>& OutChildren);
};