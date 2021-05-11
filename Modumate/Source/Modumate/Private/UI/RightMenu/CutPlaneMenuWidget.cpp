// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/CutPlane.h"
#include "UI/RightMenu/GeneralListItemObject.h"
#include "UI/RightMenu/GeneralListItemMenuBlock.h"
#include "Components/ListView.h"
#include "Objects/CutPlane.h"
#include "UI/RightMenu/CutPlaneMenuBlockExport.h"

using namespace Modumate;

UCutPlaneMenuWidget::UCutPlaneMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCutPlaneMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UCutPlaneMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());

}

void UCutPlaneMenuWidget::UpdateCutPlaneMenuBlocks()
{
	TArray<UGeneralListItemObject*> horizontalItems;
	HorizontalItemToIDMap.Empty();
	VerticalItemToIDMap.Empty();
	OtherItemToIDMap.Empty();
	CutPlaneMenuBlockHorizontal->GeneralItemsList->ClearListItems();
	CutPlaneMenuBlockVertical->GeneralItemsList->ClearListItems();
	CutPlaneMenuBlockOther->GeneralItemsList->ClearListItems();

	TArray<AModumateObjectInstance*> cutPlaneMois = GameState->Document->GetObjectsOfType(EObjectType::OTCutPlane);
	for (int32 i = 0; i < cutPlaneMois.Num(); ++i)
	{
		UGeneralListItemObject *newCutPlaneObj = NewObject<UGeneralListItemObject>(this);
		BuildCutPlaneItemFromMoi(newCutPlaneObj, cutPlaneMois[i]);

		// Place cut plane item by its orientation
		float cutPlaneUpDot = FMath::Abs(newCutPlaneObj->Rotation.GetUpVector() | FVector::UpVector);

		if (cutPlaneUpDot <= THRESH_NORMALS_ARE_ORTHOGONAL)
		{
			// Vertical cut planes need to be sorted before adding into the list
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Vertical;
			CutPlaneMenuBlockVertical->GeneralItemsList->AddItem(newCutPlaneObj);
			VerticalItemToIDMap.Add(cutPlaneMois[i]->ID, newCutPlaneObj);
		}
		else if (cutPlaneUpDot >= THRESH_NORMALS_ARE_PARALLEL)
		{
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Horizontal;
			horizontalItems.Add(newCutPlaneObj);
			HorizontalItemToIDMap.Add(cutPlaneMois[i]->ID, newCutPlaneObj);
		}
		else
		{
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Other;
			CutPlaneMenuBlockOther->GeneralItemsList->AddItem(newCutPlaneObj);
			OtherItemToIDMap.Add(cutPlaneMois[i]->ID, newCutPlaneObj);
		}
	}

	horizontalItems.Sort([](const UGeneralListItemObject &item1, const UGeneralListItemObject &item2) {
		return item1.Location.Z > item2.Location.Z;
	});
	for (auto& curItem : horizontalItems)
	{
		CutPlaneMenuBlockHorizontal->GeneralItemsList->AddItem(curItem);
	}
}

UGeneralListItemObject* UCutPlaneMenuWidget::GetListItemFromObjID(int32 ObjID /*MOD_ID_NONE*/)
{
	UGeneralListItemObject *outItem = HorizontalItemToIDMap.FindRef(ObjID);
	if (!outItem)
	{
		outItem = VerticalItemToIDMap.FindRef(ObjID);
	}
	if (!outItem)
	{
		outItem = OtherItemToIDMap.FindRef(ObjID);
	}
	return outItem;
}

bool UCutPlaneMenuWidget::RemoveCutPlaneFromMenuBlock(int32 ObjID /*= MOD_ID_NONE*/)
{
	UGeneralListItemObject *item = GetListItemFromObjID(ObjID);
	if (!item)
	{
		return false;
	}
	switch (item->CutPlaneType)
	{
	case ECutPlaneType::Horizontal:
		CutPlaneMenuBlockHorizontal->GeneralItemsList->RemoveItem(item);
		return true;
	case ECutPlaneType::Vertical:
		CutPlaneMenuBlockVertical->GeneralItemsList->RemoveItem(item);
		return true;
	case ECutPlaneType::Other:
		CutPlaneMenuBlockOther->GeneralItemsList->RemoveItem(item);
		return true;
	default:
		return false;
	}
}

