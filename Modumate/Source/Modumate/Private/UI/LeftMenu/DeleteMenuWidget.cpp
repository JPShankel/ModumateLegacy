// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/DeleteMenuWidget.h"

#include "UI/LeftMenu/NCPNavigator.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"

#define LOCTEXT_NAMESPACE "ModalDialogDelete"

UDeleteMenuWidget::UDeleteMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDeleteMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ButtonClose)
	{
		return false;
	}

	ButtonClose->ModumateButton->OnReleased.AddDynamic(this, &UDeleteMenuWidget::OnReleaseButtonClose);

	return true;
}

void UDeleteMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UDeleteMenuWidget::BuildDeleteMenu()
{
	NCPNavigatorWidget->BuildNCPNavigator(EPresetCardType::Delete);
}

void UDeleteMenuWidget::SetPresetToDelete(const FGuid& InPresetToDelete, const FGuid& InPresetToReplace)
{
	PresetGUIDToDelete = InPresetToDelete;
	PresetGUIDReplacement = InPresetToReplace;

	NCPNavigatorWidget->EmptyIgnoredPresets();
	NCPNavigatorWidget->AddToIgnoredPresets(TArray<FGuid> {PresetGUIDToDelete});
}

void UDeleteMenuWidget::BuildDeleteModalDialog()
{
	const FBIMPresetInstance* presetDelete = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUIDToDelete);
	const FBIMPresetInstance* presetReplace = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUIDReplacement);
	FText descText;
	if (presetDelete && presetReplace)
	{
		descText = FText::Format(LOCTEXT("ReplaceFormat", "Delete {0} and replace with {1}"),
			presetDelete->DisplayName, presetReplace->DisplayName);
	}
	else if (presetDelete)
	{
		descText = FText::Format(LOCTEXT("DeleteFormat", "Delete ({0})"), presetDelete->DisplayName);
	}

	FText confirmText = LOCTEXT("ConfirmDelete", "Confirm");
	auto weakThis = MakeWeakObjectPtr<UDeleteMenuWidget>(this);
	auto deferredDelete = [weakThis]() {
		if (weakThis.IsValid())
		{
			weakThis->ModalDeleteButtonConfirmReleased();
		}
	};
	EMPlayerController->EditModelUserWidget->ModalDialogWidgetBP->ShowDeletePresetDialog(descText, confirmText, deferredDelete);
}

void UDeleteMenuWidget::ModalDeleteButtonConfirmReleased()
{
	EMPlayerController->GetDocument()->DeletePreset(GetWorld(), PresetGUIDToDelete, PresetGUIDReplacement);
	EMPlayerController->EditModelUserWidget->SwitchLeftMenu(EMPlayerController->EditModelUserWidget->PreviousLeftMenuState);
}

void UDeleteMenuWidget::OnReleaseButtonClose()
{
	EMPlayerController->EditModelUserWidget->SwitchLeftMenu(EMPlayerController->EditModelUserWidget->PreviousLeftMenuState);
}

#undef LOCTEXT_NAMESPACE