// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardHeader.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "Components/Image.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"

#define LOCTEXT_NAMESPACE "PresetCardHeader"

UPresetCardHeader::UPresetCardHeader(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardHeader::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// TODO: Create button widget as needed by EPresetCardType
	if (!(ButtonEdit && ButtonSwap && ButtonConfirm))
	{
		return false;
	}

	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonEditReleased);
	ButtonSwap->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonSwapReleased);
	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonConfirmReleased);

	return true;
}

void UPresetCardHeader::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardHeader::BuildAsBrowserHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		CaptureIcon(PresetGUID, NodeID, preset->NodeScope == EBIMValueScope::Assembly);
		ItemDisplayName = preset->DisplayName;
		MainText->ChangeText(ItemDisplayName);
		UpdateButtonSetByPresetCardType(EPresetCardType::Browser);
	}
}

void UPresetCardHeader::BuildAsSelectTrayPresetCard(const FGuid& InGUID, int32 ItemCount)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		CaptureIcon(PresetGUID, BIM_ID_NONE, preset->NodeScope == EBIMValueScope::Assembly);
		ItemDisplayName = preset->DisplayName;
		UpdateSelectionHeaderItemCount(ItemCount);
		UpdateButtonSetByPresetCardType(EPresetCardType::SelectTray);
	}
}

void UPresetCardHeader::BuildAsSelectTrayPresetCardObjectType(EObjectType InObjectType, int32 ItemCount)
{
	ItemDisplayName = UModumateTypeStatics::GetTextForObjectType(InObjectType);
	UpdateSelectionHeaderItemCount(ItemCount);

	// PresetCards that represent objectType do not have interactions
	UpdateButtonSetByPresetCardType(EPresetCardType::None);
}

void UPresetCardHeader::UpdateButtonSetByPresetCardType(EPresetCardType InPresetCardType)
{
	// TODO: Create button widget as needed by EPresetCardType
	switch (InPresetCardType)
	{
	case EPresetCardType::Browser:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Visible);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EPresetCardType::SelectTray:
		ButtonSwap->SetVisibility(ESlateVisibility::Visible);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Visible);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EPresetCardType::None:
	default:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		break;
	}
}

void UPresetCardHeader::UpdateSelectionHeaderItemCount(int32 ItemCount)
{
	if (ItemCount > 1)
	{
		MainText->ChangeText(FText::Format(LOCTEXT("ItemCount", "({0}) {1}"), ItemCount, ItemDisplayName));
	}
	else
	{
		MainText->ChangeText(ItemDisplayName);
	}
}

bool UPresetCardHeader::CaptureIcon(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAsAssembly)
{
	if (!(EMPlayerController && IconImageSmall))
	{
		return false;
	}

	bool result = false;
	if (bAsAssembly)
	{
		result = EMPlayerController->DynamicIconGenerator->SetIconMeshForAssembly(InGUID, IconMaterial);
	}
	else
	{
		result = EMPlayerController->DynamicIconGenerator->SetIconMeshForBIMDesigner(true, InGUID, IconMaterial, NodeID);
	}

	if (result)
	{
		IconImageSmall->SetBrushFromMaterial(IconMaterial);
	}
	IconImageSmall->SetVisibility(result ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	return result;
}

void UPresetCardHeader::OnButtonEditReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->EditExistingAssembly(PresetGUID);
	}
}

void UPresetCardHeader::OnButtonSwapReleased()
{

}

void UPresetCardHeader::OnButtonConfirmReleased()
{

}

#undef LOCTEXT_NAMESPACE