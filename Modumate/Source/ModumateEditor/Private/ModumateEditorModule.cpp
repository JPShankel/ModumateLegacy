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
	assetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_SimpleMeshData>());
}

void FModumateEditorModule::ShutdownModule()
{
}

void FModumateEditorModule::QueryVersionInfo()
{
	auto &scProvider = ISourceControlModule::Get().GetProvider();
	if (scProvider.IsEnabled())
	{
		FString branchString;
		FText sourceControlStatus = scProvider.GetStatusText();
		TArray<FString> statusLines;
		sourceControlStatus.ToString().ParseIntoArrayLines(statusLines);
		for (const FString &statusLine : statusLines)
		{
			static const FString branchPrefix(TEXT("Branch: "));
			if (statusLine.StartsWith(branchPrefix))
			{
				branchString = statusLine;
				branchString.RemoveFromStart(branchPrefix);
				break;
			}
		}

		if (!branchString.IsEmpty())
		{
			UpdateProjectVersion(branchString);
		}
	}
}

void FModumateEditorModule::UpdateProjectVersion(const FString& Branch)
{
	const auto &curVersion = FEngineVersion::Current();
	auto updatedVersion = curVersion;
	updatedVersion.Set(curVersion.GetMajor(), curVersion.GetMinor(), curVersion.GetPatch(), curVersion.GetChangelist(), Branch);

	FModumateModule& modumateModule = FModumateModule::Get();
	modumateModule.UpdateProjectDisplayVersion(updatedVersion);

	// Save the custom version string so that we can load it in non-packaged builds when the editor isn't available.
	FFileHelper::SaveStringToFile(updatedVersion.ToString(), *FModumateModule::OverrideVersionPath);
}

#undef LOCTEXT_NAMESPACE
