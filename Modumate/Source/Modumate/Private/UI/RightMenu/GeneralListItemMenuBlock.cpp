// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/GeneralListItemMenuBlock.h"
#include "Components/ListView.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UI/RightMenu/GeneralListItemObject.h"
#include "Objects/ModumateObjectInstance.h"


UGeneralListItemMenuBlock::UGeneralListItemMenuBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UGeneralListItemMenuBlock::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	return true;
}

void UGeneralListItemMenuBlock::NativeConstruct()
{
	Super::NativeConstruct();
}

void UGeneralListItemMenuBlock::UpdateAsTerrainList()
{
	AEditModelGameState* gameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
	if (!gameState)
	{
		return;
	}

	GeneralItemsList->ClearListItems();

	TArray<AModumateObjectInstance*> terrainMOIs = gameState->Document->GetObjectsOfType(EObjectType::OTTerrain);
	for (int32 i = 0; i < terrainMOIs.Num(); ++i)
	{
		// TODO: Terrain CustomData?
		UGeneralListItemObject* newTerrainObj = NewObject<UGeneralListItemObject>(this);
		newTerrainObj->CutPlaneType = EGeneralListType::Terrain;
		newTerrainObj->ObjId = terrainMOIs[i]->ID;
		newTerrainObj->DisplayName = FString(TEXT("New Terrain"));

		GeneralItemsList->AddItem(newTerrainObj);
	}
}
