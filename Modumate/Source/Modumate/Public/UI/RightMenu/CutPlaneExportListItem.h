// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"


#include "CutPlaneExportListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UCutPlaneExportListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UCutPlaneExportListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	int32 ObjID = MOD_ID_NONE;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TooltipID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxExport;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextDimension;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButtonMain;

	UFUNCTION()
	void OnButtonMainReleased();

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	void BuildAsVerticalCutPlaneItem(const FQuat& Rotation);
	void BuildAsHorizontalCutPlaneItem(const FVector& Location);

protected:

	UFUNCTION()
	UWidget* OnTooltipWidget();
};
