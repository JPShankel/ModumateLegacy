// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialManager.h"

#include "Database/ModumateObjectEnums.h"
#include "DocumentManagement/ModumateDocument.h"
#include "JsonObjectConverter.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialMenu/TutorialWalkthroughMenu.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"

bool FModumateWalkthroughStepReqs::IsEmpty() const
{
	return
		(ToolModesToActivate.Num() == 0) &&
		(ObjectTypesToCreate.Num() == 0) &&
		(InputBindingsToPerform.Num() == 0) &&
		(InputCommandsToPerform.Num() == 0) &&
		(AnalyticEventsToRecord.Num() == 0) &&
		(CustomActionsToPerform.Num() == 0);

}

void UModumateTutorialManager::Init()
{
	UModumateAnalyticsStatics::OnRecordedAnalyticsEvent.AddDynamic(this, &UModumateTutorialManager::OnRecordedAnalyticsEvent);

	if (DynamicTutorialDataURL.IsEmpty())
	{
		OnLoadDataReply(FHttpRequestPtr(), FHttpResponsePtr(), false);
	}
	else
	{
		auto httpRequest = FHttpModule::Get().CreateRequest();
		httpRequest->SetVerb("GET");
		httpRequest->SetURL(DynamicTutorialDataURL);
		httpRequest->OnProcessRequestComplete().BindUObject(this, &UModumateTutorialManager::OnLoadDataReply);
		httpRequest->ProcessRequest();
	}
}

void UModumateTutorialManager::Shutdown()
{

}

bool UModumateTutorialManager::LoadWalkthroughData(const FString& WalkthroughDataJSON)
{
	bDataLoaded = false;
	WalkthroughStepsByCategory.Reset();

	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<> > jsonReader = TJsonReaderFactory<>::Create(WalkthroughDataJSON);
	if (!FJsonSerializer::Deserialize(jsonReader, jsonObject) || !jsonObject.IsValid())
	{
		return false;
	}

	static UEnum* categoriesEnum = StaticEnum<EModumateWalkthroughCategories>();
	for (int32 categoryIdx = 0; categoryIdx < categoriesEnum->NumEnums(); ++categoryIdx)
	{
		FString categoryName = categoriesEnum->GetNameStringByIndex(categoryIdx);
		const TArray<TSharedPtr<FJsonValue>>* categoryStepsJson = nullptr;
		if (!jsonObject->TryGetArrayField(categoryName, categoryStepsJson))
		{
			continue;
		}

		auto category = static_cast<EModumateWalkthroughCategories>(categoriesEnum->GetValueByIndex(categoryIdx));
		auto& categorySteps = WalkthroughStepsByCategory.Add(category);

		for (auto& categoryStepJsonValue : *categoryStepsJson)
		{
			const TSharedPtr<FJsonObject>* categoryStepJsonObj = nullptr;
			if (!categoryStepJsonValue->TryGetObject(categoryStepJsonObj) ||
				(categoryStepJsonObj == nullptr) || !categoryStepJsonObj->IsValid())
			{
				continue;
			}

			FModumateWalkthroughStepData categoryStep;
			if (FJsonObjectConverter::JsonObjectToUStruct(categoryStepJsonObj->ToSharedRef(), &categoryStep))
			{
				categorySteps.Add(categoryStep);
			}
		}
	}

	bDataLoaded = true;
	return true;
}

bool UModumateTutorialManager::BeginWalkthrough(EModumateWalkthroughCategories WalkthroughCategory)
{
	if (CurWalkthroughCategory != EModumateWalkthroughCategories::None)
	{
		return false;
	}

	if (!CacheObjects())
	{
		return false;
	}

	CurWalkthroughCategory = WalkthroughCategory;
	CurWalkthroughStartTime = FDateTime::Now();
	UModumateAnalyticsStatics::SetInTutorial(true);
	SetWalkthroughStepIndex(0, false);

	static const FString eventNamePrefix(TEXT("BeginWalkthrough_"));
	FString eventName = eventNamePrefix + GetEnumValueString(CurWalkthroughCategory);
	UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Tutorials, eventName);

	return true;
}

