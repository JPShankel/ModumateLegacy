// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelFFETool.h"
#include "CompoundMeshActor.h"
#include "EditModelGameState_CPP.h"
#include "ModumateCommands.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateObjectDatabase.h"
#include "ModumateObjectStatics.h"
#include "EditModelPlayerController_CPP.h"

namespace Modumate {

	FPlaceObjectTool::FPlaceObjectTool(AEditModelPlayerController_CPP *pc) :
		FEditModelToolBase(pc)
		, Cursor(nullptr)
	{}

	FPlaceObjectTool::~FPlaceObjectTool() {}


	bool FPlaceObjectTool::Activate()
	{
		FEditModelToolBase::Activate();

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
		TArray<UActorComponent*> smcs = CursorCompoundMesh->GetComponentsByClass(UStaticMeshComponent::StaticClass());
		for (auto curSmc : smcs)
		{
			Cast<UStaticMeshComponent>(curSmc)->SetRelativeLocation(FVector::ZeroVector);
		}

		Controller->DeselectAll();
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

		return true;
	}

	bool FPlaceObjectTool::ScrollToolOption(int32 dir)
	{
		return false;
	}

	bool FPlaceObjectTool::Deactivate()
	{
		FEditModelToolBase::Deactivate();
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

	bool FPlaceObjectTool::FrameUpdate()
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
					TArray<UActorComponent*> smcs = CursorCompoundMesh->GetComponentsByClass(UStaticMeshComponent::StaticClass());
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

	bool FPlaceObjectTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();
		FEditModelToolBase::EndUse();

		const FSnappedCursor &snappedCursor = Controller->EMPlayerState->SnappedCursor;
		FVector hitLoc = snappedCursor.WorldPosition;

		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		Modumate::FModumateDocument *doc = &gameState->Document;
		FName key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT).Key;

		FModumateObjectInstance *hitMOI = doc->ObjectFromActor(snappedCursor.Actor);
		int32 parentID = hitMOI != nullptr ? hitMOI->ID : Controller->EMPlayerState->GetViewGroupObjectID();
		int32 parentFaceIdx = UModumateObjectStatics::GetFaceIndexFromTargetHit(hitMOI, hitLoc, snappedCursor.HitNormal);

		Controller->ModumateCommand(
			FModumateCommand(Commands::kAddFFE)
			.Param(Parameters::kAssembly, key)
			.Param(Parameters::kLocation, hitLoc)
			.Param(Parameters::kRotator, CursorCompoundMesh->GetActorRotation())
			.Param(Parameters::kParent, parentID)
			.Param(Parameters::kIndex, parentFaceIdx)
		);

		return true;
	}

	IModumateEditorTool *MakePlaceObjectTool(AEditModelPlayerController_CPP *controller)
	{
		return new FPlaceObjectTool(controller);
	}
};