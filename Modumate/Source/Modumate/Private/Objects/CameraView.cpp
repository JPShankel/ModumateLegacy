// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/CameraView.h"
#include "UnrealClasses/SkyActor.h"
#include "UnrealClasses/AxesActor.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/ViewCubeWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/ViewMenuWidget.h"

AMOICameraView::AMOICameraView()
	: AMOIPlaneBase()
{
	FWebMOIProperty prop;

	prop.Name = TEXT("FOV");
	prop.Type = EWebMOIPropertyType::text;
	prop.DisplayName = TEXT("FOV");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("Date");
	prop.Type = EWebMOIPropertyType::cameraDate;
	prop.DisplayName = TEXT("Date");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("Time");
	prop.Type = EWebMOIPropertyType::cameraTime;
	prop.DisplayName = TEXT("Time");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("bViewCubeVisibility");
	prop.Type = EWebMOIPropertyType::boolean;
	prop.DisplayName = TEXT("View Cube");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("bAxesActorVisibility");
	prop.Type = EWebMOIPropertyType::boolean;
	prop.DisplayName = TEXT("World Axes");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("bOrthoView");
	prop.Type = EWebMOIPropertyType::boolean;
	prop.DisplayName = TEXT("Orthogonal");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("bCutPlanesColorVisibility");
	prop.Type = EWebMOIPropertyType::boolean;
	prop.DisplayName = TEXT("Cut plane colors");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("bGraphDirectionVisibility");
	prop.Type = EWebMOIPropertyType::boolean;
	prop.DisplayName = TEXT("Graph Direction");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);
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

void AMOICameraView::UpdateCamera()
{
	UScriptStruct* StructDef = nullptr;
	void* StructPtr=nullptr;
	if (GetInstanceDataStruct(StructDef, StructPtr))
	{
		const UWorld* World = GetWorld();
		const auto Controller = World->GetFirstPlayerController<AEditModelPlayerController>();

		const FDateTime NewDateTime = FDateTime(
			InstanceData.Date.GetYear(), 
			InstanceData.Date.GetMonth(), 
			InstanceData.Date.GetDay(), 
			InstanceData.Time.GetHour(), 
			InstanceData.Time.GetMinute()
		);

		Controller->SetFieldOfViewCommand(InstanceData.FOV);
		Controller->ToggleAllCutPlanesColor(InstanceData.bCutPlanesColorVisibility);
		Controller->EMPlayerPawn->SetCameraOrtho(InstanceData.bOrthoView);
		Controller->SetAlwaysShowGraphDirection(InstanceData.bGraphDirectionVisibility);
		Controller->SkyActor->SetCurrentDateTime(NewDateTime);
		Controller->AxesActor->SetActorHiddenInGame(!InstanceData.bAxesActorVisibility);
		Controller->EditModelUserWidget->ViewCubeUserWidget->SetVisibility(InstanceData.bViewCubeVisibility ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

bool AMOICameraView::FromWebMOI(const FString& InJson)
{
	if (AModumateObjectInstance::FromWebMOI(InJson))
	{
		UpdateCamera();
		return true;
	}
	return false;
}

bool AMOICameraView::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		UWorld* world = GetWorld();
		auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();

		FString currentDateTime = controller->SkyActor->GetCurrentDateTime().ToIso8601();
		FWebMOIProperty* moiProp = OutMOI.Properties.Find(TEXT("Date"));
		moiProp->ValueArray.Empty();
		moiProp->ValueArray.Add(currentDateTime);

		moiProp = OutMOI.Properties.Find(TEXT("Time"));
		moiProp->ValueArray.Empty();
		moiProp->ValueArray.Add(currentDateTime);
		return true;
	}
	return false;
}