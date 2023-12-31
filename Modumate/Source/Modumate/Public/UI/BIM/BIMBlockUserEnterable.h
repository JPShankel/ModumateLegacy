// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"

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

	FBIMPresetFormElement FormElement;

	FBIMEditorNodeIDType NodeID;
	FString OriginalValueString;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *Text_Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *Text_Value;

	UFUNCTION()
	void OnEditableTextBoxCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void BuildEnterableFieldFromProperty(class UBIMDesigner *OuterBIMDesigner, const FBIMEditorNodeIDType& InNodeID, const FBIMPresetFormElement& InFormElement);
};
