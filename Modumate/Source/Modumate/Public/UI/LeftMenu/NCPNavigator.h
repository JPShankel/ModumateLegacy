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

	EPresetCardType CurrentPresetCardType = EPresetCardType::None;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* SearchBarWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* DynamicMainListView;

	// String of the NCP, and whether it should be opened by default
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FString, bool> StarterNCPTagStrings;

	void BuildNCPNavigator(EPresetCardType BuildAsType);
	void BuildBrowserItemSubObjs(const FBIMTagPath& ParentNCP, int32 TagOrder);
	bool IsNCPAvailableForSearch(const FBIMTagPath& NCPTag);
	void CacheSearchFilteredPresets(const TArray<FBIMTagPath>& SourceNCPTags);

	void ToggleNCPTagAsSelected(const FBIMTagPath& NCPTag, bool bAsSelected);
	void SetNCPTagPathAsSelected(const FBIMTagPath& NCPTag);
	void RefreshDynamicMainListView();
	void ResetSelectedAndSearchTag();
	void ScrollPresetToView(const FGuid PresetToView);

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);
};
