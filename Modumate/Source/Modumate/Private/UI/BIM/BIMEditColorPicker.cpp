// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMEditColorPicker.h"
#include "UI/BIM/BIMDesigner.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
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

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

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

void UBIMEditColorPicker::BuildColorPicker(class UBIMDesigner* OuterBIMDesigner, const FName& InOwnerNodeID, const EBIMValueScope& InScope, const FBIMNameType& InNameType, const FBIMKey& InColorBIMKey, const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNodeID = InOwnerNodeID;
	OwnerNode = ParentBIMDesigner->IdToNodeMap.FindRef(OwnerNodeID);
	if (!ensure(OwnerNode))
	{
		return;
	}
	SwapScope = InScope;
	SwapNameType = InNameType;
	DefaultColorBIMKey = InColorBIMKey;
	DropdownOffset = InDropdownOffset;

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

	// Convert color key from hex
	FLinearColor newColor = FColor::FromHex(DefaultColorBIMKey.ToString());
	UpdateCurrentColor(newColor);

	// Build children widgets
	ColorBar_Widget_BP->BuildColorBar(CurrentColor, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentColor, this);
}

void UBIMEditColorPicker::EditColorFromHue(float InHueValue)
{
	ColorMap_Widget_BP->UpdateColorMapGradient(InHueValue);

	FLinearColor currentHSV = CurrentColor.LinearRGBToHSV();
	currentHSV.R = InHueValue;
	FLinearColor newCurrentColor = currentHSV.HSVToLinearRGB();
	UpdateCurrentColor(newCurrentColor);
}

void UBIMEditColorPicker::EditColorFromSaturationAndBrightness(float InSaturation, float InBrightness)
{
	FLinearColor currentHSV = CurrentColor.LinearRGBToHSV();
	currentHSV.G = InSaturation;
	currentHSV.B = InBrightness;
	FLinearColor newCurrentColor = currentHSV.HSVToLinearRGB();
	UpdateCurrentColor(newCurrentColor);
}

void UBIMEditColorPicker::EditColorFromRed(int32 InRedValue)
{
	FColor newColor = CurrentColor.ToFColor(true);
	newColor.R = InRedValue;
	UpdateCurrentColor(FLinearColor(newColor));

	ColorBar_Widget_BP->BuildColorBar(CurrentColor, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentColor, this);
}

void UBIMEditColorPicker::EditColorFromGreen(int32 InGreenValue)
{
	FColor newColor = CurrentColor.ToFColor(true);
	newColor.G = InGreenValue;
	UpdateCurrentColor(FLinearColor(newColor));

	ColorBar_Widget_BP->BuildColorBar(CurrentColor, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentColor, this);
}

void UBIMEditColorPicker::EditColorFromBlue(int32 InBlueValue)
{
	FColor newColor = CurrentColor.ToFColor(true);
	newColor.B = InBlueValue;
	UpdateCurrentColor(FLinearColor(newColor));

	ColorBar_Widget_BP->BuildColorBar(CurrentColor, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentColor, this);
}

void UBIMEditColorPicker::EditColorFromHex(const FString& InHexValue)
{
	FLinearColor newColor = FColor::FromHex(InHexValue);
	UpdateCurrentColor(newColor);

	ColorBar_Widget_BP->BuildColorBar(CurrentColor, this);
	ColorMap_Widget_BP->BuildColorMap(CurrentColor, this);
}

void UBIMEditColorPicker::UpdateCurrentColor(const FLinearColor& InCurrentColor)
{
	CurrentColor = InCurrentColor;
	Image_CurrentColor->SetColorAndOpacity(CurrentColor);

	// TODO: Save previous color history

	FColor readColor = CurrentColor.ToFColor(true);
	R_TextBox->ChangeText(FText::FromString(FString::FromInt(readColor.R)));
	G_TextBox->ChangeText(FText::FromString(FString::FromInt(readColor.G)));
	B_TextBox->ChangeText(FText::FromString(FString::FromInt(readColor.B)));
	Hex_TextBox->ChangeText(FText::FromString(readColor.ToHex()));
}

void UBIMEditColorPicker::UpdateParentNodeProperty()
{
	FColor readColor = CurrentColor.ToFColor(true);
	ParentBIMDesigner->SetNodeProperty(OwnerNode->ID, SwapScope, SwapNameType, readColor.ToHex());

	// Reopen color picker after BIM Designer update from SetNodeProperty
	BuildColorPicker(ParentBIMDesigner, OwnerNodeID, SwapScope, SwapNameType, readColor.ToHex(), DropdownOffset);
}
