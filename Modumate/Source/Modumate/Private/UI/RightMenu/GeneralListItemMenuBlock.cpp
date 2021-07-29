// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/GeneralListItemMenuBlock.h"
#include "Components/ListView.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UI/RightMenu/GeneralListItemObject.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/Terrain.h"


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
		const AMOITerrain* moiTerrain = Cast<AMOITerrain>(terrainMOIs[i]);
		if (moiTerrain && !moiTerrain->IsDestroyed())
		{
			FMOITerrainData terrainData;
			if (ensure(terrainMOIs[i]->GetStateData().CustomData.LoadStructData(terrainData)))
			{
				UGeneralListItemObject* newTerrainObj = NewObject<UGeneralListItemObject>(this);
				newTerrainObj->CutPlaneType = EGeneralListType::Terrain;
				newTerrainObj->DisplayName = terrainData.Name;
				newTerrainObj->ObjId = terrainMOIs[i]->ID;
				newTerrainObj->bIsCulling = moiTerrain->GetIsTranslucent();
				
				//There are a few ways to get the 'visibility' of an MOI, but in this case
				// we are going to check the inversion of setting the checkbox instead of
				// querying the MOI directly.
				UModumateDocument* doc = gameState->Document;
				auto isHiddenInDoc = doc->HiddenObjectsID.Contains(newTerrainObj->ObjId);
				newTerrainObj->Visibility = !isHiddenInDoc;

				GeneralItemsList->AddItem(newTerrainObj);
			}
		}
	}
}
