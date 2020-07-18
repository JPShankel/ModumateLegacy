// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "SelectionTrayBlockPresetList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API USelectionTrayBlockPresetList : public UUserWidget
{
	GENERATED_BODY()

public:
	USelectionTrayBlockPresetList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TMap<FName, class UComponentAssemblyListItem*> ComponentItemMap;

public:
	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	// TODO: Candidate for changing to UListViewBase 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox *AssembliesList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UComponentAssemblyListItem> ComponentSelectionListItemClass;

	void BuildPresetListFromSelection();
	void ClearPresetList();

};
