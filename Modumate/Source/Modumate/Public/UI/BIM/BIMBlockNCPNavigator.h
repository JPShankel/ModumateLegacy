// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMBlockNCPNavigator.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockNCPNavigator : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockNCPNavigator(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	FBIMTagPath CurrentNCP;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWrapBox* WrapBoxNCP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SeparatorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNCPNavigatorButton> NCPTextButtonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateTextBlockUserWidget> TextSeparatorClass;

	void BuildNCPNavigator(const FBIMTagPath& InNCP);
};
