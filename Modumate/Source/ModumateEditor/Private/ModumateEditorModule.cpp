// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateEditorModule.h"

#include "AssetToolsModule.h"
#include "Engine/Engine.h"
#include "IAssetTools.h"
#include "ISourceControlModule.h"
#include "Logging/LogMacros.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "UnrealClasses/Modumate.h"
#include "ModumateAssetFactories.h"
#include "ModumateCore/ModumateFunctionLibrary.h"


IMPLEMENT_MODULE(FModumateEditorModule, ModumateEditor)

DEFINE_LOG_CATEGORY_STATIC(LogModumateEditor, Log, All)
#define LOCTEXT_NAMESPACE "ModumateEditor"

const FName FModumateEditorModule::AssetCategoryName(TEXT("Modumate"));

void FModumateEditorModule::StartupModule()
{
	if (GEngine && GEngine->IsValidLowLevel())
	{
		QueryVersionInfo();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda([] {
			FModumateEditorModule::Get().QueryVersionInfo();
		});
	}

	FText assetCategoryText = LOCTEXT("ModumateAssetCategory", "Modumate");
	IAssetTools& assetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	ModumateAssetCategory = assetTools.RegisterAdvancedAssetCategory(AssetCategoryName, assetCategoryText);
	assetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SimpleMeshData));
}

void FModumateEditorModule::ShutdownModule()
{
}

void FModumateEditorModule::QueryVersionInfo()
{
	auto &scProvider = ISourceControlModule::Get().GetProvider();
	if (scProvider.IsEnabled())
	{
		scProvider.Login(FString(), EConcurrency::Asynchronous,
			FSourceControlOperationComplete::CreateRaw(this, &FModumateEditorModule::SourceControlOperationComplete));
	}
}

void FModumateEditorModule::UpdateProjectVersion(const FString& branch, uint32 changelist)
{
	const auto &curVersion = FEngineVersion::Current();
	auto updatedVersion = curVersion;
	updatedVersion.Set(curVersion.GetMajor(), curVersion.GetMinor(), curVersion.GetPatch(), changelist, branch);

	FModumateModule& modumateModule = FModumateModule::Get();
	modumateModule.UpdateProjectDisplayVersion(updatedVersion);

	// Save the custom version string so that we can load it in non-packaged builds when the editor isn't available.
	FFileHelper::SaveStringToFile(updatedVersion.ToString(), *FModumateModule::OverrideVersionPath);
}

void FModumateEditorModule::SourceControlOperationComplete(const FSourceControlOperationRef& Operation, ECommandResult::Type InResult)
{
	auto &resultInfo = Operation->GetResultInfo();

	static const FString branchKey(TEXT("Stream "));
	static const FString changelistKey(TEXT("change "));
	FString currentBranch;
	uint32 currentChangelist = 0;

	for (auto &resultLine : resultInfo.InfoMessages)
	{
		FString resultString = resultLine.ToString();
		if (resultString.RemoveFromStart(branchKey, ESearchCase::CaseSensitive))
		{
			currentBranch = resultString.Replace(TEXT("/"), TEXT("+"));
		}
		else if (resultString.RemoveFromStart(changelistKey, ESearchCase::CaseSensitive))
		{
			currentChangelist = (uint32)FCString::Atoi(*resultString);
		}
	}

	if (!currentBranch.IsEmpty() && (currentChangelist > 0))
	{
		UpdateProjectVersion(currentBranch, currentChangelist);
	}
}

#undef LOCTEXT_NAMESPACE
