// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialManager.h"

#include "Database/ModumateObjectEnums.h"
#include "Engine/GameViewportClient.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialMenu/TutorialWalkthroughMenu.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"

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
	// TODO: load walkthrough data from our backend
	FString backFilePath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("Tutorials") / TEXT("BackupWalkthroughData.json");
	FString backupFileString;
	if (FFileHelper::LoadFileToString(backupFileString, *backFilePath))
	{
		LoadWalkthroughData(backupFileString);
	}
}

void UModumateTutorialManager::Shutdown()
{

}

bool UModumateTutorialManager::LoadWalkthroughData(const FString& WalkthroughDataJSON)
{
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
	SetWalkthroughStepIndex(0);

	return true;
}

bool UModumateTutorialManager::AdvanceWalkthrough()
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
	SetWalkthroughStepIndex(CurWalkthroughStepIdx + 1);

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

	SetWalkthroughStepIndex(CurWalkthroughStepIdx - 1);
	return true;
}

bool UModumateTutorialManager::EndWalkthrough()
{
	if (CurWalkthroughCategory == EModumateWalkthroughCategories::None)
	{
		return false;
	}

	SetWalkthroughStepIndex(INDEX_NONE);
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

void UModumateTutorialManager::SetWalkthroughStepIndex(int32 NewStepIndex)
{
	auto& curSteps = GetCurWalkthroughSteps();
	int32 numSteps = curSteps.Num();

	if (CurWalkthroughStepIdx != NewStepIndex)
	{
		CurWalkthroughStepIdx = NewStepIndex;

		float progressPCT = (numSteps > 0) ? (float)CurWalkthroughStepIdx / numSteps : 1.0f;

		auto& curStepData = GetCurWalkthroughStepData();
		CurWalkthroughStepReqsRemaining = curStepData.Requirements;

		if (CurWalkthroughStepIdx == INDEX_NONE)
		{
			CurWalkthroughCategory = EModumateWalkthroughCategories::None;
			WalkthroughMenu->UpdateBlockVisibility(ETutorialWalkthroughBlockStage::None);
		}
		else if (CurWalkthroughStepIdx == 0)
		{
			WalkthroughMenu->ShowWalkthroughIntro(curStepData.Title, curStepData.Description);
		}
		else if (CurWalkthroughStepIdx == (numSteps - 1))
		{
			WalkthroughMenu->ShowWalkthroughOutro(curStepData.Title, curStepData.Description);
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

	if (Controller == nullptr)
	{
		Controller = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
		if (Controller == nullptr)
		{
			return false;
		}

		Controller->OnToolModeChanged.AddDynamic(this, &UModumateTutorialManager::OnToolModeChanged);
		Controller->HandledInputActionEvent.AddDynamic(this, &UModumateTutorialManager::OnPlayerInputAction);
		Controller->HandledInputAxisEvent.AddDynamic(this, &UModumateTutorialManager::OnPlayerInputAxis);
		Controller->InputHandlerComponent->OnExecutedCommand.AddDynamic(this, &UModumateTutorialManager::OnExecutedInputCommand);
	}

	auto gameViewport = world ? world->GetGameViewport() : nullptr;
	if (gameViewport)
	{
		gameViewport->OnToggleFullscreen().AddUObject(this, &UModumateTutorialManager::OnToggleFullscreen);
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

void UModumateTutorialManager::OnToggleFullscreen(bool bIsFullscreen)
{
	bool bMadeProgress = (CurWalkthroughStepReqsRemaining.CustomActionsToPerform.Remove(EModumateWalkthroughCustomActions::ToggleFullscreen) > 0);
	if (bMadeProgress)
	{
		CheckCurrentStepRequirements();
	}
}
