// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectAssembly.h"

#include "ComponentAssemblyListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UComponentAssemblyListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UComponentAssemblyListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FName AsmKey;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonMain;

	UFUNCTION()
	void OnModumateButtonMainReleased();

	bool BuildFromAssembly(const FModumateObjectAssembly *Asm);

};