bool UModumateTutorialManager::AdvanceWalkthrough(bool bSkipped)
{
	if (CurWalkthroughCategory == EModumateWalkthroughCategories::None)
	{
		return false;
	}

	auto& curSteps = GetCurWalkthroughSteps();
	if (!curSteps.IsValidIndex(CurWalkthroughStepIdx))
	{
		return false;
	}

	OnWalkthroughStepCompleted.Broadcast(CurWalkthroughCategory, CurWalkthroughStepIdx);

	// Record an event for skipping/completing the event, and how long it took
	FDateTime curTime = FDateTime::Now();
	FTimespan stepTime = curTime - CurWalkthroughStepStartTime;

	static const TCHAR* skippedStepFormat = TEXT("SkippedWalkthroughStep_{0}_{1}");
	static const TCHAR* completedStepFormat = TEXT("CompletedWalkthroughStep_{0}_{1}");
	const TCHAR* eventFormat = bSkipped ? skippedStepFormat : completedStepFormat;

	FString eventName = FString::Format(eventFormat, { GetEnumValueString(CurWalkthroughCategory), CurWalkthroughStepIdx });
	UModumateAnalyticsStatics::RecordEventCustomFloat(this, EModumateAnalyticsCategory::Tutorials, eventName, stepTime.GetTotalSeconds());

	SetWalkthroughStepIndex(CurWalkthroughStepIdx + 1, bSkipped);

	return true;
}

bool UModumateTutorialManager::RewindWalkthrough()
{
	if (CurWalkthroughCategory == EModumateWalkthroughCategories::None)
	{
		return false;
	}

	auto& curSteps = GetCurWalkthroughSteps();
	if (!curSteps.IsValidIndex(CurWalkthroughStepIdx))
	{
		return false;
	}

	SetWalkthroughStepIndex(CurWalkthroughStepIdx - 1, true);
	return true;
}

bool UModumateTutorialManager::EndWalkthrough(bool bSkipped)
{
	if (CurWalkthroughCategory == EModumateWalkthroughCategories::None)
	{
		return false;
	}

	// Record an event for skipping/completing the walkthrough, and how long it took
	FTimespan walkthroughTime = FDateTime::Now() - CurWalkthroughStartTime;

	static const FString skippedWalkthroughPrefix(TEXT("SkippedWalkthrough_"));
	static const FString completedWalkthroughPrefix(TEXT("CompletedWalkthrough_"));
	const FString& eventPrefix = bSkipped ? skippedWalkthroughPrefix : completedWalkthroughPrefix;
	FString eventName = eventPrefix + GetEnumValueString(CurWalkthroughCategory);
	UModumateAnalyticsStatics::RecordEventCustomFloat(this, EModumateAnalyticsCategory::Tutorials, eventName, walkthroughTime.GetTotalSeconds());

	ResetWalkthroughState();
	UModumateAnalyticsStatics::SetInTutorial(false);

	if (CacheObjects())
	{
		WalkthroughMenu->UpdateBlockVisibility(ETutorialWalkthroughBlockStage::None);
	}

	return true;
}

bool UModumateTutorialManager::OpenVideoTutorial(const FString& ProjectFilePath, const FString& VideoURL)
{
	if (ProjectFilePath.IsEmpty() && VideoURL.IsEmpty())
	{
		return false;
	}

	if (!ProjectFilePath.IsEmpty())
	{
		auto world = GetOuter()->GetWorld();
		// Check if this is in edit scene or main menu
		AEditModelPlayerController* controller = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
		AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
		if (controller)
		{
			if (!controller->LoadModelFilePath(ProjectFilePath, false, false, false))
			{
				return false;
			}
		}
		else if (mainMenuGameMode)
		{
			if (!mainMenuGameMode->OpenProject(ProjectFilePath, true))
			{
				return false;
			}
		}
	}

	if (!VideoURL.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*VideoURL, nullptr, nullptr);

		int32 urlLastSlashIdx;
		if (VideoURL.FindLastChar(TEXT('/'), urlLastSlashIdx))
		{
			static const FString eventNamePrefix(TEXT("OpenedVideoTutorial_"));
			FString videoID = VideoURL.RightChop(urlLastSlashIdx + 1);
			FString eventName = eventNamePrefix + videoID;
			UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Tutorials, eventName);
		}
	}

	return true;
}

bool UModumateTutorialManager::RecordWalkthroughCustomAction(EModumateWalkthroughCustomActions CustomAction)
{
	bool bMadeProgress = (CurWalkthroughStepReqsRemaining.CustomActionsToPerform.Remove(CustomAction) > 0);
	if (bMadeProgress)
	{
		CheckCurrentStepRequirements();
	}

	return bMadeProgress;
}

const TArray<FModumateWalkthroughStepData>& UModumateTutorialManager::GetCurWalkthroughSteps() const
{
	static TArray<FModumateWalkthroughStepData> emptySteps;

	return WalkthroughStepsByCategory.Contains(CurWalkthroughCategory) ? WalkthroughStepsByCategory[CurWalkthroughCategory] : emptySteps;
}

const FModumateWalkthroughStepData& UModumateTutorialManager::GetCurWalkthroughStepData() const
{
	static FModumateWalkthroughStepData invalidStepData;

	auto& curSteps = GetCurWalkthroughSteps();
	return curSteps.IsValidIndex(CurWalkthroughStepIdx) ? curSteps[CurWalkthroughStepIdx] : invalidStepData;
}

