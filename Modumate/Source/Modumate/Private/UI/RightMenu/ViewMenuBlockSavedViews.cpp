// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockSavedViews.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ModumateCore/ModumateBrowserStatics.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/RightMenu/ComponentSavedViewListItemObject.h"
#include "Components/ListView.h"
#include "UnrealClasses/SkyActor.h"


UViewMenuBlockSavedViews::UViewMenuBlockSavedViews(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UViewMenuBlockSavedViews::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ButtonAdd)
	{
		return false;
	}

	ButtonAdd->ModumateButton->OnReleased.AddDynamic(this, &UViewMenuBlockSavedViews::OnButtonAddReleased);
	return true;
}

void UViewMenuBlockSavedViews::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

void UViewMenuBlockSavedViews::OnButtonAddReleased()
{
	UCameraComponent *cameraComp = Controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
	if (cameraComp)
	{
		FDateTime dateTime = Controller->SkyActor->GetCurrentDateTime();
		FString newViewName = FString(TEXT("New Camera View ")) + FString::FromInt(SavedViewsList->GetNumItems() + 1);
		UModumateBrowserStatics::SaveCameraView(this, cameraComp, newViewName, dateTime);
	}
	UpdateSavedViewsList();
}

void UViewMenuBlockSavedViews::UpdateSavedViewsList()
{
	SavedViewsList->ClearListItems();
	TArray<FModumateCameraView> cameraViews;
	UModumateBrowserStatics::GetCameraViewsFromDoc(this, cameraViews);
	for (int32 i = 0; i < cameraViews.Num(); ++i)
	{
		UComponentSavedViewListItemObject *newListObj = NewObject<UComponentSavedViewListItemObject>(this);
		newListObj->CameraView = cameraViews[i];
		newListObj->ID = i;
		SavedViewsList->AddItem(newListObj);
	}
}
