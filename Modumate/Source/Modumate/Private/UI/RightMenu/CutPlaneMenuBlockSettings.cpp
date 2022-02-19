// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuBlockSettings.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/Custom/ModumateCheckBox.h"




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

	if (!(ButtonShowHideAll && ButtonExportAll && ToggleCutPlaneColor))
	{
		return false;
	}

	ButtonShowHideAll->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonShowHideAllReleased);
	ButtonExportAll->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonExportAllReleased);
	ToggleCutPlaneColor->OnCheckStateChanged.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnToggleCutPlaneColorChanged);

	return true;
}

void UCutPlaneMenuBlockSettings::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController>();
	GameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
}

void UCutPlaneMenuBlockSettings::OnButtonShowHideAllReleased()
{
	TArray<AModumateObjectInstance*> cutPlaneMois = GameState->Document->GetObjectsOfType(EObjectType::OTCutPlane);
	bool hideAll = false;
	TArray<int32> allCutPlaneIDs;
	for (int32 i = 0; i < cutPlaneMois.Num(); ++i)
	{
		allCutPlaneIDs.Add(cutPlaneMois[i]->ID);
		if (cutPlaneMois[i]->IsVisible())
		{
			hideAll = true;
		}
	}

	auto* playerState = Controller->GetPlayerState<AEditModelPlayerState>();

	if (hideAll)
	{
		playerState->AddHideObjectsById(allCutPlaneIDs);
	}
	else
	{
		playerState->UnhideObjectsById(allCutPlaneIDs);
	}
}

void UCutPlaneMenuBlockSettings::OnButtonExportAllReleased()
{
	Controller->EditModelUserWidget->CutPlaneMenu->SetCutPlaneExportMenuVisibility(true);
}

void UCutPlaneMenuBlockSettings::OnToggleCutPlaneColorChanged(bool bIsChecked)
{
	Controller->ToggleAllCutPlanesColor(bIsChecked);
}
