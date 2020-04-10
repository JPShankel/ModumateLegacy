// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceCabinets.h"

#include "AdjustmentHandleActor_CPP.h"
#include "DrawDebugHelpers.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPolyAdjustmentHandles.h"
#include "HUDDrawWidget_CPP.h"
#include "ModumateCommands.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"

namespace Modumate
{
	using namespace Units;

	FName FMOICabinetImpl::CabinetGeometryMatName(TEXT("Cabinet_Exterior_Finish"));

	FMOICabinetImpl::FMOICabinetImpl(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi)
		, AdjustmentHandlesVisible(false)
		, ToeKickDimensions(ForceInitToZero)
	{
	}

	FMOICabinetImpl::~FMOICabinetImpl()
	{
	}

	FVector FMOICabinetImpl::GetCorner(int32 index) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		float thickness = MOI->GetExtents().Y;
		FVector extrusionDir = FVector::UpVector;

		FPlane plane;
		UModumateGeometryStatics::GetPlaneFromPoints(MOI->GetControlPoints(), plane);
		// For now, make sure that the plane of the MOI is horizontal
		if (ensureAlways((numCP >= 3) && FVector::Parallel(extrusionDir, plane)))

		{
			FVector corner = MOI->GetControlPoint(index % numCP);

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

	FVector FMOICabinetImpl::GetNormal() const
	{
		return FVector::UpVector;
	}

	void FMOICabinetImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		FModumateObjectInstanceImplBase::UpdateVisibilityAndCollision(bOutVisible, bOutCollisionEnabled);

		if (FrontFacePortalActor.IsValid())
		{
			FrontFacePortalActor->SetActorHiddenInGame(!bOutVisible);
			FrontFacePortalActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	void FMOICabinetImpl::SetupDynamicGeometry()
	{
		UpdateToeKickDimensions();

		int32 frontFaceIndex = (MOI->GetControlPointIndices().Num() > 0) ? MOI->GetControlPointIndex(0) : INDEX_NONE;
		FArchitecturalMaterial materialData = MOI->GetAssembly().PortalConfiguration.MaterialsPerChannel.FindRef(CabinetGeometryMatName);

		DynamicMeshActor->SetupCabinetGeometry(MOI->GetControlPoints(), MOI->GetExtents().Y,
			materialData, ToeKickDimensions, frontFaceIndex);

		// refresh handle visibility, don't destroy & recreate handles
		AEditModelPlayerController_CPP *controller = DynamicMeshActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		ShowAdjustmentHandles(controller, AdjustmentHandlesVisible);

		UpdateCabinetPortal();
	}

	void FMOICabinetImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOICabinetImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		FDynamicModumateObjectInstanceImpl::GetStructuralPointsAndLines(outPoints, outLines, bForSnapping, bForSelection);

		int32 frontFaceIndex = (MOI->GetControlPointIndices().Num() > 0) ? MOI->GetControlPointIndex(0) : INDEX_NONE;
		if ((ToeKickDimensions.X > 0.0f) && (frontFaceIndex != INDEX_NONE))
		{
			int32 numCP = MOI->GetControlPoints().Num();
			int32 frontFaceOtherIndex = (frontFaceIndex + 1) % numCP;

			FVector frontFaceInNormal;
			GetTriInternalNormalFromEdge(frontFaceIndex, frontFaceOtherIndex, frontFaceInNormal);

			FVector toeKickP1 = MOI->GetControlPoint(frontFaceIndex) + (ToeKickDimensions.X * frontFaceInNormal);
			FVector toeKickP2 = MOI->GetControlPoint(frontFaceOtherIndex) + (ToeKickDimensions.X * frontFaceInNormal);
			FVector dir = (toeKickP2 - toeKickP1).GetSafeNormal();

			outPoints.Add(FStructurePoint(toeKickP1, dir, frontFaceIndex));
			outPoints.Add(FStructurePoint(toeKickP2, dir, frontFaceOtherIndex));

			outLines.Add(FStructureLine(toeKickP1, toeKickP2, frontFaceIndex, frontFaceOtherIndex));
		}
	}

	void FMOICabinetImpl::UpdateToeKickDimensions()
	{
		ToeKickDimensions.Set(0.0f, 0.0f);

		if (ensureAlways(MOI))
		{
			UModumateFunctionLibrary::GetCabinetToeKickDimensions(MOI->GetAssembly(), ToeKickDimensions);
		}
	}

	void FMOICabinetImpl::UpdateCabinetPortal()
	{
		int32 frontFaceIndex = (MOI->GetControlPointIndices().Num() > 0) ? MOI->GetControlPointIndex(0) : INDEX_NONE;
		if ((frontFaceIndex < 0) && FrontFacePortalActor.IsValid())
		{
			FrontFacePortalActor->Destroy();
		}
		else if ((frontFaceIndex >= 0) && !FrontFacePortalActor.IsValid())
		{
			FrontFacePortalActor = DynamicMeshActor->GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass());
			FrontFacePortalActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		}

		if (!FrontFacePortalActor.IsValid())
		{
			return;
		}

		if (!ensureAlways(MOI && MOI->GetAssembly().PortalConfiguration.IsValid()))
		{
			return;
		}

		// Get enough geometric information to set up the portal assembly
		FVector extrusionDir = FVector::UpVector;
		FPlane plane;
		if (!UModumateGeometryStatics::GetPlaneFromPoints(MOI->GetControlPoints(), plane))
		{
			return;
		}
		if (!FVector::Parallel(plane, extrusionDir))
		{
			return;
		}
		bool bCoincident = FVector::Coincident(FVector(plane), extrusionDir);

		const FVector &p1 = MOI->GetControlPoint(frontFaceIndex);
		const FVector &p2 = MOI->GetControlPoint((frontFaceIndex + 1) % MOI->GetControlPoints().Num());
		FVector edgeDelta = p2 - p1;
		float edgeLength = edgeDelta.Size();
		if (!ensureAlways(!FMath::IsNearlyZero(edgeLength)))
		{
			return;
		}

		FVector edgeDir = edgeDelta / edgeLength;

		FVector faceNormal = (edgeDir ^ extrusionDir) * (bCoincident ? 1.0f : -1.0f);

		float cabinetHeight = MOI->GetExtents().Y;
		FVector toeKickOffset = ToeKickDimensions.Y * extrusionDir;

		// Update the reference planes so the portal 9-slicing is correct
		FModumateObjectAssembly assembly = MOI->GetAssembly();
		TMap<FName, FPortalReferencePlane> &refPlanes = assembly.PortalConfiguration.ReferencePlanes;
		refPlanes[FPortalConfiguration::RefPlaneNameMinX].FixedValue = FUnitValue::WorldCentimeters(0.0f);
		refPlanes[FPortalConfiguration::RefPlaneNameMaxX].FixedValue = FUnitValue::WorldCentimeters(edgeLength);
		refPlanes[FPortalConfiguration::RefPlaneNameMinZ].FixedValue = FUnitValue::WorldCentimeters(0.0f);
		refPlanes[FPortalConfiguration::RefPlaneNameMaxZ].FixedValue = FUnitValue::WorldCentimeters(cabinetHeight - ToeKickDimensions.Y);
		assembly.PortalConfiguration.CacheRefPlaneValues();
		MOI->SetAssembly(assembly);

		FrontFacePortalActor->MakeFromAssembly(MOI->GetAssembly(), FVector::OneVector, MOI->GetObjectInverted(), true);

		// Now position the portal where it's supposed to go
		FVector portalOrigin = toeKickOffset + (bCoincident ? p2 : p1);
		FQuat portalRot = FRotationMatrix::MakeFromYZ(faceNormal, extrusionDir).ToQuat();

		FrontFacePortalActor->SetActorLocationAndRotation(portalOrigin, portalRot);
	}


