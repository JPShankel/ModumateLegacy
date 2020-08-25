// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/RightMenu/CutPlaneMenuBlock.h"
#include "Components/ListView.h"

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

void UCutPlaneMenuWidget::SetViewMenuVisibility(bool NewVisible)
{
	if (NewVisible)
	{
		this->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UpdateCutPlaneMenuBlocks();
	}
	else
	{
		this->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCutPlaneMenuWidget::UpdateCutPlaneMenuBlocks()
{
	TArray<UCutPlaneDimListItemObject*> verticalItems;
	CutPlaneMenuBlockHorizontal->CutPlanesList->ClearListItems();
	CutPlaneMenuBlockVertical->CutPlanesList->ClearListItems();
	CutPlaneMenuBlockOther->CutPlanesList->ClearListItems();

	TArray<FModumateObjectInstance*> cutPlaneMois = GameState->Document.GetObjectsOfType(EObjectType::OTCutPlane);
	for (int32 i = 0; i < cutPlaneMois.Num(); ++i)
	{
		UCutPlaneDimListItemObject *newCutPlaneObj = NewObject<UCutPlaneDimListItemObject>(this);
		newCutPlaneObj->DisplayName = FString(TEXT("CutPlane ")) + (FString::Printf(TEXT("%d"), cutPlaneMois[i]->ID)); //TODO: get name from moi property
		newCutPlaneObj->ObjId = cutPlaneMois[i]->ID;
		newCutPlaneObj->Location = cutPlaneMois[i]->GetObjectLocation();
		newCutPlaneObj->Rotation = cutPlaneMois[i]->GetObjectRotation();
		newCutPlaneObj->Visibility = cutPlaneMois[i]->IsVisible();

		// Place cut plane item by its orientation
		float cutPlaneUpDot = FMath::Abs(newCutPlaneObj->Rotation.GetUpVector() | FVector::UpVector);

		if (cutPlaneUpDot >= THRESH_NORMALS_ARE_PARALLEL)
		{
			// Vertical cut planes need to be sorted before adding into the list
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Vertical;
			verticalItems.Add(newCutPlaneObj);
		}
		else if (cutPlaneUpDot <= THRESH_NORMALS_ARE_ORTHOGONAL)
		{
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Horizontal;
			CutPlaneMenuBlockHorizontal->CutPlanesList->AddItem(newCutPlaneObj);
		}
		else
		{
			newCutPlaneObj->CutPlaneType = ECutPlaneType::Other;
			CutPlaneMenuBlockOther->CutPlanesList->AddItem(newCutPlaneObj);
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