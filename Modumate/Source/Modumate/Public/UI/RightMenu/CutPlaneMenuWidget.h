// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UI/RightMenu/GeneralListItem.h"

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
	class AEditModelGameState* GameState;

	UPROPERTY()
	TMap<int32, class UGeneralListItemObject*> HorizontalItemToIDMap;

	UPROPERTY()
	TMap<int32, class UGeneralListItemObject*> VerticalItemToIDMap;

	UPROPERTY()
	TMap<int32, class UGeneralListItemObject*> OtherItemToIDMap;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UGeneralListItemMenuBlock *CutPlaneMenuBlockHorizontal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UGeneralListItemMenuBlock *CutPlaneMenuBlockVertical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UGeneralListItemMenuBlock *CutPlaneMenuBlockOther;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlockExport *CutPlaneMenuBlockExport;

	void UpdateCutPlaneMenuBlocks();
	UGeneralListItemObject* GetListItemFromObjID(int32 ObjID = MOD_ID_NONE);
	bool RemoveCutPlaneFromMenuBlock(int32 ObjID = MOD_ID_NONE);
	bool UpdateCutPlaneParamInMenuBlock(int32 ObjID = MOD_ID_NONE);
	void SetCutPlaneExportMenuVisibility(bool NewVisible);
	static void BuildCutPlaneItemFromMoi(UGeneralListItemObject* CutPlaneObj, const class AModumateObjectInstance* Moi);

	// Show CutPlane by type, show all if none specified
	bool GetCutPlaneIDsByType(EGeneralListType Type, TArray<int32>& OutIDs);
};
