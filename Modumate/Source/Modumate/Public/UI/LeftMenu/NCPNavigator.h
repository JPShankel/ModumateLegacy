// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "NCPNavigator.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UNCPNavigator : public UUserWidget
{
	GENERATED_BODY()

public:
	UNCPNavigator(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	UPROPERTY()
	class AEditModelGameState* EMGameState;

	TArray<FBIMTagPath> SelectedTags;
	TArray<FGuid> SearchFilteredPresets;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* SearchBarWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* DynamicMainListView;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> StarterNCPTagStrings;

	void BuildNCPNavigator();
	void BuildBrowserItemSubObjs(const FBIMTagPath& ParentNCP, int32 TagOrder);
	bool IsNCPAvailableForSearch(const FBIMTagPath& NCPTag);
	void CacheSearchFilteredPresets(const TArray<FBIMTagPath>& SourceNCPTags);

	void ToggleNCPTagAsSelected(const FBIMTagPath& NCPTag, bool bAsSelected);
	void RefreshDynamicMainListView();

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);
};