	void FMOICabinetImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}

		auto makeActor = [this, controller](IAdjustmentHandleImpl *impl, UStaticMesh *mesh, const FVector &s, const TArray<int32>& CP, float offsetDist, float planeSign)
		{
			AAdjustmentHandleActor_CPP *actor = DynamicMeshActor->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
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

		for (int32 i = 0; i < MOI->GetControlPoints().Num(); ++i)
		{
			makeActor(new FAdjustPolyPointHandle(MOI, i, (i + 1) % MOI->GetControlPoints().Num()), AEditModelGameMode_CPP::FaceAdjusterMesh, FVector(0.0015f), { i, (i + 1) % MOI->GetControlPoints().Num() }, 16.0f, 0.0f);

			auto *frontHandleImpl = new FSelectFrontAdjustmentHandle(MOI, i);
			makeActor(frontHandleImpl, AEditModelGameMode_CPP::AnchorMesh, FVector(0.001f), { i, (i + 1) % MOI->GetControlPoints().Num() }, 16.0f, 0.0f);
			FrontSelectionHandleImpls.Add(frontHandleImpl);
		}

		makeActor(new FAdjustPolyExtrusionHandle(MOI, 1.0f), AEditModelGameMode_CPP::FaceAdjusterMesh, FVector(0.0015f), {}, 0.0f, 1.0f);
		makeActor(new FAdjustPolyExtrusionHandle(MOI, -1.0f), AEditModelGameMode_CPP::FaceAdjusterMesh, FVector(0.0015f), {}, 0.0f, -1.0f);

		ShowAdjustmentHandles(controller, true);
	}

	void FMOICabinetImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		FrontSelectionHandleImpls.Empty();
		AdjustmentHandlesVisible = false;

		FDynamicModumateObjectInstanceImpl::ClearAdjustmentHandles(controller);
	}

	void FMOICabinetImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		FDynamicModumateObjectInstanceImpl::ShowAdjustmentHandles(controller, show);

		AdjustmentHandlesVisible = show;

		// make sure that front selection handles are only visible if they are valid choices
		for (auto *frontHandleImpl : FrontSelectionHandleImpls)
		{
			if (frontHandleImpl && frontHandleImpl->Handle.IsValid())
			{
				bool frontSelectHandleEnabled = show && ((MOI->GetControlPointIndices().Num() == 0) || (MOI->GetControlPointIndex(0) == frontHandleImpl->CP));

				frontHandleImpl->Handle->GetStaticMeshComponent()->SetVisibility(frontSelectHandleEnabled);
				frontHandleImpl->Handle->GetStaticMeshComponent()->SetCollisionEnabled(frontSelectHandleEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			}
		}
	}



	const float FSelectFrontAdjustmentHandle::FaceCenterHeightOffset = 20.0f;

	FSelectFrontAdjustmentHandle::FSelectFrontAdjustmentHandle(FModumateObjectInstance *moi, int32 cp)
		: FEditModelAdjustmentHandleBase(moi)
		, CP(cp)
	{ }

	bool FSelectFrontAdjustmentHandle::OnBeginUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		TArray<int32> newControlIndices;

		if ((MOI->GetControlPointIndices().Num() == 0) || (MOI->GetControlPointIndex(0) != CP))
		{
			newControlIndices.Add(CP);
		}

		Controller->ModumateCommand
		(
			FModumateCommand(Modumate::Commands::kSetControlIndices)
			.Param("id", MOI->ID)
			.Param("indices", newControlIndices)
		);

		OnEndUse();

		return false;
	}

	FVector FSelectFrontAdjustmentHandle::GetAttachmentPoint()
	{
		const auto &points = MOI->GetControlPoints();
		FVector edgeCenter = 0.5f * (points[CP] + points[(CP + 1) % points.Num()]);
		FVector heightOffset = FVector(0, 0, 0.5f * MOI->GetExtents().Y + FaceCenterHeightOffset);
		return MOI->GetObjectRotation().RotateVector(heightOffset + edgeCenter);
	}
}
