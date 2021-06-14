// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "UI/PresetCard/PresetCardMain.h"
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
	TArray<FGuid> IgnoredPresets;

	EPresetCardType CurrentPresetCardType = EPresetCardType::None;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* SearchBarWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* DynamicMainListView;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder* BorderBackground;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* VerticalBox_TitleAndSearchbar;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bBorderAutoSize = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FAnchors EnableAutoSizeAnchor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FAnchors DisableAutoSizeAnchor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FMargin EnableAutoSizeMargin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FMargin DisableAutoSizeMargin;

	// String of the NCP, and whether it should be opened by default
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FString, bool> StarterNCPTagStrings;

	// String of the NCP tutorial
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FString, bool> TutorialNCPTagStrings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUsesHelpMenuSearchbar = false;

	void BuildAssemblyList(bool bScrollToSelectedAssembly = false);
	void BuildNCPNavigator(EPresetCardType BuildAsType);
	void BuildBrowserItemSubObjs(const FBIMTagPath& ParentNCP, int32 TagOrder);
	bool IsNCPAvailableForSearch(const FBIMTagPath& NCPTag);
	void CacheSearchFilteredPresets(const TArray<FBIMTagPath>& SourceNCPTags);

	void ToggleNCPTagAsSelected(const FBIMTagPath& NCPTag, bool bAsSelected);
	void SetNCPTagPathAsSelected(const FBIMTagPath& NCPTag);
	void RefreshDynamicMainListView();
	void ResetSelectedAndSearchTag();
	void ScrollPresetToView(const FGuid PresetToView);

	void AddToIgnoredPresets(const TArray<FGuid>& InPresets);
	void EmptyIgnoredPresets();

	void GetTopTraversalPath(const FBIMTagPath& InNCP, FBIMTagPath& TopTraversalNCP);
	EPresetCardType GetCurrentType() { return CurrentPresetCardType; }

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);

	UModumateEditableTextBoxUserWidget* GetSearchTextBox();

	// Tutorial
	void BuildTutorialList();
	void BuildTutorialBrowserItemSubObjs(const FBIMTagPath& ParentNCP, int32 TagOrder);
};
