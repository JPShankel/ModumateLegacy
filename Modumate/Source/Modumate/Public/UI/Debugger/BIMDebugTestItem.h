// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Debugger/BIMDebugger.h"

#include "BIMDebugTestItem.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UBIMDebugTestItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMDebugTestItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	class UBIMDebugger* ParentDebugger;
	EBIMDebugTestType BIMDebugTestType = EBIMDebugTestType::TestDebugFalse;
	FString TestItemName;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateButton* ModumateButtonMain;

	UFUNCTION()
	void OnButtonMainReleased();

	void BuildDebugTestItem(class UBIMDebugger* OuterBIMDebugger, EBIMDebugTestType InBIMDebugTestType);
};
