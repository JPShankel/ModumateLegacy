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

	// Explicitly create widgets for all possible tool properties that we want to edit.
	// TODO: dynamically populate a vertical box of properties, based on what the current tool supports and how each property can be edited.

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox *HorizontalBoxInstWidth;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox *HorizontalBoxInstHeight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox *HorizontalBoxInstSillHeight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox *HorizontalBoxInstJustification;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBoxInstWidth;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBoxInstHeight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBoxInstSillHeight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget *DropDownInstJustification;

	UFUNCTION(BlueprintCallable)
	void ChangeBlockProperties(class UEditModelToolBase* CurrentTool);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnInstWidthTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnInstHeightTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnInstSillHeightTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnInstJustificationCommitted(FString SelectedItem, ESelectInfo::Type SelectionType);

};
