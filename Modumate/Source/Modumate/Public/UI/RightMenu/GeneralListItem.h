// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "ModumateCore/ModumateTypes.h"

#include "GeneralListItem.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class EGeneralListType : uint8
{
	Horizontal,
	Vertical,
	Other,
	Terrain,
	None
};

UCLASS()
class MODUMATE_API UGeneralListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UGeneralListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	int32 ObjID = MOD_ID_NONE;
	EGeneralListType CurrentGeneralListType = EGeneralListType::None;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TooltipID_CheckBoxVisibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TooltipID_CheckBoxCullModel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox* CheckBoxVisibility;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextDimension;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* TextTitleEditable;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButtonMain;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonEdit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxCullModel;

	UFUNCTION()
	void OnButtonMainReleased();

	UFUNCTION()
	void OnButtonEditReleased();

	UFUNCTION()
	void OnCheckBoxVisibilityChanged(bool IsChecked);

	UFUNCTION()
	void OnEditableTitleCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnCheckBoxCullModelChanged(bool IsChecked);

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	void BuildAsVerticalCutPlaneItem(const FQuat& Rotation);
	void BuildAsHorizontalCutPlaneItem(const FVector& Location);
	void BuildAsTerrainItem(const class UGeneralListItemObject* TerrainItemObject);
	void UpdateVisibilityAndName(bool NewVisible, const FString& NewName);

protected:

	UFUNCTION()
	UWidget* OnCheckBoxTooltipWidget();

	UFUNCTION()
	UWidget* OnCheckBoxCullModelTooltipWidget();
};
