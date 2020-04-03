// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceRoof.h"

#include "AdjustmentHandleActor_CPP.h"
#include "DrawDebugHelpers.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPolyAdjustmentHandles.h"
#include "HUDDrawWidget_CPP.h"
#include "ModumateCommands.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"
#include "ModumateObjectStatics.h"

namespace Modumate
{
	using namespace Units;


	FMOIRoofImpl::FMOIRoofImpl(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi)
		, bAdjustmentHandlesVisible(false)
	{
	}

	FMOIRoofImpl::~FMOIRoofImpl()
	{
	}

	FVector FMOIRoofImpl::GetCorner(int32 index) const
	{
		int32 numCP = MOI->ControlPoints.Num();
		float thickness = MOI->Extents.Y;
		FVector extrusionDir = FVector::UpVector;

		FPlane plane;
		UModumateGeometryStatics::GetPlaneFromPoints(MOI->ControlPoints, plane);

		// For now, make sure that the plane of the MOI is horizontal
		if (ensureAlways((numCP >= 3) && FVector::Parallel(extrusionDir, plane)))
		{
			FVector corner = MOI->ControlPoints[index % numCP];

			if (index >= numCP)
			{
				corner += thickness * extrusionDir;
			}

			return corner;
		}
		else
		{
			return GetLocation();
		}
	}

	void FMOIRoofImpl::SetupDynamicGeometry()
	{
		GotGeometry = true;

		if (UModumateObjectStatics::GetRoofGeometryValues(MOI->ControlPoints, MOI->ControlIndices, EdgePoints, EdgeSlopes, EdgesHaveFaces))
		{
			TArray<FVector> CombinedPolyVerts;
			TArray<int32> PolyVertIndices;
			if (UModumateGeometryStatics::TessellateSlopedEdges(EdgePoints, EdgeSlopes, EdgesHaveFaces, CombinedPolyVerts, PolyVertIndices))
			{
				DynamicMeshActor->SetupRoofGeometry(MOI->ObjectAssembly, CombinedPolyVerts, PolyVertIndices, EdgesHaveFaces);
			}
		}

		// refresh handle visibility, don't destroy & recreate handles
		AEditModelPlayerController_CPP *controller = DynamicMeshActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		ShowAdjustmentHandles(controller, bAdjustmentHandlesVisible);
	}

	void FMOIRoofImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIRoofImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}

		auto makeActor = [this, controller](IAdjustmentHandleImpl *impl, UStaticMesh *mesh, const FVector &s, const TArray<int32>& CP, float offsetDist, float planeSign)
		{
			if (!DynamicMeshActor.IsValid() || !World.IsValid())
			{
				return;
			}

			AAdjustmentHandleActor_CPP *actor = World->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(mesh);
			actor->SetHandleScale(s);
			actor->SetHandleScaleScreenSize(s);
			if (CP.Num() > 0)
			{
				actor->SetPolyHandleSide(CP, MOI, offsetDist);
			}
			else if (planeSign != 0.0f)
			{
				actor->SetOverrideHandleDirection(planeSign * FVector::UpVector, MOI, offsetDist);
			}

			impl->Handle = actor;
			actor->Implementation = impl;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
			AdjustmentHandles.Add(actor);
		};

		int32 numEdges = EdgePoints.Num();
		for (int32 i = 0; i < numEdges; ++i)
		{
			int32 nextI = (i + 1) % numEdges;
			// TODO: fix face handles after giving them support for roof edge points, and allowing roof geometry to be edited
			//makeActor(new FAdjustPolyPointHandle(MOI, i, nextI), AEditModelGameMode_CPP::FaceAdjusterMesh, FVector(0.0015f), { i, nextI }, 16.0f, 0.0f);

			auto *frontHandleImpl = new FSetSegmentSlopeHandle(MOI, i);
			makeActor(frontHandleImpl, AEditModelGameMode_CPP::AnchorMesh, FVector(0.001f), { i, nextI }, 16.0f, 0.0f);
			SetSlopeHandleImpls.Add(frontHandleImpl);
		}

		ShowAdjustmentHandles(controller, true);
	}

	void FMOIRoofImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numPolyPoints = EdgePoints.Num();

		for (int32 i = 0; i < numPolyPoints; ++i)
		{
			int32 nextI = (i + 1) % numPolyPoints;
			const FVector &edgeP1 = EdgePoints[i];
			const FVector &edgeP2 = EdgePoints[nextI];
			FVector dir = (edgeP2 - edgeP1).GetSafeNormal();

			outPoints.Add(FStructurePoint(edgeP1, dir, i));

			outLines.Add(FStructureLine(edgeP1, edgeP2, i, nextI));
		}
	}


	const float FSetSegmentSlopeHandle::FaceCenterHeightOffset = 20.0f;

	FSetSegmentSlopeHandle::FSetSegmentSlopeHandle(FModumateObjectInstance *moi, int32 cp)
		: FEditModelAdjustmentHandleBase(moi)
		, CP(cp)
	{ }

	bool FSetSegmentSlopeHandle::OnBeginUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		if ((CP >= 0) && (CP < EdgeSlopes.Num()))
		{
			UModumateObjectStatics::GetRoofGeometryValues(MOI->ControlPoints, MOI->ControlIndices, EdgePoints, EdgeSlopes, EdgesHaveFaces);
			EdgesHaveFaces[CP] = !EdgesHaveFaces[CP];

			TArray<FVector> newCPs;
			TArray<int32> newCIs;
			UModumateObjectStatics::GetRoofControlValues(EdgePoints, EdgeSlopes, EdgesHaveFaces, newCPs, newCIs);

			Controller->ModumateCommand(
				FModumateCommand(Commands::kUpdateControlValues)
				.Param(Parameters::kObjectID, MOI->ID)
				.Param(Parameters::kControlPoints, newCPs)
				.Param(Parameters::kIndices, newCIs)
			);
		}

		OnEndUse();

		MOI->ShowAdjustmentHandles(Controller.Get(), true);

		return false;
	}

	FVector FSetSegmentSlopeHandle::GetAttachmentPoint()
	{
		if (UModumateObjectStatics::GetRoofGeometryValues(MOI->ControlPoints, MOI->ControlIndices, EdgePoints, EdgeSlopes, EdgesHaveFaces))
		{
			int32 numPoints = EdgePoints.Num();
			FVector edgeCenter = 0.5f * (EdgePoints[CP] + EdgePoints[(CP + 1) % EdgePoints.Num()]);
			FVector heightOffset = FVector(0, 0, 0.5f * MOI->Extents.Y + FaceCenterHeightOffset);
			return MOI->GetObjectRotation().RotateVector(heightOffset + edgeCenter);
		}

		return FVector::ZeroVector;
	}
}
