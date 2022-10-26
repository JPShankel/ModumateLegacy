// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VDPTable.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"


class FBIMPresetEditorNode;

using FBIMPresetEditorNodeSharedPtr = TSharedPtr<FBIMPresetEditorNode>;
using FBIMPresetEditorNodeWeakPtr = TWeakPtr<FBIMPresetEditorNode>;

using FBIMPresetEditorNodeSharedPtrConst = TSharedPtr<const FBIMPresetEditorNode>;
using FBIMPresetEditorNodeWeakPtrConst = TWeakPtr<const FBIMPresetEditorNode>;

using FBIMEditorNodeIDType = FName;
static const FBIMEditorNodeIDType BIM_ID_NONE = NAME_None;

class UModumateDocument;

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
	FBIMPresetEditorNode(const FBIMEditorNodeIDType& InInstanceID,int32 InPinSetIndex,int32 InPinSetPosition,const FBIMPresetInstance& InPreset, const FVDPTable* VdpTable);

	FBIMEditorNodeIDType InstanceID;

	EBIMResult SortChildren();
	EBIMResult UpdateAddButtons();
	int32 GetNumChildrenOnPin(int32 InPin) const;
	EBIMResult DetachSelfFromParent();

	bool ValidateNode() const;

	const FVDPTable* VDPTable; 

public:
	~FBIMPresetEditorNode()
	{
#if WITH_EDITOR // for debugging new/delete balance
		--InstanceCount;
#endif
	};

	FBIMPresetEditorNodeWeakPtr ParentInstance;
	FText CategoryTitle;

	FBIMPresetInstance Preset;
	int32 MyParentPinSetIndex = INDEX_NONE;
	int32 MyParentPinSetPosition = INDEX_NONE;
	TArray<FBIMPresetEditorNodeWeakPtr> ChildNodes;

	FGuid MyParentPartSlot;
	TArray<FBIMPresetEditorNodeWeakPtr> PartNodes;

	TArray<FString> VisibleNamedDimensions;
	bool bWantAddButton = false;

	EBIMResult GetPresetForm(const UModumateDocument* InDocument, FBIMPresetForm& OutForm) const;

	FBIMEditorNodeIDType GetInstanceID() const;

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