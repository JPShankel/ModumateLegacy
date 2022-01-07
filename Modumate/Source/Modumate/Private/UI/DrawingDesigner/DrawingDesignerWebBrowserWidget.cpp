// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/DrawingDesigner/DrawingDesignerWebBrowserWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/MultiLineEditableTextBox.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "Online/ModumateCloudConnection.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"

#define LOCTEXT_NAMESPACE "DrawingDesignerWebBrowserWidget"

TAutoConsoleVariable<FString> CVarModumateDrawingDesignerURL(TEXT("modumate.DrawingDesignerURL"),
	TEXT("http://drawingdesigner.modumate.com"), TEXT("Address of Drawing Designer app"), ECVF_Default);


UDrawingDesignerWebBrowserWidget::UDrawingDesignerWebBrowserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDrawingDesignerWebBrowserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (ResetDocumentButton != nullptr)
	{
		ResetDocumentButton->OnReleased.AddDynamic(this, &UDrawingDesignerWebBrowserWidget::ResetDocumentButtonPressed);
	}

	return true;
}

void UDrawingDesignerWebBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UDrawingDesignerWebBrowserWidget::ResetDocumentButtonPressed()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();

	if (ensure(controller))
	{
		UModumateDocument* doc = controller->GetDocument();
		doc->DrawingDesignerDocument = FDrawingDesignerDocument();
		doc->drawing_request_document();
	}
}

void UDrawingDesignerWebBrowserWidget::InitWithController()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();

	if (ensureAlways(controller && controller->GetDocument()))
	{
		static const FString bindObjName(TEXT("doc"));
		DrawingSetWebBrowser->CallBindUObject(bindObjName, controller->GetDocument(), true);

		DrawingSetWebBrowser->CallBindUObject(TEXT("ui"), Cast<UObject>(controller->InputHandlerComponent), true);
	}

	DrawingSetWebBrowser->LoadURL(CVarModumateDrawingDesignerURL.GetValueOnAnyThread());
}

#undef LOCTEXT_NAMESPACE