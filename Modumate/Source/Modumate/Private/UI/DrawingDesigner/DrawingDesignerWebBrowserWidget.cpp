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
#include "UnrealClasses/EditModelInputHandler.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "ModumateCore/ModumateFunctionLibrary.h"

#define LOCTEXT_NAMESPACE "DrawingDesignerWebBrowserWidget"

TAutoConsoleVariable<FString> CVarModumateDrawingDesignerURL(TEXT("modumate.DrawingDesignerURL"),
#if WITH_EDITOR
	TEXT("http://staging.app.modumate.com"),
#else
	TEXT("http://app.modumate.com"),
#endif
	TEXT("Address of Drawing Designer app"), ECVF_Default);


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
		auto newDoc = FDrawingDesignerDocument();
		
		FDrawingDesignerJsDeltaPackage package;		
		TSharedPtr<FDrawingDesignerDocumentDelta> delta = MakeShared<FDrawingDesignerDocumentDelta>();
		
		delta->from = doc->DrawingDesignerDocument;
		delta->to = newDoc;

		TArray<FDeltaPtr> wrapped;
		wrapped.Add(delta);

		doc->ApplyDeltas(wrapped, doc->GetWorld());
		doc->DrawingPushDD();
	}
}

void UDrawingDesignerWebBrowserWidget::InitWithController()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto gameInstance = controller->GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance->GetCloudConnection();
	
	if (ensureAlways(controller && controller->GetDocument()))
	{
		static const FString bindObjName(TEXT("doc"));
		DrawingSetWebBrowser->CallBindUObject(bindObjName, controller->GetDocument(), true);
		DrawingSetWebBrowser->CallBindUObject(TEXT("ui"), controller->InputHandlerComponent, true);
		DrawingSetWebBrowser->CallBindUObject(TEXT("game"), GetGameInstance(), true);
		
	}

	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	FString currentVersion = projectSettings->ProjectVersion;
	DrawingSetWebBrowser->LoadURL(CVarModumateDrawingDesignerURL.GetValueOnAnyThread() + TEXT("?CLOUD=") + cloudConnection->GetCloudRootURL() + TEXT("&CLIENT_VERSION=") + currentVersion);
}

#undef LOCTEXT_NAMESPACE