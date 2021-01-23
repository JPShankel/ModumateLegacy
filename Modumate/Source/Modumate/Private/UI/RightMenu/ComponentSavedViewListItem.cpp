// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ComponentSavedViewListItem.h"
#include "UI/RightMenu/ComponentSavedViewListItemObject.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "ModumateCore/ModumateBrowserStatics.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/ViewMenuWidget.h"
#include "UI/RightMenu/ViewMenuBlockSavedViews.h"
#include "UnrealClasses/SkyActor.h"
#include "UI/RightMenu/ViewMenuBlockProperties.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"


UComponentSavedViewListItem::UComponentSavedViewListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UComponentSavedViewListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonMain && ButtonDelete && ButtonUpdate &&ButtonEdit && SavedViewEdiableTitle))
	{
		return false;
	}

	SavedViewEdiableTitle->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UComponentSavedViewListItem::OnEditableTitleCommitted);
	ButtonMain->OnReleased.AddDynamic(this, &UComponentSavedViewListItem::OnButtonMainReleased);
	ButtonDelete->ModumateButton->OnReleased.AddDynamic(this, &UComponentSavedViewListItem::OnButtonDeleteReleased);
	ButtonUpdate->ModumateButton->OnReleased.AddDynamic(this, &UComponentSavedViewListItem::OnButtonUpdateReleased);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UComponentSavedViewListItem::OnButtonEditReleased);

	return true;
}

void UComponentSavedViewListItem::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Controller->EditModelUserWidget->ViewMenu->MouseOnHoverView(this);
}

void UComponentSavedViewListItem::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Controller->EditModelUserWidget->ViewMenu->MouseEndHoverView(this);
}

void UComponentSavedViewListItem::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController>();
}

void UComponentSavedViewListItem::OnButtonEditReleased()
{
	SavedViewEdiableTitle->ModumateEditableTextBox->SetKeyboardFocus();
}

void UComponentSavedViewListItem::OnButtonMainReleased()
{
	ActivateCameraView();
}

void UComponentSavedViewListItem::OnButtonDeleteReleased()
{
	UModumateBrowserStatics::RemoveCameraView(this, ID);
	Controller->EditModelUserWidget->ViewMenu->ViewMenu_Block_SavedViews->UpdateSavedViewsList();
}

void UComponentSavedViewListItem::OnButtonUpdateReleased()
{
	UCameraComponent *cameraComp = Controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
	if (cameraComp)
	{
		FDateTime dateTime = Controller->SkyActor->GetCurrentDateTime();
		UModumateBrowserStatics::SaveCameraView(this, cameraComp, CameraView.Name, dateTime, ID);
	}
	Controller->EditModelUserWidget->ViewMenu->ViewMenu_Block_SavedViews->UpdateSavedViewsList();
}

void UComponentSavedViewListItem::OnEditableTitleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Lose focus if this commit is not entering its value
	if (!(CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	UModumateBrowserStatics::EditCameraViewName(this, ID, Text.ToString());
	Controller->EditModelUserWidget->ViewMenu->ViewMenu_Block_SavedViews->UpdateSavedViewsList();
}

void UComponentSavedViewListItem::ActivateCameraView()
{
	APawn *pawn = Controller->GetPawn();
	if (pawn)
	{
		pawn->SetActorLocationAndRotation(CameraView.Position, CameraView.Rotation);
		Controller->SkyActor->SetCurrentDateTime(CameraView.TimeOfDay);
		Controller->EditModelUserWidget->ViewMenu->ViewMenu_Block_Properties->SyncTextBoxesWithSkyActorCurrentTime();
		Controller->EditModelUserWidget->ViewMenu->MouseEndHoverView(this);
	}
}

void UComponentSavedViewListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UComponentSavedViewListItemObject *viewListObj = Cast<UComponentSavedViewListItemObject>(ListItemObject);
	if (!viewListObj)
	{
		return;
	}
	CameraView = viewListObj->CameraView;
	ID = viewListObj->ID;
	ListNumber->ChangeText(FText::AsNumber(viewListObj->ID + 1));
	SavedViewEdiableTitle->ChangeText(FText::FromString(CameraView.Name));
}

