// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/CameraView.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateBrowserStatics.h"
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

	prop.Name = TEXT("SavedTime");
	prop.Type = EWebMOIPropertyType::cameraTime;
	prop.DisplayName = TEXT("Date");
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

	prop.Name = TEXT("UpdateCameraPosition");
	prop.Type = EWebMOIPropertyType::cameraPositionUpdate;
	prop.DisplayName = TEXT("Camera Position");
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
		FDateTime dateTime;
		FDateTime::ParseIso8601(*InstanceData.SavedTime, dateTime);

		auto* controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
		controller->SetFieldOfViewCommand(InstanceData.FOV);
		controller->ToggleAllCutPlanesColor(InstanceData.bCutPlanesColorVisibility);
		controller->EMPlayerPawn->SetCameraOrtho(InstanceData.bOrthoView);
		controller->SetAlwaysShowGraphDirection(InstanceData.bGraphDirectionVisibility);
		controller->SkyActor->SetCurrentDateTime(dateTime);
		controller->AxesActor->SetActorHiddenInGame(!InstanceData.bAxesActorVisibility);
		controller->EditModelUserWidget->ViewCubeUserWidget->SetVisibility(InstanceData.bViewCubeVisibility ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		controller->EMPlayerPawn->SetActorLocationAndRotation(InstanceData.Position, InstanceData.Rotation);
		controller->GetDocument()->OnCameraViewSelected(ID);
	}
}

bool AMOICameraView::FromWebMOI(const FString& InJson)
{
	if (AModumateObjectInstance::FromWebMOI(InJson))
	{
		FWebMOI WebMOI;
		if (!ReadJsonGeneric<FWebMOI>(InJson, &WebMOI))
		{
			return false;
		}

		const FWebMOIProperty* FormPropUpdatePosition = WebMOI.Properties.Find(TEXT("UpdateCameraPosition"));
		const bool bUpdatePosition = FormPropUpdatePosition->ValueArray[0] == TEXT("true");

		if (bUpdatePosition)
		{
			const auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
			auto* cameraComp = controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
			if (cameraComp)
			{
				const FDateTime dateTime = controller->SkyActor->GetCurrentDateTime();
				UModumateBrowserStatics::UpdateCameraViewAsMoi(this, cameraComp, ID, dateTime);
			}
		}
		else
		{
			UpdateCamera();
		}
		
		return true;
	}
	return false;
}

bool AMOICameraView::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		const FWebMOIProperty* formPropUpdatePosition = WebProperties.Find(TEXT("UpdateCameraPosition"));
		FWebMOIProperty webPropUpdatePosition = *formPropUpdatePosition;
		webPropUpdatePosition.ValueArray.Empty();
		webPropUpdatePosition.ValueArray.Add(TEXT("false"));
		OutMOI.Properties.Add(TEXT("UpdateCameraPosition"), webPropUpdatePosition);
		
		return true;
	}
	return false;
}