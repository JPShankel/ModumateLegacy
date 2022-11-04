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

	prop.name = TEXT("fOV");
	prop.type = EWebMOIPropertyType::text;
	prop.displayName = TEXT("FOV");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("savedTime");
	prop.type = EWebMOIPropertyType::cameraTime;
	prop.displayName = TEXT("Date");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bViewCubeVisibility");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("View Cube");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bAxesActorVisibility");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("World Axes");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bOrthoView");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("Orthogonal");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bCutPlanesColorVisibility");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("Cut plane colors");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bGraphDirectionVisibility");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("Graph Direction");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("updateCameraPosition");
	prop.type = EWebMOIPropertyType::cameraPositionUpdate;
	prop.displayName = TEXT("Camera Position");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bRTEnabled");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("Ray Tracing Enabled");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("rayTracingExposure");
	prop.type = EWebMOIPropertyType::number;
	prop.displayName = TEXT("Ray Tracing Exposure");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("rayTracingQuality");
	prop.type = EWebMOIPropertyType::number;
	prop.displayName = TEXT("Ray Tracing Quality");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("bShowLights");
	prop.type = EWebMOIPropertyType::number;
	prop.displayName = TEXT("Show Lights");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);
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

void AMOICameraView::UpdateCameraPosition()
{
	UScriptStruct* StructDef = nullptr;
	void* StructPtr=nullptr;
	if (GetInstanceDataStruct(StructDef, StructPtr))
	{
		auto* controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
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

		const FWebMOIProperty* FormPropUpdatePosition = WebMOI.properties.Find(TEXT("updateCameraPosition"));
		const bool bUpdatePosition = FormPropUpdatePosition->valueArray[0] == TEXT("true");

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
		
		return true;
	}
	return false;
}

bool AMOICameraView::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		const FWebMOIProperty* formPropUpdatePosition = WebProperties.Find(TEXT("updateCameraPosition"));
		FWebMOIProperty webPropUpdatePosition = *formPropUpdatePosition;
		webPropUpdatePosition.valueArray.Empty();
		webPropUpdatePosition.valueArray.Add(TEXT("false"));
		OutMOI.properties.Add(TEXT("updateCameraPosition"), webPropUpdatePosition);
		
		return true;
	}
	return false;
}