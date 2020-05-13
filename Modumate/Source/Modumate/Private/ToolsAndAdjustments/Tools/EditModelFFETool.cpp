// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelFFETool.h"
#include "CompoundMeshActor.h"
#include "EditModelGameState_CPP.h"
#include "ModumateCommands.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateObjectDatabase.h"
#include "ModumateObjectStatics.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"

using namespace Modumate;

UPlaceObjectTool::UPlaceObjectTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Cursor(nullptr)
{}

bool UPlaceObjectTool::Activate()
{
	Super::Activate();

	// Spawn CompoundMeshActor as cursor
	CursorCompoundMesh = Controller->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform::Identity);
	CursorCompoundMesh->SetActorEnableCollision(false);
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;
	FName key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT).Key;
	const FModumateObjectAssembly *obAsmPtr = gameState->GetAssemblyByKey(key);

	if (!ensureAlways(obAsmPtr))
	{
		return false;
	}

	CurrentFFEAssembly = *obAsmPtr;
	CursorCompoundMesh->MakeFromAssembly(CurrentFFEAssembly, FVector::OneVector, false, false);
	TArray<UStaticMeshComponent*> smcs;
	CursorCompoundMesh->GetComponents(smcs);
	for (auto curSmc : smcs)
	{
		Cast<UStaticMeshComponent>(curSmc)->SetRelativeLocation(FVector::ZeroVector);
	}

	Controller->DeselectAll();
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	return true;
}

bool UPlaceObjectTool::ScrollToolOption(int32 dir)
{
	return false;
}

bool UPlaceObjectTool::Deactivate()
{
	Super::Deactivate();
	if (Cursor != nullptr)
	{
		Cursor->Destroy();
		Cursor = nullptr;
	}
	if (CursorCompoundMesh != nullptr)
	{
		CursorCompoundMesh->Destroy();
		CursorCompoundMesh = nullptr;
	}

	return true;
}

bool UPlaceObjectTool::FrameUpdate()
{
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;
	FName key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT).Key;
	const FModumateObjectAssembly *assembly = gameState->GetAssemblyByKey_DEPRECATED(EToolMode::VE_PLACEOBJECT, key);


	if (assembly != nullptr)
	{
		if (CurrentFFEAssembly.DatabaseKey != assembly->DatabaseKey)
		{
			if (CursorCompoundMesh != nullptr)
			{
				CursorCompoundMesh->MakeFromAssembly(*assembly, FVector::OneVector, false, false);
				TArray<UStaticMeshComponent*> smcs;
				CursorCompoundMesh->GetComponents(smcs);
				for (auto curSmc : smcs)
				{
					Cast<UStaticMeshComponent>(curSmc)->SetRelativeLocation(FVector::ZeroVector);
				}
			}
		}

		CurrentFFEAssembly = *assembly;
	}

	FTransform mountedTransform;
	if (UModumateObjectStatics::GetFFEMountedTransform(&CurrentFFEAssembly,
		Controller->EMPlayerState->SnappedCursor, mountedTransform))
	{
		CursorCompoundMesh->SetActorTransform(mountedTransform);
	}

	return true;
}

bool UPlaceObjectTool::BeginUse()
{
	Super::BeginUse();
	Super::EndUse();

	const FSnappedCursor &snappedCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snappedCursor.WorldPosition;

	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;
	FName key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT).Key;

	FModumateObjectInstance *hitMOI = doc->ObjectFromActor(snappedCursor.Actor);
	int32 parentID = hitMOI != nullptr ? hitMOI->ID : Controller->EMPlayerState->GetViewGroupObjectID();
	int32 parentFaceIdx = UModumateObjectStatics::GetFaceIndexFromTargetHit(hitMOI, hitLoc, snappedCursor.HitNormal);

	FMOIStateData state;

	state.StateType = EMOIDeltaType::Create;
	state.ControlIndices = { parentFaceIdx };
	state.Location = hitLoc;
	state.Orientation = CursorCompoundMesh->GetActorRotation().Quaternion();
	state.ParentID = parentID;
	state.ObjectAssemblyKey = key;
	state.ObjectType = EObjectType::OTFurniture;
	state.ObjectID = doc->GetNextAvailableID();

	TSharedPtr <FMOIDelta> delta = MakeShareable(new FMOIDelta({ state }));
	Controller->ModumateCommand(delta->AsCommand());

	return true;
}
