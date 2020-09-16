// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMKey.h"
#include "ToolTrayBlockAssembliesList.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ESwapType : uint8
{
	SwapFromNode,
	SwapFromAssemblyList,
	None
};

UCLASS()
class MODUMATE_API UToolTrayBlockAssembliesList : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockAssembliesList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController_CPP* Controller;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

	ESwapType SwapType = ESwapType::None;
	int32 CurrentNodeForSwap = -1;

public:
	UPROPERTY()
	class UToolTrayWidget *ToolTray;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView *AssembliesList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonAdd;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateButtonUserWidget *ButtonCancel;

	UFUNCTION(BlueprintCallable)
	void CreateAssembliesListForCurrentToolMode();

	void CreatePresetListInNodeForSwap(const FBIMKey& ParentPresetID, const FBIMKey& PresetIDToSwap, int32 NodeID);
	void CreatePresetListInAssembliesListForSwap(EToolMode ToolMode, const FBIMKey& PresetID);

	UFUNCTION()
	void OnButtonAddReleased();

	UFUNCTION()
	void OnButtonCancelReleased();
};
