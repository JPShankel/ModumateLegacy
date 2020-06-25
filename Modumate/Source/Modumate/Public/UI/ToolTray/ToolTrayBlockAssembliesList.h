// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ToolTrayBlockAssembliesList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayBlockAssembliesList : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockAssembliesList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox *AssembliesList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UComponentAssemblyListItem> ComponentAssemblyListItemClass;

	UFUNCTION(BlueprintCallable)
	void CreateAssembliesListForCurrentToolMode();
};
