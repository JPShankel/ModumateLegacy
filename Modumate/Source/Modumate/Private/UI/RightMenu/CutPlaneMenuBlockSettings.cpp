// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuBlockSettings.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"


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

	if (!(ButtonShowHideAll && ButtonExportAll))
	{
		return false;
	}

	ButtonShowHideAll->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonShowHideAllReleased);
	ButtonExportAll->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockSettings::OnButtonExportAllReleased);

	return true;
}

void UCutPlaneMenuBlockSettings::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	GameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
}

void UCutPlaneMenuBlockSettings::OnButtonShowHideAllReleased()
{
	TArray<FModumateObjectInstance*> cutPlaneMois = GameState->Document.GetObjectsOfType(EObjectType::OTCutPlane);
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
	if (hideAll)
	{
		GameState->Document.AddHideObjectsById(GetWorld(), allCutPlaneIDs);
	}
	else
	{
		GameState->Document.UnhideObjectsById(GetWorld(), allCutPlaneIDs);
	}
}

void UCutPlaneMenuBlockSettings::OnButtonExportAllReleased()
{
	Controller->EditModelUserWidget->CutPlaneMenu->SetCutPlaneExportMenuVisibility(true);
}
