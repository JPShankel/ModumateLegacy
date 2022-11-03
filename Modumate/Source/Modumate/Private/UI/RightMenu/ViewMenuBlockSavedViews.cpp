// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockSavedViews.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "ModumateCore/ModumateCameraViewStatics.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/RightMenu/ComponentSavedViewListItemObject.h"
#include "Components/ListView.h"
#include "UnrealClasses/SkyActor.h"
#include "Objects/CameraView.h"
#include "DocumentManagement/ModumateDocument.h"


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
	Controller = GetOwningPlayer<AEditModelPlayerController>();
}

void UViewMenuBlockSavedViews::OnButtonAddReleased()
{
	UCameraComponent *cameraComp = Controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
	if (cameraComp)
	{
		FDateTime dateTime = Controller->SkyActor->GetCurrentDateTime();
		int32 listOrder = SavedViewsList->GetNumItems() + 1;
		FString newViewName = FString(TEXT("New Camera View ")) + FString::FromInt(listOrder);
		int32 nextID = Controller->GetDocument()->GetNextAvailableID();
		UModumateCameraViewStatics::CreateCameraViewAsMoi(this, cameraComp, newViewName, dateTime, nextID, listOrder);
	}
}

void UViewMenuBlockSavedViews::RebuildSavedViewsList()
{
	SavedViewsList->ClearListItems();
	auto mois = Controller->GetDocument()->GetObjectsOfType(EObjectType::OTCameraView);

	// Cast and sort camera views according to its list order
	TArray<AMOICameraView*> moiCVs;
	for (const auto& curMoi : mois)
	{
		AMOICameraView* moiCV = Cast<AMOICameraView>(curMoi);
		if (moiCV && !curMoi->IsDestroyed())
		{
			moiCVs.Add(moiCV);
		}
	}

	moiCVs.Sort([](const AMOICameraView& cv1, const AMOICameraView& cv2) 
		{
		return cv2.InstanceData.CameraViewIndex >= cv1.InstanceData.CameraViewIndex;
		});

	for (int32 i = 0; i < moiCVs.Num(); ++i)
	{
		// Change list order from instance data because its order might have changed from sort
		// Currently this isn't part of delta because sorting only applies to client
		// Send this via ApplyDeltas if other clients need to see reordering
		moiCVs[i]->InstanceData.CameraViewIndex = i;

		UComponentSavedViewListItemObject* newListObj = NewObject<UComponentSavedViewListItemObject>(this);
		newListObj->CameraView = moiCVs[i]->InstanceData;
		SavedViewsList->AddItem(newListObj);
	}
}
