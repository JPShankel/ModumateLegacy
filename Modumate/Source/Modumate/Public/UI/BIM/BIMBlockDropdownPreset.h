// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMProperties.h"

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
	class UBIMBlockNode* OwnerNode;

	FVector2D DropdownOffset = FVector2D::ZeroVector;
	FBIMKey PresetID;
	EBIMValueScope SwapScope = EBIMValueScope::None;
	FBIMNameType SwapNameType = NAME_None;

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

	void BuildDropdownFromPropertyPreset(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const EBIMValueScope& InScope, const FBIMNameType& InNameType, FBIMKey InPresetID, FVector2D InDropdownOffset);
};
