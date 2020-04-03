// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IModumateEditorModule.h"
#include "ISourceControlProvider.h"
#include "Containers/Ticker.h"
#include "AssetTypeCategories.h"

class FModumateEditorModule : public IModumateEditorModule
{
public:
	/** Accessor for the module interface */
	static FModumateEditorModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<FModumateEditorModule>(TEXT("ModumateEditor"));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void QueryVersionInfo();
	void UpdateProjectVersion(const FString& branch, uint32 changelist);

	static const FName AssetCategoryName;
	EAssetTypeCategories::Type ModumateAssetCategory = EAssetTypeCategories::FirstUser;

protected:
	void SourceControlOperationComplete(const FSourceControlOperationRef& Operation, ECommandResult::Type InResult);
};
