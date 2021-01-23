// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMBlockNCPSwitcher.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockNCPSwitcher : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockNCPSwitcher(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	FBIMTagPath CurrentNCP;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonClose;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* VerticalBoxNCP;

	// Padding width under its parent tag
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TagPaddingSize = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNCPSwitcherButton> NCPSwitcherButtonClassSelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNCPSwitcherButton> NCPSwitcherButtonClassDefault;

	UFUNCTION()
	void OnButtonClosePress();

	void BuildNCPSwitcher(const FBIMTagPath& InCurrentNCP);
	void AddNCPTagButtons(int32 TagOrder);
};
