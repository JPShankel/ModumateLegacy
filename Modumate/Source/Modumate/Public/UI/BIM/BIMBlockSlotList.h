// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "BIMBlockSlotList.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UBIMBlockSlotList : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockSlotList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class UBIMBlockNode* ParentBIMNode;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* VerticalBoxSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockSlotListItem> SlotListItemClass;

	// Typedef'd as FBIMEditorNodeIDType...UPROPERTIES don't support typedef
	UPROPERTY()
	TMap<FName, class UBIMBlockSlotListItem*> NodeIDToSlotMapItem;

	void BuildSlotAssignmentList(const FBIMPresetEditorNodeSharedPtr& NodePtr);
	void ReleaseSlotAssignmentList();
};