bool UCutPlaneMenuWidget::UpdateCutPlaneParamInMenuBlock(int32 ObjID /*= MOD_ID_NONE*/)
{
	// Listview uses UCutPlaneDimListItemObject to build UCutPlaneDimListItem widget, both need to be updated
	UGeneralListItemObject* item = GetListItemFromObjID(ObjID);
	AModumateObjectInstance* moi = GameState->Document->GetObjectById(ObjID);
	if (!(item && moi))
	{
		return false;
	}

	// Update UCutPlaneDimListItemObject
	BuildCutPlaneItemFromMoi(item, moi);

	UGeneralListItemMenuBlock* block;
	switch (item->CutPlaneType)
	{
	case ECutPlaneType::Horizontal:
		block = CutPlaneMenuBlockHorizontal;
		break;
	case ECutPlaneType::Vertical:
		block = CutPlaneMenuBlockVertical;
		break;
	case ECutPlaneType::Other:
		block = CutPlaneMenuBlockOther;
		break;
	default:
		return false;
	}

	// Update UCutPlaneDimListItem from the updated CutPlaneDimListItemObject
	UGeneralListItem* itemWidget = Cast<UGeneralListItem>(block->GeneralItemsList->GetEntryWidgetFromItem(item));
	if (itemWidget)
	{
		itemWidget->NativeOnListItemObjectSet(item);
		return true;
	}
	return false;
}

void UCutPlaneMenuWidget::SetCutPlaneExportMenuVisibility(bool NewVisible)
{
	// If this menu is currently closed, open it
	if (NewVisible && this->GetVisibility() == ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UpdateCutPlaneMenuBlocks();
	}
	CutPlaneMenuBlockExport->SetExportMenuVisibility(NewVisible);
}

void UCutPlaneMenuWidget::BuildCutPlaneItemFromMoi(UGeneralListItemObject* CutPlaneObj, const class AModumateObjectInstance* Moi)
{
	FMOICutPlaneData cutPlaneData;
	if (ensure(Moi->GetStateData().CustomData.LoadStructData(cutPlaneData)))
	{
		CutPlaneObj->DisplayName = cutPlaneData.Name;
		CutPlaneObj->ObjId = Moi->ID;
		CutPlaneObj->Location = cutPlaneData.Location;
		CutPlaneObj->Rotation = cutPlaneData.Rotation;
		CutPlaneObj->Visibility = !Moi->IsRequestedHidden();
		CutPlaneObj->CanExport = cutPlaneData.bIsExported;

		const AMOICutPlane* moiCutPlane = Cast<AMOICutPlane>(Moi);
		if (moiCutPlane)
		{
			CutPlaneObj->bIsCulling = moiCutPlane->GetIsCulling();
		}
	}
}

bool UCutPlaneMenuWidget::GetCutPlaneIDsByType(ECutPlaneType Type, TArray<int32>& OutIDs)
{
	OutIDs.Reset();
	switch (Type)
	{
	case ECutPlaneType::Horizontal:
		for (auto curItem : HorizontalItemToIDMap)
		{
			OutIDs.Add(curItem.Key);
		}
		return true;
	case ECutPlaneType::Vertical:
		for (auto curItem : VerticalItemToIDMap)
		{
			OutIDs.Add(curItem.Key);
		}
		return true;
	case ECutPlaneType::Other:
		for (auto curItem : OtherItemToIDMap)
		{
			OutIDs.Add(curItem.Key);
		}
		return true;
	case ECutPlaneType::None:
		for (auto curItem : HorizontalItemToIDMap)
		{
			OutIDs.Add(curItem.Key);
		}
		for (auto curItem : VerticalItemToIDMap)
		{
			OutIDs.Add(curItem.Key);
		}
		for (auto curItem : OtherItemToIDMap)
		{
			OutIDs.Add(curItem.Key);
		}
		return true;
	default:
		return false;
	}
}
