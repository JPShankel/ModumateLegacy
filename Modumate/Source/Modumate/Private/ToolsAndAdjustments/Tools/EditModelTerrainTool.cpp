// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelTerrainTool.h"

#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UI/ToolTray/ToolTrayBlockModes.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Objects/Terrain.h"
#include "UnrealClasses/LineActor.h"
#include "Graph/Graph2D.h"

#include <numeric>
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"


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

	TerrainGraph.Reset();
	TArray<AModumateObjectInstance*> terrainMois = doc->GetObjectsOfType(EObjectType::OTTerrain);
	for (auto* tm : terrainMois)
	{
		if (FMath::IsNearlyEqual(tm->GetLocation().Z, ZHeight, THRESH_POINT_ON_PLANE))
		{
			TerrainGraph = doc->FindSurfaceGraph(tm->ID);
			if (!ensure(TerrainGraph.IsValid()))
			{
				return false;
			}
			ZHeight = tm->GetLocation().Z;
			hitLoc.Z = ZHeight;
		}
	}

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, FVector::ZeroVector, false);
	CurrentPoint = hitLoc;

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	if (dimensionActor != nullptr)
	{
		ALineActor* pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = CurrentPoint;
		pendingSegment->Point2 = CurrentPoint;
		pendingSegment->SetVisibilityInApp(true);
	}

	Points.Empty();
	Points.Add(hitLoc);
	State = AddEdge;
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

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	hitLoc.Z = ZHeight;  // Project to terrain height.

	if (State == AddEdge)
	{
		GameState->Document->ClearPreviewDeltas(GetWorld(), false);

		if (TerrainGraph == nullptr)
		{
			AddFirstEdge(CurrentPoint, hitLoc);
		}
		else
		{
			AddNewEdge(CurrentPoint, hitLoc);
		}

		CurrentPoint = hitLoc;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, FVector::ZeroVector, false);
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		if (dimensionActor != nullptr)
		{
			ALineActor* pendingSegment = dimensionActor->GetLineActor();
			pendingSegment->Point1 = CurrentPoint;
			pendingSegment->Point2 = CurrentPoint;
		}
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

bool UTerrainTool::HandleInputNumber(double n)
{
	Super::HandleInputNumber(n);

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	if (State == AddEdge)
	{
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		if (dimensionActor != nullptr)
		{
			ALineActor* pendingSegment = dimensionActor->GetLineActor();
			FVector endPoint = pendingSegment->Point1 + (pendingSegment->Point2 - pendingSegment->Point1).GetSafeNormal() * n;
			endPoint.Z = ZHeight;

			if (TerrainGraph == nullptr)
			{
				AddFirstEdge(pendingSegment->Point1, endPoint);
			}
			else
			{
				AddNewEdge(pendingSegment->Point1, endPoint);
			}
			pendingSegment->Point1 = pendingSegment->Point2 = CurrentPoint = endPoint;
			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(CurrentPoint, FVector::UpVector, FVector::ZeroVector, false);
		}
	}

	return true;
}

// Add the first edge of a new terrain MOI.
bool UTerrainTool::AddFirstEdge(FVector Point1, FVector Point2)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	int32 nextID = doc->GetNextAvailableID();
	FMOITerrainData moiData;
	moiData.Name = GetNextName();
	moiData.Origin = Point1;  // First point will be origin for new Terrain.
	moiData.Heights.Add(nextID + 1, StartingZHeight);
	moiData.Heights.Add(nextID + 2, StartingZHeight);

	FMOIStateData stateData(nextID, EObjectType::OTTerrain);
	stateData.CustomData.SaveStructData(moiData);
	auto createMoiDelta = MakeShared<FMOIDelta>();
	createMoiDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

	TerrainGraph = MakeShared<FGraph2D>(nextID, THRESH_POINTS_ARE_NEAR);
	if (!ensure(TerrainGraph.IsValid()))
	{
		return false;
	}
	++nextID;

	TArray<FGraph2DDelta> addEdgeDeltas;
	if (!TerrainGraph->AddEdge(addEdgeDeltas, nextID, FVector2D::ZeroVector, ProjectToPlane(Point1, Point2)))
	{
		return false;
	}

	TArray<FDeltaPtr> deltas;
	deltas.Add(createMoiDelta);
	deltas.Add(MakeShared<FGraph2DDelta>(TerrainGraph->GetID(), EGraph2DDeltaType::Add, THRESH_POINTS_ARE_NEAR));
	if (!doc->FinalizeGraph2DDeltas(addEdgeDeltas, nextID, deltas))
	{
		return false;
	}

	bool bRetVal = doc->ApplyDeltas(deltas, GetWorld());

	TerrainGraph = doc->FindSurfaceGraph(TerrainGraph->GetID());

	return bRetVal;
}

bool UTerrainTool::AddNewEdge(FVector Point1, FVector Point2)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	auto* terrainMoi = Cast<AMOITerrain>(doc->GetObjectById(TerrainGraph->GetID()));
	if (terrainMoi == nullptr)
	{
		return false;
	}
	FVector origin = terrainMoi->GetLocation();

	int32 nextID = doc->GetNextAvailableID();
	TArray<FGraph2DDelta> addEdgeDeltas;

	if (!TerrainGraph->AddEdge(addEdgeDeltas, nextID, ProjectToPlane(origin, Point1), ProjectToPlane(origin, Point2)) )
	{
		return false;
	}
	TArray<FDeltaPtr> deltas;
	if (!doc->FinalizeGraph2DDeltas(addEdgeDeltas, nextID, deltas))
	{
		return false;
	}

	FMOITerrainData moiData = terrainMoi->InstanceData;
	for (const auto& delta: addEdgeDeltas)
	{
		for (const auto& deletion : delta.VertexDeletions)
		{
			moiData.Heights.Remove(deletion.Key);
		}
		for (const auto& additions : delta.VertexAdditions)
		{
			moiData.Heights.Add(additions.Key, StartingZHeight);
		}
	}
	auto deltaPtr = MakeShared<FMOIDelta>();
	FMOIStateData& newState = deltaPtr->AddMutationState(terrainMoi);
	newState.CustomData.SaveStructData(moiData);

	deltas.Add(deltaPtr);

	return doc->ApplyDeltas(deltas, GetWorld());
}

FVector2D UTerrainTool::ProjectToPlane(FVector Origin, FVector Point)
{
	return FVector2D(Point.X - Origin.X, Point.Y - Origin.Y);
}

FString UTerrainTool::GetNextName() const
{
	static const TCHAR namePattern[] = TEXT("Terrain %d");
	int32 n = 1;
	const UModumateDocument* doc = GameState->Document;

	auto existingTerrain = doc->GetObjectsOfType(EObjectType::OTTerrain);
	TSet<FString> existingNames;
	for (const auto* terrain : existingTerrain)
	{
		FMOIStateData stateData = terrain->GetStateData();
		FMOITerrainData terrainData;
		stateData.CustomData.LoadStructData(terrainData);
		existingNames.Add(terrainData.Name);
	}

	FString candidate;
	do
	{
		candidate = FString::Printf(namePattern, n++);
	} while (existingNames.Contains(candidate));

	return candidate;
}