void UModumateTutorialManager::OpenWalkthroughProject(EModumateWalkthroughCategories WalkthroughCategory)
{
	if (CurWalkthroughCategory != EModumateWalkthroughCategories::None)
	{
		EndWalkthrough(true);
	}

	// TODO: this should be from record
	static const FString beginnerProjectName(TEXT("Beginner Tutorial Project.mdmt"));
	static const FString IntermediateProjectName(TEXT("IntermediateTutorialProject.mdmt"));

	FString walkthroughFullPath;
	if (WalkthroughCategory == EModumateWalkthroughCategories::Beginner)
	{
		GetTutorialFilePath(beginnerProjectName, walkthroughFullPath);
	}
	else if (WalkthroughCategory == EModumateWalkthroughCategories::Intermediate)
	{
		GetTutorialFilePath(IntermediateProjectName, walkthroughFullPath);
	}

	auto world = GetOuter()->GetWorld();
	// Check if this is in edit scene or main menu
	AEditModelPlayerController* controller = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	if (controller)
	{
		controller->LoadModelFilePath(walkthroughFullPath, false, false, false);
		BeginWalkthrough(WalkthroughCategory);
	}
	else
	{
		AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
		if (mainMenuGameMode)
		{
			FromMainMenuWalkthroughCategory = WalkthroughCategory;
			mainMenuGameMode->OpenProject(walkthroughFullPath, true);
		}
	}
}

bool UModumateTutorialManager::GetTutorialFilePath(const FString& TutorialFileName, FString& OutFullTutorialFilePath)
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

bool UModumateTutorialManager::CheckAbsoluteBeginner()
{
	const auto accountManager = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetAccountManager();
	if (accountManager && accountManager->IsFirstLogin() && !bCompletedFirstTime)
	{
		bCompletedFirstTime = true;
		OpenWalkthroughProject(EModumateWalkthroughCategories::Beginner);
		return true;
	}
	return false;
}

void UModumateTutorialManager::OnLoadDataReply(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		FString responseJsonString = Response->GetContentAsString();
		LoadWalkthroughData(responseJsonString);
	}
	else
	{
		FString backFilePath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("Tutorials") / TEXT("BackupWalkthroughData.json");
		FString backupFileString;
		if (FFileHelper::LoadFileToString(backupFileString, *backFilePath))
		{
			LoadWalkthroughData(backupFileString);
		}
	}
}

void UModumateTutorialManager::SetWalkthroughStepIndex(int32 NewStepIndex, bool bSkipped)
{
	auto& curSteps = GetCurWalkthroughSteps();
	int32 numSteps = curSteps.Num();

	if (CurWalkthroughStepIdx != NewStepIndex)
	{
		CurWalkthroughStepIdx = NewStepIndex;
		CurWalkthroughStepStartTime = FDateTime::Now();

		float progressPCT = (numSteps > 0) ? (float)CurWalkthroughStepIdx / numSteps : 1.0f;

		auto& curStepData = GetCurWalkthroughStepData();
		CurWalkthroughStepReqsRemaining = curStepData.Requirements;

		if (!curSteps.IsValidIndex(CurWalkthroughStepIdx))
		{
			EndWalkthrough(bSkipped);
		}
		else if (CurWalkthroughStepIdx == 0)
		{
			WalkthroughMenu->ShowWalkthroughIntro(curStepData.Title, curStepData.Description);
		}
		else if (CurWalkthroughStepIdx == (numSteps - 1))
		{
			// TODO: allow for advancing to "expert", not just from "beginner"
			bool bShowProceedButton = (CurWalkthroughCategory == EModumateWalkthroughCategories::Beginner);
			WalkthroughMenu->ShowWalkthroughOutro(curStepData.Title, curStepData.Description, bShowProceedButton);
		}
		else
		{
			WalkthroughMenu->ShowWalkthroughStep(curStepData.Title, curStepData.Description, curStepData.VideoURL, progressPCT);
		}
	}
}

