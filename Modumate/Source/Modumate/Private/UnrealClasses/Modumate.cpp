// Copyright 2018 Modumate, Inc. All Rights Reserved.
#include "UnrealClasses/Modumate.h"

#include "GeneralProjectSettings.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FModumateModule, Modumate, "Modumate")

DEFINE_LOG_CATEGORY(LogAutoDrafting);
DEFINE_LOG_CATEGORY(LogUnitTest);
DEFINE_LOG_CATEGORY(LogCallTrace);
DEFINE_LOG_CATEGORY(LogCommand);
DEFINE_LOG_CATEGORY(LogPerformance);

const FString FModumateModule::OverrideVersionPath(TEXT("Modumate.version"));

void FModumateModule::StartupModule()
{
	// In packaged builds, the current engine version has all of the specific data we want.
	FEngineVersion modumateVersion = FEngineVersion::Current();

	// In non-packaged builds, try to load the custom version string if the editor isn't available.
#if !UE_BUILD_SHIPPING
	FString savedVersionString;
	if (FFileHelper::LoadFileToString(savedVersionString, *FModumateModule::OverrideVersionPath) &&
		FEngineVersion::Parse(savedVersionString, modumateVersion))
	{
		UE_LOG(LogTemp, Log, TEXT("Parsed saved project version: %s"), *savedVersionString);
	}
#endif

	UpdateProjectDisplayVersion(modumateVersion);

	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping("/Project", ShaderDirectory);
}

void FModumateModule::ShutdownModule()
{

}

void FModumateModule::UpdateProjectDisplayVersion(const FEngineVersion &engineVersion)
{
	const auto *projectSettings = GetDefault<UGeneralProjectSettings>();
	const FString &projectVersion = projectSettings->ProjectVersion;
	FString fullBranch = engineVersion.GetBranchDescriptor();

	FString branchName, branchCommit, versionSuffix;
	fullBranch.Split(TEXT("+"), &branchName, &branchCommit, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
#if !UE_BUILD_SHIPPING
	if (!branchName.Contains(projectVersion))
	{
		ProjectDisplayVersion = FString::Printf(TEXT("%s %s+%s"), *branchName, *projectVersion, *branchCommit);
	}
	else
#endif
	{
		ProjectDisplayVersion = FString::Printf(TEXT("%s+%s"), *projectVersion, *branchCommit);
	}

	UE_LOG(LogTemp, Log, TEXT("Updated project version: %s"), *ProjectDisplayVersion);
}
