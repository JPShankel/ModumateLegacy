// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "SwapMenuWidget.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ESwapMenuType : uint8
{
	SwapFromNode,
	SwapFromSelection,
	None
};

UCLASS()
class MODUMATE_API USwapMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USwapMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	FGuid PresetGUIDToSwap;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	ESwapMenuType CurrentSwapMenuType = ESwapMenuType::None;
	FBIMEditorNodeIDType SwapBIMNodeID;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPNavigator* NCPNavigatorWidget;

	void SetSwapMenuAsSelection(FGuid InPresetGUIDToSwap);
	void BuildSwapMenu();

	const FGuid& GetPresetGUIDToSwap() const { return PresetGUIDToSwap; }
};