bool UModumateTutorialManager::CacheObjects()
{
	auto world = GetOuter()->GetWorld();
	if (world == nullptr)
	{
		return false;
	}

	if (Controller == nullptr)
	{
		Controller = world->GetFirstPlayerController<AEditModelPlayerController>();
		if (Controller == nullptr)
		{
			return false;
		}

		Controller->OnToolModeChanged.AddDynamic(this, &UModumateTutorialManager::OnToolModeChanged);
		Controller->HandledInputActionEvent.AddDynamic(this, &UModumateTutorialManager::OnPlayerInputAction);
		Controller->HandledInputAxisEvent.AddDynamic(this, &UModumateTutorialManager::OnPlayerInputAxis);
		Controller->InputHandlerComponent->OnExecutedCommand.AddDynamic(this, &UModumateTutorialManager::OnExecutedInputCommand);
		Controller->OnDestroyed.AddDynamic(this, &UModumateTutorialManager::OnControllerDestroyed);

		if (auto doc = Controller->GetDocument())
		{
			doc->OnAppliedMOIDeltas.AddDynamic(this, &UModumateTutorialManager::OnAppliedMOIDeltas);
		}
	}

	if (WalkthroughMenu == nullptr)
	{
		WalkthroughMenu = Controller->EditModelUserWidget->TutorialWalkthroughMenuBP;
	}

	return Controller && WalkthroughMenu;
}

void UModumateTutorialManager::CheckCurrentStepRequirements()
{
	auto& curStepData = GetCurWalkthroughStepData();
	if (!curStepData.Requirements.IsEmpty() && CurWalkthroughStepReqsRemaining.IsEmpty())
	{
		WalkthroughMenu->ShowCountdown(curStepData.AutoProceedCountdown);
	}
}

void UModumateTutorialManager::ResetWalkthroughState()
{
	CurWalkthroughCategory = EModumateWalkthroughCategories::None;
	CurWalkthroughStepIdx = INDEX_NONE;
	CurWalkthroughStepReqsRemaining = FModumateWalkthroughStepReqs();
	CurWalkthroughStartTime = FDateTime::MinValue();
	CurWalkthroughStepStartTime = FDateTime::MinValue();
}

void UModumateTutorialManager::OnToolModeChanged()
{
	if (Controller)
	{
		bool bMadeProgress = (CurWalkthroughStepReqsRemaining.ToolModesToActivate.Remove(Controller->GetToolMode()) > 0);
		if (bMadeProgress)
		{
			CheckCurrentStepRequirements();
		}
	}
}

void UModumateTutorialManager::OnAppliedMOIDeltas(EObjectType ObjectType, int32 Count, EMOIDeltaType DeltaType)
{
	if ((Count == 0) || CurWalkthroughStepReqsRemaining.IsEmpty())
	{
		return;
	}

	switch (DeltaType)
	{
	case EMOIDeltaType::Create:
	{
		bool bMadeProgress = (CurWalkthroughStepReqsRemaining.ObjectTypesToCreate.Remove(ObjectType) > 0);
		if (bMadeProgress)
		{
			CheckCurrentStepRequirements();
		}
	}
	break;
	default:
		break;
	}
}

void UModumateTutorialManager::OnPlayerInputAction(FName ActionName, EInputEvent InputEvent)
{
	if ((InputEvent != IE_Pressed) && (InputEvent != IE_Released))
	{
		return;
	}

	FName bindingName(*FString::Printf(TEXT("%s%s"), *ActionName.ToString(),
		(InputEvent == IE_Pressed) ? TEXT("+") : TEXT("-")));
	bool bMadeProgress = (CurWalkthroughStepReqsRemaining.InputBindingsToPerform.Remove(bindingName) > 0);
	if (bMadeProgress)
	{
		CheckCurrentStepRequirements();
	}
}

void UModumateTutorialManager::OnPlayerInputAxis(FName AxisName, float Value)
{
	if (Value == 0.0f)
	{
		return;
	}

	FName bindingName(*FString::Printf(TEXT("%s%s"), *AxisName.ToString(),
		(Value > 0) ? TEXT("+") : TEXT("-")));
	bool bMadeProgress = (CurWalkthroughStepReqsRemaining.InputBindingsToPerform.Remove(bindingName) > 0);
	if (bMadeProgress)
	{
		CheckCurrentStepRequirements();
	}
}

void UModumateTutorialManager::OnExecutedInputCommand(EInputCommand InputCommand)
{
	if (Controller)
	{
		bool bMadeProgress = (CurWalkthroughStepReqsRemaining.InputCommandsToPerform.Remove(InputCommand) > 0);
		if (bMadeProgress)
		{
			CheckCurrentStepRequirements();
		}
	}
}

void UModumateTutorialManager::OnRecordedAnalyticsEvent(EModumateAnalyticsCategory EventCategory, const FString& EventName)
{
	FName fullEventName(*(GetEnumValueString(EventCategory) / EventName));

	bool bMadeProgress = (CurWalkthroughStepReqsRemaining.AnalyticEventsToRecord.Remove(fullEventName) > 0);
	if (bMadeProgress)
	{
		CheckCurrentStepRequirements();
	}
}

void UModumateTutorialManager::OnControllerDestroyed(AActor* PlayerController)
{
	Controller = nullptr;
	WalkthroughMenu = nullptr;
}
