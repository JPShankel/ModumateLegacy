// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceScopeBox.h"

#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"
#include "UnrealClasses/HUDDrawWidget_CPP.h"

namespace Modumate
{
	FMOIScopeBoxImpl::FMOIScopeBoxImpl(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi),
		EdgeSelectedColor(255.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f),
		EdgeColor(255.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f),
		HandleScale(0.0015f),
		PolyPointHandleOffset(16.0f),
		ExtrusionHandleOffset(0.0f)
	{

	}

	void FMOIScopeBoxImpl::SetupDynamicGeometry()
	{
		AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->ScopeBoxMaterial : nullptr;

		float thickness = MOI->GetExtents().Y;
		DynamicMeshActor->SetupPrismGeometry(MOI->GetControlPoints(), thickness, MaterialData);

		MOI->UpdateVisibilityAndCollision();

		FPlane outPlane;
		UModumateGeometryStatics::GetPlaneFromPoints(MOI->GetControlPoints(), outPlane);

		Normal = FVector(outPlane);
	}

	void FMOIScopeBoxImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	FVector FMOIScopeBoxImpl::GetCorner(int32 index) const
	{
		int32 numCP = MOI->GetControlPoints().Num();

		if (ensureAlways(numCP == 4) && index < numCP * 2)
		{
			FVector corner = MOI->GetControlPoint(index % numCP);

			if (index >= numCP)
			{
				corner += Normal * MOI->GetExtents().Y;
			}

			return corner;
		}

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

		int32 numPolyPoints = MOI->GetControlPoints().Num();
		FVector offset = MOI->GetExtents().Y * Normal;
 
		for (int32 i = 0; i < numPolyPoints; ++i)
		{
			int32 nextI = (i + 1) % numPolyPoints;
			const FVector &cp1 = MOI->GetControlPoint(i);
			const FVector &cp2 = MOI->GetControlPoint(nextI);
			FVector dir = (cp2 - cp1).GetSafeNormal();

			const FVector &cp1n = cp1 + offset;
			const FVector &cp2n = cp2 + offset;
			outPoints.Add(FStructurePoint(cp1, dir, i));
			outPoints.Add(FStructurePoint(cp1n, dir, i));

			outLines.Add(FStructureLine(cp1, cp2, i, nextI));
			outLines.Add(FStructureLine(cp1n, cp2n, i + numPolyPoints, nextI + numPolyPoints));
			outLines.Add(FStructureLine(cp1, cp1n, i + numPolyPoints * 2, nextI + numPolyPoints * 2));
		}
	}

	void FMOIScopeBoxImpl::AddDraftingLines(UHUDDrawWidget_CPP *HUDDrawWidget)
	{
		TArray<Modumate::FStructurePoint> ScopeBoxStructurePoints;
		TArray<Modumate::FStructureLine> ScopeBoxStructureLines;
		GetStructuralPointsAndLines(ScopeBoxStructurePoints, ScopeBoxStructureLines, false, true);

		for (auto &structureLine : ScopeBoxStructureLines)
		{
			FModumateLines objectSelectionLine = FModumateLines();
			objectSelectionLine.Point1 = structureLine.P1;
			objectSelectionLine.Point2 = structureLine.P2;
			objectSelectionLine.OwnerActor = MOI->GetActor();
			objectSelectionLine.Thickness = 2.0f;
			objectSelectionLine.Color = MOI->IsSelected() ? EdgeSelectedColor : EdgeColor;

			HUDDrawWidget->LinesToDraw.Add(MoveTemp(objectSelectionLine));
		}

	}

	bool FMOIScopeBoxImpl::ShowStructureOnSelection() const
	{
		return false;
	}

	void FMOIScopeBoxImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}

		int32 numCP = MOI->GetControlPoints().Num();
		if (!ensureAlways(numCP == 4))
		{
			return;
		}

		UStaticMesh *faceAdjusterMesh = controller->EMPlayerState->GetEditModelGameMode()->FaceAdjusterMesh;

		for (int32 idx = 0; idx < numCP; idx++)
		{
			TArray<int32> cornerIdxs = { idx, (idx + 1) % numCP };
			FAdjustPolyPointHandle *handle = new FAdjustPolyPointHandle(MOI, cornerIdxs[0], cornerIdxs[1]);

			AAdjustmentHandleActor_CPP *actor = DynamicMeshActor->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(faceAdjusterMesh);
			actor->SetHandleScale(HandleScale);
			actor->SetHandleScaleScreenSize(HandleScale);

			actor->SetPolyHandleSide({ cornerIdxs[0], cornerIdxs[1] }, MOI, PolyPointHandleOffset);

			handle->Handle = actor;
			actor->Implementation = handle;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
			AdjustmentHandles.Add(actor);
		}

		for (float sign : { 1.0f, -1.0f })
		{
			FAdjustPolyExtrusionHandle *handle = new FAdjustPolyExtrusionHandle(MOI, sign);

			AAdjustmentHandleActor_CPP *actor = DynamicMeshActor->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(faceAdjusterMesh);
			actor->SetHandleScale(HandleScale);
			actor->SetHandleScaleScreenSize(HandleScale);

			actor->SetOverrideHandleDirection(sign * Normal, MOI, ExtrusionHandleOffset);

			handle->Handle = actor;
			actor->Implementation = handle;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
			AdjustmentHandles.Add(actor);
		}

		ShowAdjustmentHandles(controller, true);
	}
}
