// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuBlockSettings.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateButton.h"


using namespace Modumate;

UCutPlaneMenuBlockSettings::UCutPlaneMenuBlockSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCutPlaneMenuBlockSettings::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonNewCutPlane && ButtonShowHideAll && ButtonExportSelected))
	{
		return false;
	}

	ButtonNewCutPlane->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonNewCutPlaneReleased);
	ButtonShowHideAll->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonShowHideAllReleased);
	ButtonExportSelected->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonExportSelectedReleased);

	return true;
}

void UCutPlaneMenuBlockSettings::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

void UCutPlaneMenuBlockSettings::OnButtonNewCutPlaneReleased()
{
	Controller->SetToolMode(EToolMode::VE_CUTPLANE);
}

void UCutPlaneMenuBlockSettings::OnButtonShowHideAllReleased()
{
	// TODO: Toggle all cut planes visibility (instance properties?)
}

void UCutPlaneMenuBlockSettings::OnButtonExportSelectedReleased()
{
	// TODO: Get selected cut plane, and then export
}
