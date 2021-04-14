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
	EPresetCardType PresetCardType = EPresetCardType::None;

	void ConfirmGUID(const FGuid& InGUID);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImageSmall;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UOverlay* OverlayIconSmall;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDuplicate;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	// Builder
	void BuildAsBrowserHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAllowOptions = true);
	void BuildAsSwapHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAllowOptions = true);
	void BuildAsDeleteHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAllowOptions);
	void BuildAsSelectTrayPresetCard(const FGuid& InGUID, int32 ItemCount, bool bAllowOptions = true);
	void BuildAsSelectTrayPresetCardObjectType(EObjectType InObjectType, int32 ItemCount, bool bAllowOptions = true);
	void BuildAsAssembliesListHeader(const FGuid& InGUID, bool bAllowOptions = true);

	void UpdateOptionButtonSetByPresetCardType(EPresetCardType InPresetCardType, bool bHideAllOnly);
	void UpdateSelectionHeaderItemCount(int32 ItemCount);
	bool CaptureIcon(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAsAssembly);
	void UpdateEditButtonIfPresetIsEditable();

	UFUNCTION()
	void OnButtonEditReleased();

	UFUNCTION()
	void OnButtonSwapReleased();

	UFUNCTION()
	void OnButtonTrashReleased();

	UFUNCTION()
	void OnButtonConfirmReleased();

	UFUNCTION()
	void OnButtonDuplicateReleased();
};
