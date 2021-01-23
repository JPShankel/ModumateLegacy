// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/ScopeBox.h"

#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UI/HUDDrawWidget.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

AMOIScopeBox::AMOIScopeBox()
	: AModumateObjectInstance()
	, EdgeSelectedColor(255.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f)
	, EdgeColor(255.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f)
	, HandleScale(0.0015f)
	, PolyPointHandleOffset(16.0f)
	, ExtrusionHandleOffset(0.0f)
{

}

AActor* AMOIScopeBox::CreateActor(const FVector &loc, const FQuat &rot)
{
	AActor* returnActor = Super::CreateActor(loc, rot);
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	auto playerState = controller ? controller->EMPlayerState : nullptr;
	if (playerState)
	{
		playerState->OnUpdateScopeBoxes.Broadcast();
	}
	return returnActor;
}

void AMOIScopeBox::PreDestroy()
{
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	auto playerState = controller ? controller->EMPlayerState : nullptr;
	if (playerState)
	{
		playerState->OnUpdateScopeBoxes.Broadcast();
	}
}

void AMOIScopeBox::SetupDynamicGeometry()
{
	// TODO: generate plane points and extrusion from extents and transform, rather than old-school control points
#if 0
	AEditModelGameMode *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
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

void AMOIScopeBox::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

FVector AMOIScopeBox::GetCorner(int32 index) const
{
	// TODO: derive from extents and transform, rather than old-school control points
	return GetLocation();
}

FVector AMOIScopeBox::GetNormal() const
{
	return Normal;
}

void AMOIScopeBox::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
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

void AMOIScopeBox::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
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

bool AMOIScopeBox::ShowStructureOnSelection() const
{
	return false;
}

void AMOIScopeBox::SetupAdjustmentHandles(AEditModelPlayerController *controller)
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
