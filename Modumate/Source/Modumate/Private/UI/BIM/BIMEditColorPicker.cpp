// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMEditColorPicker.h"
#include "UI/BIM/BIMDesigner.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/BIM/BIMBlockNode.h"
#include "UI/BIM/BIMEditColorBar.h"
#include "UI/BIM/BIMEditColorMap.h"
#include "Components/Image.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"

UBIMEditColorPicker::UBIMEditColorPicker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMEditColorPicker::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!(R_TextBox && G_TextBox && B_TextBox && Hex_TextBox))
	{
		return false;
	}

	R_TextBox->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UBIMEditColorPicker::OnRTextBoxCommitted);
	G_TextBox->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UBIMEditColorPicker::OnGTextBoxCommitted);
	B_TextBox->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UBIMEditColorPicker::OnBTextBoxCommitted);
	Hex_TextBox->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UBIMEditColorPicker::OnHexTextBoxCommitted);

	return true;
}

void UBIMEditColorPicker::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMEditColorPicker::OnRTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		FString valueString = Text.ToString();
		EditColorFromRed(FCString::Atoi(*valueString));
		UpdateParentNodeProperty();
	}
}

void UBIMEditColorPicker::OnGTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		FString valueString = Text.ToString();
		EditColorFromGreen(FCString::Atoi(*valueString));
		UpdateParentNodeProperty();
	}
}

void UBIMEditColorPicker::OnBTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		FString valueString = Text.ToString();
		EditColorFromBlue(FCString::Atoi(*valueString));
		UpdateParentNodeProperty();
	}
}

void UBIMEditColorPicker::OnHexTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		EditColorFromHex(Text.ToString());
		UpdateParentNodeProperty();
	}
}

void UBIMEditColorPicker::BuildColorPicker(class UBIMDesigner* OuterBIMDesigner, const FName& InOwnerNodeID, const EBIMValueScope& InScope, const FBIMNameType& InNameType, const FString& InColorBIMKey, const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNodeID = InOwnerNodeID;
	SwapScope = InScope;
	SwapNameType = InNameType;
	CurrentColorHex = InColorBIMKey;
	DropdownOffset = InDropdownOffset;

	// Convert color key from hex
	FLinearColor newColor = FColor::FromHex(CurrentColorHex);
	FLinearColor newHSV = newColor.LinearRGBToHSV();
	UpdateCurrentHSV(newHSV);

	UpdateColorPicker();
}

void UBIMEditColorPicker::UpdateColorPicker()
{
	UBIMBlockNode* OwnerNode = ParentBIMDesigner->IdToNodeMap.FindRef(OwnerNodeID);
	if (!ensure(OwnerNode))
	{
		return;
	}

	ParentBIMDesigner->ToggleColorPickerVisibility(true);
	UCanvasPanelSlot* nodeCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(OwnerNode);
	UCanvasPanelSlot* swapCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this);
	if (nodeCanvasSlot && swapCanvasSlot)
	{
		FVector2D newPosition = nodeCanvasSlot->GetPosition();
		if (OwnerNode->NodeDirty)
		{
			newPosition.Y += OwnerNode->DirtyTabSize;
		}
		newPosition += DropdownOffset;
		swapCanvasSlot->SetPosition(newPosition);
	}

	// Build children widgets
	ColorBar_Widget_BP->BuildColorBar(CurrentHSV, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentHSV, this);
}

void UBIMEditColorPicker::EditColorFromHue(float InHueValue)
{
	ColorMap_Widget_BP->UpdateColorMapGradient(InHueValue);

	FLinearColor newCurrentHSV = CurrentHSV;
	newCurrentHSV.R = InHueValue;
	UpdateCurrentHSV(newCurrentHSV);
}

void UBIMEditColorPicker::EditColorFromSaturationAndBrightness(float InSaturation, float InBrightness)
{
	FLinearColor newCurrentHSV = CurrentHSV;
	newCurrentHSV.G = InSaturation;
	newCurrentHSV.B = InBrightness;
	UpdateCurrentHSV(newCurrentHSV);
}

void UBIMEditColorPicker::EditColorFromRed(int32 InRedValue)
{
	FColor newColor = CurrentHSV.HSVToLinearRGB().ToFColor(true);
	newColor.R = InRedValue;
	UpdateCurrentHSV(FLinearColor(newColor).LinearRGBToHSV());

	ColorBar_Widget_BP->BuildColorBar(CurrentHSV, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentHSV, this);
}

void UBIMEditColorPicker::EditColorFromGreen(int32 InGreenValue)
{
	FColor newColor = CurrentHSV.HSVToLinearRGB().ToFColor(true);
	newColor.G = InGreenValue;
	UpdateCurrentHSV(FLinearColor(newColor).LinearRGBToHSV());

	ColorBar_Widget_BP->BuildColorBar(CurrentHSV, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentHSV, this);
}

void UBIMEditColorPicker::EditColorFromBlue(int32 InBlueValue)
{
	FColor newColor = CurrentHSV.HSVToLinearRGB().ToFColor(true);
	newColor.B = InBlueValue;
	UpdateCurrentHSV(FLinearColor(newColor).LinearRGBToHSV());

	ColorBar_Widget_BP->BuildColorBar(CurrentHSV, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentHSV, this);
}

void UBIMEditColorPicker::EditColorFromHex(const FString& InHexValue)
{
	FLinearColor newColor = FColor::FromHex(InHexValue);
	UpdateCurrentHSV(newColor.LinearRGBToHSV());

	ColorBar_Widget_BP->BuildColorBar(CurrentHSV, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentHSV, this);
}

void UBIMEditColorPicker::UpdateCurrentHSV(const FLinearColor& InCurrentHSV)
{
	CurrentHSV = InCurrentHSV;
	FLinearColor newCurrentColor = CurrentHSV.HSVToLinearRGB();
	newCurrentColor.A = 1.f;
	Image_CurrentColor->SetColorAndOpacity(newCurrentColor);

	// TODO: Save previous color history

	FColor readColor = newCurrentColor.ToFColor(true);
	R_TextBox->ChangeText(FText::FromString(FString::FromInt(readColor.R)));
	G_TextBox->ChangeText(FText::FromString(FString::FromInt(readColor.G)));
	B_TextBox->ChangeText(FText::FromString(FString::FromInt(readColor.B)));
	Hex_TextBox->ChangeText(FText::FromString(readColor.ToHex().Left(6)));
}

void UBIMEditColorPicker::UpdateParentNodeProperty()
{
	FColor readColor = CurrentHSV.HSVToLinearRGB().ToFColor(true);
	ParentBIMDesigner->SetNodeProperty(OwnerNodeID, SwapScope, SwapNameType, readColor.ToHex());

	// Reopen color picker after BIM Designer update from SetNodeProperty
	UpdateColorPicker();
}
