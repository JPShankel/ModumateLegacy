// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Objects/CutPlane.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/RightMenu/CutPlaneMenuBlock.h"
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
	GameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());

}

void UCutPlaneMenuWidget::SetCutPlaneMenuVisibility(bool NewVisible)
{
	if (NewVisible)
	{
		this->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UpdateCutPlaneMenuBlocks();
	}
	else
	{
		CutPlaneMenuBlockExport->SetExportMenuVisibility(false);
		this->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCutPlaneMenuWidget::UpdateCutPlaneMenuBlocks()
{
	TArray<UCutPlaneDimListItemObject*> verticalItems;
	HorizontalItemToIDMap.Empty();
	VerticalItemToIDMap.Empty();
	OtherItemToIDMap.Empty();
	CutPlaneMenuBlockHorizontal->CutPlanesList->ClearListItems();
	CutPlaneMenuBlockVertical->CutPlanesList->ClearListItems();
	CutPlaneMenuBlockOther->CutPlanesList->ClearListItems();

	TArray<AModumateObjectInstance*> cutPlaneMois = GameState->Document.GetObjectsOfType(EObjectType::OTCutPlane);
	for (int32 i = 0; i < cutPlaneMois.Num(); ++i)
	{
		UCutPlaneDimListItemObject *newCutPlaneObj = NewObject<UCutPlaneDimListItemObject>(this);
		BuildCutPlaneItemFromMoi(newCutPlaneObj, cutPlaneMois[i]);

		// Place cut plane item by its orientation
		float cutPlaneUpDot = FMath::Abs(newCutPlaneObj->Rotation.GetUpVector() | FVector::UpVector);

		if (cutPlaneUpDot <= THRESH_NORMALS_ARE_ORTHOGONAL)
		{
			// Vertical cut planes need to be sorted before adding into the list
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Vertical;
			verticalItems.Add(newCutPlaneObj);
			VerticalItemToIDMap.Add(cutPlaneMois[i]->ID, newCutPlaneObj);
		}
		else if (cutPlaneUpDot >= THRESH_NORMALS_ARE_PARALLEL)
		{
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Horizontal;
			CutPlaneMenuBlockHorizontal->CutPlanesList->AddItem(newCutPlaneObj);
			HorizontalItemToIDMap.Add(cutPlaneMois[i]->ID, newCutPlaneObj);
		}
		else
		{
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Other;
			CutPlaneMenuBlockOther->CutPlanesList->AddItem(newCutPlaneObj);
			OtherItemToIDMap.Add(cutPlaneMois[i]->ID, newCutPlaneObj);
		}
	}

	verticalItems.Sort([](const UCutPlaneDimListItemObject &item1, const UCutPlaneDimListItemObject &item2) {
		return item1.Location.Z > item2.Location.Z;
	});
	for (auto& curItem : verticalItems)
	{
		CutPlaneMenuBlockVertical->CutPlanesList->AddItem(curItem);
	}
}

UCutPlaneDimListItemObject* UCutPlaneMenuWidget::GetListItemFromObjID(int32 ObjID /*MOD_ID_NONE*/)
{
	UCutPlaneDimListItemObject *outItem = HorizontalItemToIDMap.FindRef(ObjID);
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
	UCutPlaneDimListItemObject *item = GetListItemFromObjID(ObjID);
	if (!item)
	{
		return false;
	}
	switch (item->CutPlaneType)
	{
	case ECutPlaneType::Horizontal:
		CutPlaneMenuBlockHorizontal->CutPlanesList->RemoveItem(item);
		return true;
	case ECutPlaneType::Vertical:
		CutPlaneMenuBlockVertical->CutPlanesList->RemoveItem(item);
		return true;
	case ECutPlaneType::Other:
		CutPlaneMenuBlockOther->CutPlanesList->RemoveItem(item);
		return true;
	default:
		return false;
	}
}

bool UCutPlaneMenuWidget::UpdateCutPlaneParamInMenuBlock(int32 ObjID /*= MOD_ID_NONE*/)
{
	// Listview uses UCutPlaneDimListItemObject to build UCutPlaneDimListItem widget, both need to be updated
	UCutPlaneDimListItemObject* item = GetListItemFromObjID(ObjID);
	AModumateObjectInstance* moi = GameState->Document.GetObjectById(ObjID);
	if (!(item && moi))
	{
		return false;
	}

	// Update UCutPlaneDimListItemObject
	BuildCutPlaneItemFromMoi(item, moi);

	UCutPlaneMenuBlock* block;
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
	UCutPlaneDimListItem* itemWidget = Cast<UCutPlaneDimListItem>(block->CutPlanesList->GetEntryWidgetFromItem(item));
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
		SetCutPlaneMenuVisibility(true);
	}
	CutPlaneMenuBlockExport->SetExportMenuVisibility(NewVisible);
}

void UCutPlaneMenuWidget::BuildCutPlaneItemFromMoi(UCutPlaneDimListItemObject* CutPlaneObj, const class AModumateObjectInstance* Moi)
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
