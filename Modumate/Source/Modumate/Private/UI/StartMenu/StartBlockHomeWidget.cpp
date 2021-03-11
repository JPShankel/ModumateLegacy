// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartBlockHomeWidget.h"

#include "Components/WrapBox.h"
#include "Misc/FileHelper.h"
#include "Online/ModumateAccountManager.h"
#include "UI/StartMenu/StartBlockNewProjectCardWidget.h"
#include "UI/StartMenu/StartBlockProjectCardWidget.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UnrealClasses/ModumateGameInstance.h"

UStartBlockHomeWidget::UStartBlockHomeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartBlockHomeWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	MainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode>();

	return true;
}

void UStartBlockHomeWidget::OpenRecentProjectMenu()
{
	if (!(WrapBoxProjects && MainMenuGameMode))
	{
		return;
	}
	WrapBoxProjects->ClearChildren();

	bool bShowNewProjectCard = true;

	// Hide the New Project card if we don't have permission to save multiple projects,
	// and if the local restricted project already exists. If so, it is guaranteed to exist in the recent project list.
	auto gameInstance = MainMenuGameMode->GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	FString restrictedSavePath = FModumateUserSettings::GetRestrictedSavePath();
	if (accountManager.IsValid() && !accountManager->HasPermission(EModumatePermission::ProjectSave) &&
		IFileManager::Get().FileExists(*restrictedSavePath))
	{
		bShowNewProjectCard = false;
	}

	UStartBlockNewProjectCardWidget* newProjectCard = bShowNewProjectCard ? CreateWidget<UStartBlockNewProjectCardWidget>(this, NewProjectCardWidgetClass) : nullptr;
	if (newProjectCard)
	{
		WrapBoxProjects->AddChildToWrapBox(newProjectCard);
	}

	for (int32 i = 0; i < MainMenuGameMode->NumRecentProjects; ++i)
	{
		UStartBlockProjectCardWidget *newLoadProjectCard = CreateWidget<UStartBlockProjectCardWidget>(this, LoadProjectCardWidgetClass);
		if (newLoadProjectCard && newLoadProjectCard->BuildProjectCard(i))
		{
			WrapBoxProjects->AddChildToWrapBox(newLoadProjectCard);
		}
	}
}

void UStartBlockHomeWidget::NativeConstruct()
{
	Super::NativeConstruct();
}