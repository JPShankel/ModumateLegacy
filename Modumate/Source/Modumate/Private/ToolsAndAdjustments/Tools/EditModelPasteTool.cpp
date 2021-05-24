// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPasteTool.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



UPasteTool::UPasteTool()
	: Super()
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

	if (!PasteJson->TryGetObjectField(DocObjectInstanceField, docOb))
	{
		return false;
	}

	// TODO: add reset function
	CurrentRecord = FMOIDocumentRecord();
	if (!FJsonObjectConverter::JsonObjectToUStruct<FMOIDocumentRecord>(docOb->ToSharedRef(), &CurrentRecord))
	{
		return false;
	}

	if (CurrentRecord.VolumeGraph.Vertices.Num() > 0)
	{
		for (auto& vertexRecord : CurrentRecord.VolumeGraph.Vertices)
		{
			auto& pos = vertexRecord.Value.Position;

			if (PasteOrigin.IsZero())
			{
				PasteOrigin = FVector(pos);
			}

			// prefer negative X (left), positive y (towards screen), then negative z (down)
			bool bLess = (pos.X < PasteOrigin.X) ? true : (pos.Y > PasteOrigin.Y) ? true : (pos.Z < PasteOrigin.Z);
			if (bLess)
			{
				PasteOrigin = FVector(pos);
			}
		}
	}
	else if (CurrentRecord.SurfaceGraphs.Num() == 1)
	{
		// TODO: doesn't work between files because it requires the original surface graph to exist
		// this could be solved by saving a paste origin in the record

		auto iterator = CurrentRecord.SurfaceGraphs.CreateConstIterator();
		auto originalSurfaceGraphMoi = GameState->Document->GetObjectById(iterator->Key);
		if (originalSurfaceGraphMoi)
		{
			FVector2D pasteOrigin2D = FVector2D::ZeroVector;
			for (auto& vertexRecord : iterator->Value.Vertices)
			{
				auto& pos = vertexRecord.Value;

				if (pasteOrigin2D.IsZero())
				{
					pasteOrigin2D = pos;
				}

				bool bLess = (pos.X < pasteOrigin2D.X) ? true : (pos.Y < PasteOrigin.Y);
				if (bLess)
				{
					pasteOrigin2D = pos;
				}
			}

			FTransform originalTransform = FTransform(originalSurfaceGraphMoi->GetRotation(), originalSurfaceGraphMoi->GetLocation());
			FVector originalX = originalTransform.GetUnitAxis(EAxis::X);
			FVector originalY = originalTransform.GetUnitAxis(EAxis::Y);

			PasteOrigin = UModumateGeometryStatics::Deproject2DPoint(pasteOrigin2D, originalX, originalY, originalTransform.GetLocation());
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
	GameState->Document->StartPreviewing();
	FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, PasteOrigin, GameState->Document, Controller, true);

	return true;
}

bool UPasteTool::EnterNextStage()
{
	Super::EnterNextStage();

	CurDeltas.Reset();
	GameState->Document->ClearPreviewDeltas(GetWorld());
	FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, PasteOrigin, GameState->Document, Controller, false);

	return true;
}
