// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/RoofPerimeterHandles.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateRoofStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UI/RoofPerimeterPropertiesWidget.h"
#include "UI/WidgetClassAssetData.h"

using namespace Modumate;

bool ACreateRoofFacesHandle::BeginUse()
{
	if (!ensure(TargetMOI && GameState))
	{
		return false;
	}

	UWorld *world = GetWorld();

	// Get the hint normal for the roof, assuming it's erring on the side of Z+
	FVector roofNormal = TargetMOI->GetNormal();
	if (roofNormal.Z < 0.0f)
	{
		roofNormal *= -1.0f;
	}

	if (!UModumateRoofStatics::GetAllProperties(TargetMOI, EdgePoints, EdgeIDs, DefaultEdgeProperties, EdgeProperties))
	{
		return false;
	}

	if (!UModumateRoofStatics::TessellateSlopedEdges(EdgePoints, EdgeProperties, CombinedPolyVerts, PolyVertIndices, roofNormal, world))
	{
		return false;
	}

	int32 numEdges = EdgeIDs.Num();
	int32 numCalculatedPolys = PolyVertIndices.Num();
	if (numCalculatedPolys > 0)
	{
		const FVector *combinedPolyVertsPtr = CombinedPolyVerts.GetData();
		int32 vertIdxStart = 0, vertIdxEnd = 0;

		FModumateDocument &doc = GameState->Document;
		const FGraph3D &volumeGraph = doc.GetVolumeGraph();
		FGraph3D &tempVolumeGraph = doc.GetTempVolumeGraph();
		int32 nextID = doc.GetNextAvailableID();
		TSet<int32> groupIDs({ TargetMOI->ID });

		bool bFaceAdditionFailure = false;
		TArray<FGraph3DDelta> graphDeltas;

		for (int32 polyIdx = 0; polyIdx < numCalculatedPolys; ++polyIdx)
		{
			vertIdxEnd = PolyVertIndices[polyIdx];

			int32 numPolyVerts = (vertIdxEnd - vertIdxStart) + 1;
			TArray<FVector> polyVerts(combinedPolyVertsPtr + vertIdxStart, numPolyVerts);

			int32 existingFaceID;
			bool bAddedFace = tempVolumeGraph.GetDeltaForFaceAddition(polyVerts, graphDeltas, nextID, existingFaceID, groupIDs);
			if (!bAddedFace)
			{
				bFaceAdditionFailure = true;
				break;
			}

			vertIdxStart = vertIdxEnd + 1;
		}

		if (graphDeltas.Num() == 0)
		{
			bFaceAdditionFailure = true;
		}

		FGraph3D::CloneFromGraph(tempVolumeGraph, volumeGraph);

		if (bFaceAdditionFailure)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to generate graph deltas for all requested roof faces :("));
			EndUse();
			return false;
		}

		TArray<FDeltaPtr> deltaPtrs;
		for (const FGraph3DDelta &graphDelta : graphDeltas)
		{
			deltaPtrs.Add(MakeShared<FGraph3DDelta>(graphDelta));
		}

		bool bAppliedDeltas = doc.ApplyDeltas(deltaPtrs, world);

		if (!bAppliedDeltas)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to apply graph deltas for all requested roof faces :("));
		}
	}

	return false;
}

FVector ACreateRoofFacesHandle::GetHandlePosition() const
{
	return TargetMOI->GetObjectLocation();
}

bool ACreateRoofFacesHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->RoofCreateFacesStyle;
	OutWidgetSize = FVector2D(32.0f, 32.0f);
	return true;
}


bool ARetractRoofFacesHandle::BeginUse()
{
	if (!ensure(TargetMOI && GameState))
	{
		return false;
	}

	FModumateDocument &doc = GameState->Document;
	const FGraph3D &volumeGraph = doc.GetVolumeGraph();
	if (!volumeGraph.GetGroup(TargetMOI->ID, TempGroupMembers))
	{
		return false;
	}

	TempFaceIDs.Reset();
	for (int32 groupMember : TempGroupMembers)
	{
		if (auto face = volumeGraph.FindFace(groupMember))
		{
			TempFaceIDs.Add(face->ID);
		}
	}

	// TODO: migrate this to one or more deltas, when we can delete objects that way.
	doc.DeleteObjects(TempFaceIDs, true, true);

	return false;
}

FVector ARetractRoofFacesHandle::GetHandlePosition() const
{
	return TargetMOI->GetObjectLocation();
}

bool ARetractRoofFacesHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->RoofRetractFacesStyle;
	OutWidgetSize = FVector2D(32.0f, 32.0f);
	return true;
}


AEditRoofEdgeHandle::AEditRoofEdgeHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetEdgeID(MOD_ID_NONE)
{

}

bool AEditRoofEdgeHandle::BeginUse()
{
	// TODO: we should be able to rely on Super's OnBeginUse, but it currently sets preview state on the target MOI unconditionally
	bIsInUse = true;

	PropertiesWidget = PlayerHUD->GetOrCreateWidgetInstance<URoofPerimeterPropertiesWidget>(PlayerHUD->WidgetClasses->RoofPerimeterPropertiesClass);
	PropertiesWidget->SetTarget(TargetMOI->ID, TargetEdgeID, this);
	PropertiesWidget->AddToViewport();
	PropertiesWidget->SetPositionInViewport(FVector2D::ZeroVector);
	PropertiesWidget->UpdateTransform();

	return true;
}

bool AEditRoofEdgeHandle::UpdateUse()
{
	return (PropertiesWidget != nullptr);
}

void AEditRoofEdgeHandle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PropertiesWidget)
	{
		PropertiesWidget->UpdateTransform();
	}
}

void AEditRoofEdgeHandle::EndUse()
{
	// TODO: we should be able to rely on Super's EndUse, but it currently creates an FMOIDelta unconditionally
	AbortUse();
}

void AEditRoofEdgeHandle::AbortUse()
{
	bIsInUse = false;

	if (PropertiesWidget)
	{
		PropertiesWidget->RemoveFromViewport();
		PropertiesWidget->ConditionalBeginDestroy();
	}
}

FVector AEditRoofEdgeHandle::GetHandlePosition() const
{
	const FModumateDocument *doc = TargetMOI ? TargetMOI->GetDocument() : nullptr;
	const FModumateObjectInstance *targetEdgeMOI = doc ? doc->GetObjectById(FMath::Abs(TargetEdgeID)) : nullptr;
	if (ensure(targetEdgeMOI))
	{
		return targetEdgeMOI->GetObjectLocation();
	}

	return FVector::ZeroVector;
}

void AEditRoofEdgeHandle::SetTargetEdge(FGraphSignedID InTargetEdgeID)
{
	TargetEdgeID = InTargetEdgeID;
}

bool AEditRoofEdgeHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->RoofEditEdgeStyle;
	OutWidgetSize = FVector2D(32.0f, 32.0f);
	return true;
}
