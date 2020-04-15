// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersion.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAutoDrafting, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogUnitTest, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCallTrace, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCommand, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogPerformance, Log, All);

class IModumateModule : public IModuleInterface
{
public:
	virtual bool IsGameModule() const override
	{
		return true;
	}
};

class MODUMATE_API FModumateModule : public IModumateModule
{
public:
	/** Accessor for the module interface */
	static FModumateModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<FModumateModule>(TEXT("Modumate"));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	const FString &GetProjectDisplayVersion() { return ProjectDisplayVersion; }
	void UpdateProjectDisplayVersion(const FEngineVersion &engineVersion);

	static const FString OverrideVersionPath;

protected:
	FString ProjectDisplayVersion = FString();
};
