// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/CameraView.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/ViewMenuWidget.h"

AMOICameraView::AMOICameraView()
	: AMOIPlaneBase()
{
}

AActor* AMOICameraView::CreateActor(const FVector& loc, const FQuat& rot)
{
	AActor* returnActor = AMOIPlaneBase::CreateActor(loc, rot);
	return returnActor;
}

void AMOICameraView::PostCreateObject(bool bNewObject)
{
	Super::PostCreateObject(bNewObject);
	GetActor()->OnDestroyed.AddDynamic(this, &AMOICameraView::OnCameraActorDestroyed);
}

bool AMOICameraView::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	UpdateViewMenu();
	return true;
}

void AMOICameraView::UpdateViewMenu()
{
	UWorld* world = GetWorld();
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		if (controller->EditModelUserWidget->CurrentLeftMenuState == ELeftMenuState::ViewMenu)
		{
			controller->EditModelUserWidget->ViewMenu->BuildViewMenu();
		}
	}
}

void AMOICameraView::OnCameraActorDestroyed(AActor* DestroyedActor)
{
	UpdateViewMenu();
}
