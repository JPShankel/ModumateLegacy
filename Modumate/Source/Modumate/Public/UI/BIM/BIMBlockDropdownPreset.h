// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMKey.h"

#include "BIMBlockDropdownPreset.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockDropdownPreset : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockDropdownPreset(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController_CPP* Controller;

	UPROPERTY()
	class UBIMDesigner* ParentBIMDesigner;

	UPROPERTY()
	class UBIMBlockNode* ParentNode;

	int32 EmbeddedParentID = INDEX_NONE;
	FVector2D DropdownOffset = FVector2D::ZeroVector;
	int32 NodeID = INDEX_NONE;
	FBIMKey PresetID;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* PresetText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonSwap;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	UPROPERTY()
	class UTexture* IconTexture;

	UFUNCTION()
	void OnButtonSwapReleased();

	void BuildDropdownFromProperty(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InParentNode, int32 InNodeID, int32 InEmbeddedParentID, FVector2D InDropdownOffset);
};
