// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "Components/VerticalBox.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/TutorialMenu/TutorialMenuCardWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Sizebox.h"
#include "HttpModule.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/TutorialManager.h"


UTutorialMenuWidget::UTutorialMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonBeginnerWalkthrough && ButtonIntermediateWalkthrough))
	{
		return false;
	}

	ButtonBeginnerWalkthrough->ModumateButton->OnReleased.AddDynamic(this, &UTutorialMenuWidget::OnReleaseButtonBeginnerWalkthrough);
	ButtonIntermediateWalkthrough->ModumateButton->OnReleased.AddDynamic(this, &UTutorialMenuWidget::OnReleaseButtonIntermediateWalkthrough);

	auto world = GetWorld();
	auto gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	TutorialManager = gameInstance ? gameInstance->TutorialManager : nullptr;

	return true;
}

void UTutorialMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TitleHeader->SetVisibility(AsStartMenuTutorial ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	TutorialTitleHeader->SetVisibility(AsStartMenuTutorial ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UTutorialMenuWidget::OnReleaseButtonBeginnerWalkthrough()
{
	if (TutorialManager)
	{
		// TODO: Support from start menu
		TutorialManager->BeginWalkthrough(EModumateWalkthroughCategories::Beginner);
	}
}

void UTutorialMenuWidget::OnReleaseButtonIntermediateWalkthrough()
{
	if (TutorialManager)
	{
		// TODO: Support from start menu
		TutorialManager->BeginWalkthrough(EModumateWalkthroughCategories::Intermediate);
	}
}

void UTutorialMenuWidget::BuildTutorialMenu()
{
	// Only need to request from online source once
	if (CurrentTutorialMenuCardInfo.Num() > 0)
	{
		return;
	}

	TutorialHeaderMessageText->ChangeText(DefaultTutorialHeaderMessage);

	auto httpRequest = FHttpModule::Get().CreateRequest();
	httpRequest->SetVerb("GET");
	httpRequest->SetURL(JsonTutorialLink);
	httpRequest->OnProcessRequestComplete().BindUObject(this, &UTutorialMenuWidget::OnHttpReply);
	httpRequest->ProcessRequest();
}

void UTutorialMenuWidget::OnHttpReply(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		FString fileJsonString = Response->GetContentAsString();

		auto jsonReader = TJsonReaderFactory<>::Create(fileJsonString);

		TSharedPtr<FJsonObject> fileJsonObj;
		bool bLoadJsonSuccess = FJsonSerializer::Deserialize(jsonReader, fileJsonObj) && fileJsonObj.IsValid();
		if (!bLoadJsonSuccess)
		{
			return;
		}

		const TSharedPtr<FJsonObject>* jsonObj;
		if (!fileJsonObj->TryGetObjectField(DocObjectTutorialInfo, jsonObj))
		{
			return;
		}

		FTutorialInfoArrayCollection newTutorialInfoArray;
		if (!FJsonObjectConverter::JsonObjectToUStruct<FTutorialInfoArrayCollection>(jsonObj->ToSharedRef(), &newTutorialInfoArray))
		{
			return;
		}
		TutorialHeaderMessageText->ChangeText(FText::FromString(newTutorialInfoArray.HeaderMessage));
		UpdateTutorialMenu(newTutorialInfoArray);
	}
}

void UTutorialMenuWidget::UpdateTutorialMenu(const FTutorialInfoArrayCollection& InTutorialInfo)
{
	CurrentTutorialMenuCardInfo = InTutorialInfo.ModumateTutorialInfoObjects;
	VerticalBoxTutorialCards->ClearChildren();

	for (int32 i = 0; i < CurrentTutorialMenuCardInfo.Num(); ++i)
	{
		if (CurrentTutorialMenuCardInfo[i].ShowTitleOnly)
		{
			UModumateTextBlockUserWidget* titleCardWidget = CreateWidget<UModumateTextBlockUserWidget>(this, TutorialTitleTextClass);
			if (titleCardWidget)
			{
				titleCardWidget->EllipsizeWordAt = EllipsizeTitleWordAt;
				titleCardWidget->ChangeText(FText::FromString(CurrentTutorialMenuCardInfo[i].Title));
				VerticalBoxTutorialCards->AddChildToVerticalBox(titleCardWidget);
				UVerticalBoxSlot* slot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(titleCardWidget);
				slot->SetPadding(TitleSectionMargin);
			}
		}
		else
		{
			UTutorialMenuCardWidget* tutorialCardWidget = CreateWidget<UTutorialMenuCardWidget>(this, TutorialCardClass);
			if (tutorialCardWidget)
			{
				tutorialCardWidget->BuildTutorialCard(CurrentTutorialMenuCardInfo[i], this);
				VerticalBoxTutorialCards->AddChildToVerticalBox(tutorialCardWidget);
				UVerticalBoxSlot* slot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(tutorialCardWidget);
				slot->SetPadding(CardMargin);
			}
		}
	}
}

bool UTutorialMenuWidget::GetTutorialFilePath(const FString& TutorialFileName, FString& OutFullTutorialFilePath)
{
	FString tutorialsFolderPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("Tutorials");
	FString fullTutorialPath = tutorialsFolderPath / TutorialFileName;
	if (IFileManager::Get().FileExists(*fullTutorialPath))
	{
		OutFullTutorialFilePath = fullTutorialPath;
		return true;
	}
	else
	{
		return false;
	}
}
