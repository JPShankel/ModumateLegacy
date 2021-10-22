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

	if (DebugSubmit != NULL) 
	{
		DebugSubmit->OnReleased.AddDynamic(this, &UDrawingDesignerWebBrowserWidget::OnDebugSubmit);
	}

	return true;
}

void UDrawingDesignerWebBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UDrawingDesignerWebBrowserWidget::OnDebugSubmit()
{
	if (DebugDocumentTextBox != NULL)
	{
		auto controller = GetOwningPlayer<AEditModelPlayerController>();

		if (controller != NULL)
		{
			FString document = DebugDocumentTextBox->GetText().ToString();
			FString js = TEXT("UE_pushDocument(");
			js = js + document + TEXT(")");
			UE_LOG(LogTemp, Log, TEXT("js=%s"), *js);
			UModumateDocument* doc = controller->GetDocument();
			if (doc)
			{
				FDrawingDesignerDocument replacer;
				replacer.ReadJson(document);
				doc->DrawingDesignerDocument = replacer;
			}
			DrawingSetWebBrowser->ExecuteJavascript(js);
		}
	}
}

void UDrawingDesignerWebBrowserWidget::InitWithController()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();

	if (ensureAlways(controller && controller->GetDocument()))
	{
		static const FString bindObjName(TEXT("doc"));
		DrawingSetWebBrowser->CallBindUObject(bindObjName, controller->GetDocument(), true);
	}

	DrawingSetWebBrowser->LoadURL(CVarModumateDrawingDesignerURL.GetValueOnAnyThread());
}

#undef LOCTEXT_NAMESPACE