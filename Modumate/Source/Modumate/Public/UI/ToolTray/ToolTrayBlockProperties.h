// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"

#include "ToolTrayBlockProperties.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayBlockProperties : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockProperties(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox *HorizontalBoxInstHeight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox *HorizontalBoxInstJustification;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBoxInstHeight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget *DropDownInstJustification;

	UFUNCTION(BlueprintCallable)
	void ChangeBlockProperties(EToolMode mode);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnInstHeightTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnInstJustificationCommitted(FString SelectedItem, ESelectInfo::Type SelectionType);

};
