// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockDialogBox.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/BIM/BIMDesigner.h"



UBIMBlockDialogBox::UBIMBlockDialogBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockDialogBox::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!(Controller && Button_VariableText_GreyOutline && Button_ActionText_Blue))
	{
		return false;
	}

	Button_VariableText_GreyOutline->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockDialogBox::OnReleaseButton_VariableText_GreyOutline);
	Button_ActionText_Blue->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockDialogBox::OnReleaseButton_ActionText_Blue);
	return true;
}

void UBIMBlockDialogBox::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockDialogBox::OnReleaseButton_VariableText_GreyOutline()
{
	if (Controller && Controller->EditModelUserWidget)
	{
		// ToolMenu requires specific category
		if (Controller->EditModelUserWidget->PreviousLeftMenuState == ELeftMenuState::ToolMenu)
		{
			EToolCategories toolCategory = UModumateTypeStatics::GetToolCategory(Controller->GetToolMode());
			Controller->EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ToolMenu, toolCategory);
		}
		else
		{
			Controller->EditModelUserWidget->SwitchLeftMenu(Controller->EditModelUserWidget->PreviousLeftMenuState);
		}
	}
}

void UBIMBlockDialogBox::OnReleaseButton_ActionText_Blue()
{
	// TODO: toggle all collapse/expand node
	if (Controller && Controller->EditModelUserWidget)
	{
		Controller->EditModelUserWidget->BIMDesigner->ToggleCollapseExpandNodes();
	}
}
