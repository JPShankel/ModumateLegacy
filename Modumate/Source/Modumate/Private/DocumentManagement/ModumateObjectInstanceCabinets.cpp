// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceCabinets.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "DrawDebugHelpers.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"

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
		// TODO: revisit the handle paradigm for cabinets
		MOI->ShowAdjustmentHandles(controller, AdjustmentHandlesVisible);

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
		int32 numCP = MOI->GetControlPoints().Num();
		for (int32 i = 0; i < numCP; ++i)
		{
			auto pointHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			pointHandle->SetTargetIndex(i);

			auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			edgeHandle->SetTargetIndex(i);
			edgeHandle->SetAdjustPolyEdge(true);

			auto selectFrontHandle = MOI->MakeHandle<ASelectCabinetFrontHandle>();
			selectFrontHandle->SetTargetIndex(i);

			FrontSelectionHandles.Add(selectFrontHandle);
		}

		auto topExtrusionHandle = MOI->MakeHandle<AAdjustPolyExtrusionHandle>();
		topExtrusionHandle->SetSign(1.0f);

		auto bottomExtrusionHandle = MOI->MakeHandle<AAdjustPolyExtrusionHandle>();
		bottomExtrusionHandle->SetSign(-1.0f);
	}

	void FMOICabinetImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow)
	{
		FDynamicModumateObjectInstanceImpl::ShowAdjustmentHandles(Controller, bShow);

		AdjustmentHandlesVisible = bShow;

		// make sure that front selection handles are only visible if they are valid choices
		for (auto frontHandle : FrontSelectionHandles)
		{
			if (frontHandle.IsValid())
			{
				bool bHandleEnabled = bShow && ((MOI->GetControlPointIndices().Num() == 0) || (MOI->GetControlPointIndex(0) == frontHandle->TargetIndex));
				frontHandle->SetEnabled(bHandleEnabled);
			}
		}
	}
}

using namespace Modumate;

const float ASelectCabinetFrontHandle::FaceCenterHeightOffset = 20.0f;

bool ASelectCabinetFrontHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	TArray<int32> newControlIndices;

	if ((TargetMOI->GetControlPointIndices().Num() == 0) || (TargetMOI->GetControlPointIndex(0) != TargetIndex))
	{
		newControlIndices.Add(TargetIndex);
	}

	TargetMOI->SetControlPointIndices(newControlIndices);
	TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta({ TargetMOI }));

	EndUse();

	Controller->ModumateCommand(delta->AsCommand());
	return false;
}

FVector ASelectCabinetFrontHandle::GetHandlePosition() const
{
	const auto &points = TargetMOI->GetControlPoints();
	FVector edgeCenter = 0.5f * (points[TargetIndex] + points[(TargetIndex + 1) % points.Num()]);
	FVector heightOffset = FVector(0, 0, 0.5f * TargetMOI->GetExtents().Y + FaceCenterHeightOffset);
	return TargetMOI->GetObjectRotation().RotateVector(heightOffset + edgeCenter);
}

bool ASelectCabinetFrontHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->CabinetFrontFaceStyle;
	return true;
}
