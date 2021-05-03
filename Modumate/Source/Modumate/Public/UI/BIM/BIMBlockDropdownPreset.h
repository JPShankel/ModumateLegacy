// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"

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
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class UBIMDesigner* ParentBIMDesigner;

	UPROPERTY()
	class UBIMBlockNode* OwnerNode;

	FVector2D DropdownOffset = FVector2D::ZeroVector;
	FGuid PresetGUID;
	FBIMPresetFormElement FormElement;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* PresetText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonSwap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget* DropdownList;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	UFUNCTION()
	void OnButtonSwapReleased();

	UFUNCTION()
	void OnDropdownListSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void BuildDropdownFromPropertyPreset(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement, const FVector2D& InDropdownOffset);
	void BuildDropdownFromColor(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement, const FVector2D& InDropdownOffset);
	void BuildDropdownFromStringList(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement, const FVector2D& InDropdownOffset);
};
