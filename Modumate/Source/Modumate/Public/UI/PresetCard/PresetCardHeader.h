// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/PresetCard/PresetCardMain.h"
#include "PresetCardHeader.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardHeader : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardHeader(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	FGuid PresetGUID;
	FText ItemDisplayName;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImageSmall;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonEdit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonSwap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonTrash;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonConfirm;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	// Builder
	void BuildAsBrowserHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID);
	void BuildAsSwapHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID);
	void BuildAsSelectTrayPresetCard(const FGuid& InGUID, int32 ItemCount);
	void BuildAsSelectTrayPresetCardObjectType(EObjectType InObjectType, int32 ItemCount);

	void UpdateButtonSetByPresetCardType(EPresetCardType InPresetCardType);
	void UpdateSelectionHeaderItemCount(int32 ItemCount);
	bool CaptureIcon(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAsAssembly);

	UFUNCTION()
	void OnButtonEditReleased();

	UFUNCTION()
	void OnButtonSwapReleased();

	UFUNCTION()
	void OnButtonConfirmReleased();
};
