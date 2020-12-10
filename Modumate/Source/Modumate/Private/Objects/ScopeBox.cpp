// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/ScopeBox.h"

#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UI/HUDDrawWidget.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

FMOIScopeBoxImpl::FMOIScopeBoxImpl()
	: FModumateObjectInstance()
	, EdgeSelectedColor(255.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f)
	, EdgeColor(255.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f)
	, HandleScale(0.0015f)
	, PolyPointHandleOffset(16.0f)
	, ExtrusionHandleOffset(0.0f)
{

}

AActor* FMOIScopeBoxImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	AActor* returnActor = FModumateObjectInstance::CreateActor(world, loc, rot);
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	auto playerState = controller ? controller->EMPlayerState : nullptr;
	if (playerState)
	{
		playerState->OnUpdateScopeBoxes.Broadcast();
	}
	return returnActor;
}

void FMOIScopeBoxImpl::PreDestroy()
{
	auto controller = World.IsValid() ? World->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	auto playerState = controller ? controller->EMPlayerState : nullptr;
	if (playerState)
	{
		playerState->OnUpdateScopeBoxes.Broadcast();
	}
}

void FMOIScopeBoxImpl::SetupDynamicGeometry()
{
	// TODO: generate plane points and extrusion from extents and transform, rather than old-school control points
#if 0
	AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	MaterialData.EngineMaterial = gameMode ? gameMode->ScopeBoxMaterial : nullptr;

	float thickness = GetExtents().Y;
	FVector extrusionDelta = thickness * FVector::UpVector;
	DynamicMeshActor->SetupPrismGeometry(GetControlPoints(), extrusionDelta, MaterialData, true, true);

	UpdateVisualFlags();

	FPlane outPlane;
	UModumateGeometryStatics::GetPlaneFromPoints(GetControlPoints(), outPlane);

	Normal = FVector(outPlane);
#endif
}

void FMOIScopeBoxImpl::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

FVector FMOIScopeBoxImpl::GetCorner(int32 index) const
{
	// TODO: derive from extents and transform, rather than old-school control points
	return GetLocation();
}

FVector FMOIScopeBoxImpl::GetNormal() const
{
	return Normal;
}

void FMOIScopeBoxImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	// Don't return points or lines if we're snapping,
	// since otherwise the plane will interfere with edges and vertices.
	if (bForSnapping)
	{
		return;
	}

	// TODO: derive from extents and transform, rather than old-school control points
#if 0
	int32 numPolyPoints = GetControlPoints().Num();
	FVector offset = GetExtents().Y * Normal;
 
	for (int32 i = 0; i < numPolyPoints; ++i)
	{
		int32 nextI = (i + 1) % numPolyPoints;
		const FVector &cp1 = GetControlPoint(i);
		const FVector &cp2 = GetControlPoint(nextI);
		FVector dir = (cp2 - cp1).GetSafeNormal();

		const FVector &cp1n = cp1 + offset;
		const FVector &cp2n = cp2 + offset;
		outPoints.Add(FStructurePoint(cp1, dir, i));
		outPoints.Add(FStructurePoint(cp1n, dir, i));

		outLines.Add(FStructureLine(cp1, cp2, i, nextI));
		outLines.Add(FStructureLine(cp1n, cp2n, i + numPolyPoints, nextI + numPolyPoints));
		outLines.Add(FStructureLine(cp1, cp1n, i + numPolyPoints * 2, nextI + numPolyPoints * 2));
	}
#endif
}

void FMOIScopeBoxImpl::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
{
	TArray<FStructurePoint> ScopeBoxStructurePoints;
	TArray<FStructureLine> ScopeBoxStructureLines;
	GetStructuralPointsAndLines(ScopeBoxStructurePoints, ScopeBoxStructureLines, false, true);

	for (auto &structureLine : ScopeBoxStructureLines)
	{
		FModumateLines objectSelectionLine = FModumateLines();
		objectSelectionLine.Point1 = structureLine.P1;
		objectSelectionLine.Point2 = structureLine.P2;
		objectSelectionLine.OwnerActor = GetActor();
		objectSelectionLine.Thickness = 2.0f;
		objectSelectionLine.Color = IsSelected() ? EdgeSelectedColor : EdgeColor;

		HUDDrawWidget->LinesToDraw.Add(MoveTemp(objectSelectionLine));
	}

}

bool FMOIScopeBoxImpl::ShowStructureOnSelection() const
{
	return false;
}

void FMOIScopeBoxImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	for (int32 i = 0; i < 4; ++i)
	{
		auto pointHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		pointHandle->SetTargetIndex(i);

		auto edgeHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
	}

	// TODO: rewrite extrusion handles specifically for scope boxes
}
