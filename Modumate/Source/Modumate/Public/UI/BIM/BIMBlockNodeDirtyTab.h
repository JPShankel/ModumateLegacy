// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMBlockNodeDirtyTab.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockNodeDirtyTab : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockNodeDirtyTab(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSave;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonAddNew;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonCancel;
};
