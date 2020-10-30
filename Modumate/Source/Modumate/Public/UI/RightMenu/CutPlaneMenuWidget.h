// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModumateCore/ModumateTypes.h"

#include "CutPlaneMenuWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UCutPlaneMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCutPlaneMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

	UPROPERTY()
	TMap<int32, class UCutPlaneDimListItemObject*> HorizontalItemToIDMap;

	UPROPERTY()
	TMap<int32, class UCutPlaneDimListItemObject*> VerticalItemToIDMap;

	UPROPERTY()
	TMap<int32, class UCutPlaneDimListItemObject*> OtherItemToIDMap;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlock *CutPlaneMenuBlockHorizontal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlock *CutPlaneMenuBlockVertical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlock *CutPlaneMenuBlockOther;

	void SetViewMenuVisibility(bool NewVisible);
	void UpdateCutPlaneMenuBlocks();
	UCutPlaneDimListItemObject *GetListItemFromObjID(int32 ObjID = MOD_ID_NONE);
	bool RemoveCutPlaneFromMenuBlock(int32 ObjID = MOD_ID_NONE);
	bool UpdateCutPlaneParamInMenuBlock(int32 ObjID = MOD_ID_NONE);
};
