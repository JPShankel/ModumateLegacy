// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPasteTool.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

using namespace Modumate;

UPasteTool::UPasteTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UPasteTool::Activate()
{
	PasteOrigin = FVector::ZeroVector;

	FString pasteString;
	FPlatformApplicationMisc::ClipboardPaste(pasteString);

	auto JsonReader = TJsonReaderFactory<>::Create(pasteString);

	TSharedPtr<FJsonObject> PasteJson;
	bool bLoadJsonSuccess = FJsonSerializer::Deserialize(JsonReader, PasteJson) && PasteJson.IsValid();
	if (!bLoadJsonSuccess)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> *docOb;

	if (!PasteJson->TryGetObjectField(Modumate::DocObjectInstanceField, docOb))
	{
		return false;
	}

	// TODO: add reset function
	CurrentRecord = FMOIDocumentRecord();
	if (!FJsonObjectConverter::JsonObjectToUStruct<FMOIDocumentRecord>(docOb->ToSharedRef(), &CurrentRecord))
	{
		return false;
	}

	for (auto& vertexRecord : CurrentRecord.VolumeGraph.Vertices)
	{
		auto& pos = vertexRecord.Value.Position;

		if (PasteOrigin.IsZero())
		{
			PasteOrigin = pos;
		}

		// prefer negative X (left), positive y (towards screen), then negative z (down)
		bool bLess = (pos.X < PasteOrigin.X) ? true : (pos.Y > PasteOrigin.Y) ? true : (pos.Z < PasteOrigin.Z);
		if (bLess)
		{
			PasteOrigin = pos;
		}
	}

	Super::Activate();
	return BeginUse();
}

bool UPasteTool::FrameUpdate()
{
	if (!Super::FrameUpdate())
	{
		return false;
	}

	CurDeltas.Reset();

	FVector currentOffset;

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	currentOffset = hitLoc - PasteOrigin;
		
	GameState->Document->StartPreviewing();

	GameState->Document->PasteMetaObjects(&CurrentRecord.VolumeGraph, CurDeltas, currentOffset, true);

	GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());

	return true;
}

bool UPasteTool::EnterNextStage()
{
	Super::EnterNextStage();

	FVector currentOffset;
	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	currentOffset = hitLoc - PasteOrigin;

	CurDeltas.Reset();
	GameState->Document->ClearPreviewDeltas(GetWorld());
	if (!GameState->Document->PasteMetaObjects(&CurrentRecord.VolumeGraph, CurDeltas, currentOffset, false))
	{
		return false;
	}
	GameState->Document->ApplyDeltas(CurDeltas, GetWorld());

	return true;
}
