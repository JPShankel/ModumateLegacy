// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserBlockExport.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"


UBrowserBlockExport::UBrowserBlockExport(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBrowserBlockExport::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonExport && ButtonCancel))
	{
		return false;
	}

	ButtonExport->ModumateButton->OnReleased.AddDynamic(this, &UBrowserBlockExport::OnButtonExportReleased);
	ButtonCancel->ModumateButton->OnReleased.AddDynamic(this, &UBrowserBlockExport::OnButtonCancelReleased);

	return true;
}

void UBrowserBlockExport::NativeConstruct()
{
	Super::NativeConstruct();

	//ExportRemainText->ChangeText(FText::FromString("fdgajld"));
}

void UBrowserBlockExport::OnButtonExportReleased()
{
	if (ensureAlways(ParentBrowserMenuWidget))
	{
		ParentBrowserMenuWidget->AskToExportEstimates();
	}
}

void UBrowserBlockExport::OnButtonCancelReleased()
{
	if (ensureAlways(ParentBrowserMenuWidget))
	{
		ParentBrowserMenuWidget->ToggleExportWidgetMenu(false);
	}
}
