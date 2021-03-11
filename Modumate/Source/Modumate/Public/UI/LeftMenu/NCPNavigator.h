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

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicMainList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* SearchBarWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNCPButton> NCPButtonClass;

	void BuildNCPNavigator();
	bool IsPresetAvailableForSearch(const FGuid& PresetGuid);
	bool IsNCPAvailableForSearch(const FBIMTagPath& NCPTag);

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);
};
