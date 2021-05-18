// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelTerrainTool.h"

#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Objects/Terrain.h"
#include "UnrealClasses/LineActor.h"

#include <numeric>
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"


UTerrainTool::UTerrainTool()
{
	auto* world = GetWorld();
	if (world)
	{
		GameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	}
}

bool UTerrainTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	return Super::Activate();
}

bool UTerrainTool::Deactivate()
{
	Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return Super::Deactivate();
}

bool UTerrainTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	ZHeight = hitLoc.Z;

	TArray<AModumateObjectInstance*> terrainMois = doc->GetObjectsOfType(EObjectType::OTTerrain);
	for (const auto* tm : terrainMois)
	{
		if (tm->GetLocation().Z == ZHeight)
		{	// TODO: handle multiple terrain patches at same height.
			return false;
		}
	}

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	FVector xAxis;
	FVector yAxis;
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, FVector::ZeroVector, false);

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	if (dimensionActor != nullptr)
	{
		ALineActor* pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = hitLoc;
		pendingSegment->Point2 = hitLoc;
		pendingSegment->SetVisibilityInApp(true);
	}

	Points.Empty();
	Points.Add(hitLoc);
	State = FirstPoint;
	GameState->Document->StartPreviewing();

	return true;
}

bool UTerrainTool::EndUse()
{
	State = Idle;

	return Super::EndUse();
}

bool UTerrainTool::AbortUse()
{
	EndUse();
	return Super::AbortUse();
}

bool UTerrainTool::FrameUpdate()
{
	Super::FrameUpdate();
	CurDeltas.Reset();

	if (State == Idle || !Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return true;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (State != Idle)
	{
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		if (dimensionActor != nullptr)
		{
			auto pendingSegment = dimensionActor->GetLineActor();
			pendingSegment->Point2 = hitLoc;
			GameState->Document->StartPreviewing();
			GetDeltas(hitLoc, false,CurDeltas);
			GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
		}
	}

	return true;
}

bool UTerrainTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	if (State == FirstPoint || State == MultiplePoints)
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		hitLoc.Z = ZHeight;  // Force to affordance plane.
		if (State == MultiplePoints && hitLoc.Equals(Points[0], CloseLoopEpsilon))
		{
			MakeObject();
			return EndUse();
		}

		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		if (dimensionActor != nullptr)
		{
			ALineActor* pendingSegment = dimensionActor->GetLineActor();
			pendingSegment->Point1 = hitLoc;
			pendingSegment->Point2 = hitLoc;
			pendingSegment->SetVisibilityInApp(true);
		}

		Points.Add(hitLoc);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, FVector::ZeroVector, false);
		State = MultiplePoints;
	}

	return true;
}

bool UTerrainTool::PostEndOrAbort()
{
	if (GameState && GameState->Document)
	{
		GameState->Document->ClearPreviewDeltas(GetWorld(), false);
	}

	return Super::PostEndOrAbort();
}

void UTerrainTool::RegisterToolDataUI(class UToolTrayBlockProperties* PropertiesUI, int32& OutMaxNumRegistrations)
{
	static const FString heightPropertyName(TEXT("Height"));
	if (auto heightField = PropertiesUI->RequestPropertyField<UInstPropWidgetLinearDimension>(this, heightPropertyName))
	{
		heightField->RegisterValue(this, StartingZHeight);
		heightField->ValueChangedEvent.AddDynamic(this, &UTerrainTool::OnToolUIChangedHeight);
		OutMaxNumRegistrations = 1;
	}
}

void UTerrainTool::OnToolUIChangedHeight(float NewHeight)
{
	StartingZHeight = NewHeight;
}

bool UTerrainTool::GetDeltas(const FVector& CurrentPoint, bool bClosed, TArray<FDeltaPtr>& OutDeltas)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	int32 terrainMoiID = doc->GetNextAvailableID();
	int32 graph2dID = terrainMoiID + 1;
	const int32 startVertexID = graph2dID;

	const int32 numPoints = Points.Num();
	FMOITerrainData moiData;
	moiData.Origin = Points[0];
	for (int32 p = 0; p < numPoints; ++p)
	{
#if 0  // For testing:
		float h = FMath::RandRange(20.0f, 250.0f);
		moiData.Heights.Emplace(startVertexID + p, h);
#else
		moiData.Heights.Emplace(startVertexID + p, 30.48);
#endif
	}

	FMOIStateData stateData(terrainMoiID, EObjectType::OTTerrain);
	stateData.CustomData.SaveStructData(moiData);
	auto createMoiDelta = MakeShared<FMOIDelta>();
	createMoiDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

	auto createGraphDelta = MakeShared<FGraph2DDelta>(terrainMoiID, EGraph2DDeltaType::Add);
	auto addToGraphDelta = MakeShared<FGraph2DDelta>(terrainMoiID);

	for (const auto& p : Points)
	{
		FVector position = p - Points[0];
		addToGraphDelta->AddNewVertex(FVector2D(position), graph2dID);
	}
	if (!bClosed)
	{
		addToGraphDelta->AddNewVertex(FVector2D(CurrentPoint - Points[0]), graph2dID);
	}

	for (int32 i = 0; i < numPoints - 1; ++i)
	{
		FGraphVertexPair newEdge(startVertexID + i, startVertexID + 1 + i);
		addToGraphDelta->AddNewEdge(newEdge, graph2dID);
	}
	if (bClosed)
	{
		addToGraphDelta->AddNewEdge(FGraphVertexPair(startVertexID + numPoints - 1, startVertexID), graph2dID);
		TArray<int32> vertexArray;
		vertexArray.SetNum(Points.Num());
		std::iota(vertexArray.begin(), vertexArray.end(), startVertexID);
		addToGraphDelta->AddNewPolygon(vertexArray, graph2dID);
	}
	else
	{
		addToGraphDelta->AddNewEdge(FGraphVertexPair(startVertexID + numPoints - 1, startVertexID + numPoints), graph2dID);
	}

	OutDeltas.Add(createMoiDelta);
	OutDeltas.Add(createGraphDelta);
	OutDeltas.Add(addToGraphDelta);

	return true;
}

bool UTerrainTool::MakeObject()
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}


	doc->ClearPreviewDeltas(GetWorld());

	CurDeltas.Empty();
	bool bSuccess = GetDeltas(FVector::ZeroVector, true, CurDeltas);
	if (bSuccess)
	{
		return doc->ApplyDeltas(CurDeltas, GetWorld());
	}

	return bSuccess;
}
