// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModumateCore/ModumateTypes.h"

#include "BIMBlockSlotListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockSlotListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockSlotListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextBlockWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UButton* ButtonSlot;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ButtonImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node Texture")
	class UTexture2D* ConnectedTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node Texture")
	class UTexture2D* DisconnectedTexture;

	// Represents index of parent's PartSlots
	int32 SlotIndex = INDEX_NONE;

	// Owner of this slot
	int32 ParentID = MOD_ID_NONE;

	// The node connecting to this slot. None if this slot is empty
	int32 ConnectedNodeID = MOD_ID_NONE;

	void ConnectSlotItemToNode(int32 NodeID);

	UFUNCTION()
	void OnButtonSlotReleased();
};
