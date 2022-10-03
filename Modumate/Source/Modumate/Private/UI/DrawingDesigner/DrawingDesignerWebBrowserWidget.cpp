// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/DrawingDesigner/DrawingDesignerWebBrowserWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/MultiLineEditableTextBox.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "Online/ModumateCloudConnection.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/DocumentWebBridge.h"

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

}

void UDrawingDesignerWebBrowserWidget::InitWithController()
{
	UE_LOG(LogTemp, Log, TEXT("Binding UI..."));
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto gameState = GetWorld() ? Cast<AEditModelGameState>(GetWorld()->GetGameState()) : nullptr;
	auto gameInstance = controller ? controller->GetGameInstance<UModumateGameInstance>() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	
	if (ensureAlways(controller && controller->GetDocument() && gameState && gameState->DocumentWebBridge))
	{
		DrawingSetWebBrowser->CallBindUObject(TEXT("doc"), gameState->DocumentWebBridge);
		DrawingSetWebBrowser->CallBindUObject(TEXT("ui"), controller->InputHandlerComponent);
		DrawingSetWebBrowser->CallBindUObject(TEXT("game"), GetGameInstance());
		UE_LOG(LogTemp, Log, TEXT("...Done"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("...Could not bind UI"));
	}

	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	FString currentVersion = projectSettings->ProjectVersion;
	DrawingSetWebBrowser->LoadURL(CVarModumateDrawingDesignerURL.GetValueOnAnyThread() + TEXT("?CLOUD=") + cloudConnection->GetCloudRootURL() + TEXT("&CLIENT_VERSION=") + currentVersion);
}

#undef LOCTEXT_NAMESPACE