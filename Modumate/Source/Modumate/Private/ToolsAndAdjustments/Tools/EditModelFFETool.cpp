// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFFETool.h"

#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/FFE.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

using namespace Modumate;

UFFETool::UFFETool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Cursor(nullptr)
{}

bool UFFETool::Activate()
{
	Super::Activate();
	// Spawn CompoundMeshActor as cursor
	CursorCompoundMesh = Controller->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform::Identity);
	CursorCompoundMesh->SetActorEnableCollision(false);
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;
	FBIMKey key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT);
	const FBIMAssemblySpec *obAsmPtr = doc->PresetManager.GetAssemblyByKey(EToolMode::VE_PLACEOBJECT,key);

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

bool UFFETool::ScrollToolOption(int32 dir)
{
	return false;
}

bool UFFETool::Deactivate()
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

bool UFFETool::FrameUpdate()
{
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;
	FBIMKey key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT);
	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(EToolMode::VE_PLACEOBJECT, key);

	if (assembly != nullptr)
	{
		if (CurrentFFEAssembly.UniqueKey() != assembly->UniqueKey())
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

bool UFFETool::BeginUse()
{
	Super::BeginUse();
	Super::EndUse();

	const FSnappedCursor &snappedCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snappedCursor.WorldPosition;

	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;
	FBIMKey key = Controller->EMPlayerState->GetAssemblyForToolMode(EToolMode::VE_PLACEOBJECT);

	FModumateObjectInstance *hitMOI = doc->ObjectFromActor(snappedCursor.Actor);
	int32 parentID = hitMOI != nullptr ? hitMOI->ID : Controller->EMPlayerState->GetViewGroupObjectID();
	int32 parentFaceIdx = UModumateObjectStatics::GetFaceIndexFromTargetHit(hitMOI, hitLoc, snappedCursor.HitNormal);

	FMOIFFEData ffeData;
	ffeData.Location = hitLoc;
	ffeData.Rotation = CursorCompoundMesh->GetActorRotation().Quaternion();
	ffeData.bLateralInverted = false;
	ffeData.ParentFaceIndex = parentFaceIdx;

	FMOIStateData stateData(doc->GetNextAvailableID(), EObjectType::OTFurniture, parentID);
	stateData.AssemblyKey = key;
	stateData.CustomData.SaveStructData(ffeData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

	doc->ApplyDeltas({ delta }, GetWorld());

	return true;
}

TArray<EEditViewModes> UFFETool::GetRequiredEditModes() const
{
	return { EEditViewModes::ObjectEditing };
}
