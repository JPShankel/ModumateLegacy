// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMProperties.h"

#include "BIMBlockUserEnterable.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockUserEnterable : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockUserEnterable(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class UBIMDesigner *ParentBIMDesigner;

	EBIMValueScope Scope = EBIMValueScope::None;
	FBIMNameType NameTpye = NAME_None;
	int32 NodeID = -1;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *Text_Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *Text_Value;

	UFUNCTION()
	void OnEditableTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void BuildEnterableFieldFromProperty(class UBIMDesigner *OuterBIMDesigner, int32 InNodeID, const EBIMValueScope &InScope, const FBIMNameType &InNameType, const FString &InValue);
};
