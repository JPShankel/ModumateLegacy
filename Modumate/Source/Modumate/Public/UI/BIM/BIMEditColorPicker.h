// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMProperties.h"

#include "BIMEditColorPicker.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMEditColorPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMEditColorPicker(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController_CPP* Controller;

	FName OwnerNodeID;
	FVector2D DropdownOffset = FVector2D::ZeroVector;
	FString CurrentColorHex;
	EBIMValueScope SwapScope = EBIMValueScope::None;
	FBIMNameType SwapNameType = NAME_None;
	FLinearColor CurrentHSV = FLinearColor::Black;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* R_TextBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* G_TextBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* B_TextBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* Hex_TextBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMEditColorBar* ColorBar_Widget_BP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMEditColorMap* ColorMap_Widget_BP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* Image_CurrentColor;

	UPROPERTY()
	class UBIMDesigner* ParentBIMDesigner;

	UFUNCTION()
	void OnRTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnGTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnBTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnHexTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void BuildColorPicker(class UBIMDesigner* OuterBIMDesigner, const FName& InOwnerNodeID, const EBIMValueScope& InScope, const FBIMNameType& InNameType, const FString& InColorBIMKey, const FVector2D& InDropdownOffset);
	void UpdateColorPicker();
	void EditColorFromHue(float InHueValue);
	void EditColorFromSaturationAndBrightness(float InSaturation, float InBrightness);
	void EditColorFromRed(int32 InRedValue);
	void EditColorFromGreen(int32 InGreenValue);
	void EditColorFromBlue(int32 InBlueValue);
	void EditColorFromHex(const FString& InHexValue);

	void UpdateCurrentHSV(const FLinearColor& InCurrentHSV);
	void UpdateParentNodeProperty();
};
