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
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/AxesActor.h"
#include "UI/ViewCubeWidget.h"
#include "DocumentManagement/ModumateDocument.h"


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
	SavedViewEdiableTitle->ModumateEditableTextBox->SetText(FText::FromString(CameraView.Name));
	SavedViewEdiableTitle->ModumateEditableTextBox->SetKeyboardFocus();
}

void UComponentSavedViewListItem::OnButtonMainReleased()
{
	ActivateCameraView();
}

void UComponentSavedViewListItem::OnButtonDeleteReleased()
{
	UModumateBrowserStatics::RemoveCameraViewMoi(this, CameraView.MoiId);
}

void UComponentSavedViewListItem::OnButtonUpdateReleased()
{
	UCameraComponent *cameraComp = Controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
	if (cameraComp)
	{
		FDateTime dateTime = Controller->SkyActor->GetCurrentDateTime();
		UModumateBrowserStatics::UpdateCameraViewAsMoi(this, cameraComp, CameraView.MoiId, dateTime);
	}
}

void UComponentSavedViewListItem::OnEditableTitleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Lose focus if this commit is not entering its value
	if (!(CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	UModumateBrowserStatics::EditCameraViewName(this, CameraView.MoiId, Text.ToString());
}

void UComponentSavedViewListItem::ActivateCameraView()
{
	APawn *pawn = Controller->GetPawn();
	if (pawn)
	{
		// Position and rotation
		pawn->SetActorLocationAndRotation(CameraView.Position, CameraView.Rotation);

		// Projection mode
		Controller->EMPlayerPawn->SetCameraOrtho(CameraView.bOrthoView);

		// FOV and ortho width
		UCameraComponent* cameraComp = Controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
		if (cameraComp)
		{
			cameraComp->FieldOfView = CameraView.FOV;
			cameraComp->SetOrthoWidth(CameraView.OrthoWidth);
		}

		// Time
		FDateTime newDateTime;
		if (FDateTime::ParseIso8601(*CameraView.SavedTime, newDateTime))
		{
			Controller->SkyActor->SetCurrentDateTime(newDateTime);
		}

		// Axis actor
		if (Controller->AxesActor)
		{
			Controller->AxesActor->SetActorHiddenInGame(!CameraView.bAxesActorVisibility);
		}

		// Viewcube
		if (Controller->EditModelUserWidget->ViewCubeUserWidget && Controller->EditModelUserWidget->ViewCubeUserWidget)
		{
			Controller->EditModelUserWidget->ViewCubeUserWidget->SetVisibility(CameraView.bViewCubeVisibility ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}

		// Sync with menu
		Controller->EditModelUserWidget->ViewMenu->ViewMenu_Block_Properties->SyncAllMenuProperties();
		Controller->EditModelUserWidget->ViewMenu->MouseEndHoverView(this);

		// Update cutplane culling
		Controller->SetCurrentCullingCutPlane(CameraView.SavedCullingCutPlane, false);

		// Update cutplane visibility
		TArray<int32> visibleCPs;
		TArray<int32> hiddenCPs;
		for (const auto& kvp : CameraView.SavedCutPlaneVisibilities)
		{
			if (kvp.Value)
			{
				visibleCPs.Add(kvp.Key);
			}
			else
			{
				hiddenCPs.Add(kvp.Key);
			}
		}

		auto* playerState = Controller->GetPlayerState<AEditModelPlayerState>();
		playerState->AddHideObjectsById(hiddenCPs);
		playerState->UnhideObjectsById(visibleCPs);
		Controller->GetDocument()->OnCameraViewSelected(CameraView.MoiId);
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
	ListNumber->ChangeText(FText::AsNumber(viewListObj->CameraView.CameraViewIndex));
	SavedViewEdiableTitle->ChangeText(FText::FromString(CameraView.Name));
}

