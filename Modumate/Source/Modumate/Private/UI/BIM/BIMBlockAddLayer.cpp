// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockAddLayer.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"

UBIMBlockAddLayer::UBIMBlockAddLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockAddLayer::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!Button_AddLayer)
	{
		return false;
	}

	Button_AddLayer->OnReleased.AddDynamic(this, &UBIMBlockAddLayer::OnButtonAddReleased);

	return true;
}

void UBIMBlockAddLayer::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

void UBIMBlockAddLayer::OnButtonAddReleased()
{
	if (Controller)
	{
		Controller->EditModelUserWidget->BIMDesigner->AddNodeFromPreset(ParentID, PresetID, ParentSetIndex, ParentSetPosition);
	}
}
