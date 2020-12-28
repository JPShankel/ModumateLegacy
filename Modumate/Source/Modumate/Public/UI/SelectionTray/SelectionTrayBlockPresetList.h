// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "SelectionTrayBlockPresetList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API USelectionTrayBlockPresetList : public UUserWidget
{
	GENERATED_BODY()

public:
	USelectionTrayBlockPresetList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TMap<FGuid, class UComponentListObject*> ComponentItemMap;

public:
	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView *AssembliesList;

	void BuildPresetListFromSelection();
	void ClearPresetList();

};
