// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstancePortal.h"

#include "AdjustmentHandleActor_CPP.h"
#include "CompoundMeshActor.h"
#include "EditModelAdjustmentHandleBase.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPortalAdjustmentHandles.h"
#include "ExpressionEvaluator.h"
#include "HUDDrawWidget_CPP.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateDraftingElements.h"
#include "ModumateObjectInstanceParts_CPP.h"
#include "ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "WidgetBlueprintLibrary.h"


class AEditModelPlayerController_CPP;

namespace Modumate
{

	FMOIPortalImpl::FMOIPortalImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, Controller(nullptr)
		, CachedRelativePos(ForceInitToZero)
		, CachedWorldPos(ForceInitToZero)
		, CachedRelativeRot(ForceInit)
		, CachedWorldRot(ForceInit)
		, bHaveValidTransform(false)
	{
		SetControlPointsFromAssembly();
	}

	void FMOIPortalImpl::GetControlPointsFromAssembly(const FModumateObjectAssembly &ObjectAssembly, TArray<FVector> &ControlPoints)
	{
		auto &portalConfig = ObjectAssembly.PortalConfiguration;
		if (ensureAlways(portalConfig.IsValid()))
		{
			ControlPoints.SetNum(4);
			float conceptualWidth = 0.0f, conceptualHeight = 0.0f;

			const FPortalReferencePlane &minXPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMinX];
			const FPortalReferencePlane &maxXPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxX];
			const FPortalReferencePlane &minZPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMinZ];
			const FPortalReferencePlane &maxZPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxZ];

			float minXValue = minXPlane.FixedValue.AsWorldCentimeters();
			float maxXValue = maxXPlane.FixedValue.AsWorldCentimeters();
			float minZValue = minZPlane.FixedValue.AsWorldCentimeters();
			float maxZValue = maxZPlane.FixedValue.AsWorldCentimeters();

			ControlPoints[0].Set(minXValue, 0.0f, minZValue);
			ControlPoints[1].Set(minXValue, 0.0f, maxZValue);
			ControlPoints[2].Set(maxXValue, 0.0f, maxZValue);
			ControlPoints[3].Set(maxXValue, 0.0f, minZValue);
		}
	}

	void FMOIPortalImpl::SetControlPointsFromAssembly()
	{
		TArray<FVector> controlPoints;
		GetControlPointsFromAssembly(MOI->GetAssembly(), controlPoints);
		MOI->SetControlPoints(controlPoints);
	}

	void FMOIPortalImpl::UpdateAssemblyFromControlPoints()
	{
		if (MOI->GetControlPoints().Num() < 4)
		{
			return;
		}

		if (MOI->GetAssembly().PortalConfiguration.IsValid())
		{
			FModumateObjectAssembly assembly = MOI->GetAssembly();
			auto &portalConfig = assembly.PortalConfiguration;

			FPortalReferencePlane &minXPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMinX];
			FPortalReferencePlane &maxXPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxX];
			FPortalReferencePlane &minZPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMinZ];
			FPortalReferencePlane &maxZPlane = portalConfig.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxZ];

			minXPlane.FixedValue = Units::FUnitValue::WorldCentimeters(MOI->GetControlPoint(0).X);
			maxXPlane.FixedValue = Units::FUnitValue::WorldCentimeters(MOI->GetControlPoint(2).X);
			minZPlane.FixedValue = Units::FUnitValue::WorldCentimeters(MOI->GetControlPoint(0).Z);
			maxZPlane.FixedValue = Units::FUnitValue::WorldCentimeters(MOI->GetControlPoint(2).Z);

			portalConfig.CacheRefPlaneValues();
			MOI->SetAssembly(assembly);
		}
	}

	FMOIPortalImpl::~FMOIPortalImpl()
	{}

	AActor *FMOIPortalImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		return world->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
	}

	void FMOIPortalImpl::OnAssemblyChanged()
	{
		SetControlPointsFromAssembly();
		SetupCompoundActor();
		CacheCorners();
	}

	FVector FMOIPortalImpl::GetNormal() const
	{
		AActor *portalActor = MOI ? MOI->GetActor() : nullptr;
		if (!ensure(portalActor != nullptr))
		{
			return FVector::ZeroVector;
		}

		FTransform portalTransform = portalActor->GetActorTransform();
		return portalTransform.GetUnitAxis(EAxis::Y);
	}

	bool FMOIPortalImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag))
		{
			return false;
		}

		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			auto *parentObj = MOI ? MOI->GetParentObject() : nullptr;
			if (parentObj)
			{
				parentObj->MarkDirty(EObjectDirtyFlags::Visuals);
			}
			else
			{
				return false;
			}
		}
		break;
		}

		return true;
	}

	void FMOIPortalImpl::SetupDynamicGeometry()
	{
		// If the MOI's control points got reset, then we need to recreate them here before they're used.
		if (MOI->GetControlPoints().Num() == 0)
		{
			SetControlPointsFromAssembly();
		}

		UpdateAssemblyFromControlPoints();
		SetupCompoundActor();
		SetRelativeTransform(CachedRelativePos, CachedRelativeRot);
	}

	void FMOIPortalImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIPortalImpl::SetupCompoundActor()
	{
		if (ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor()))
		{
			cma->MakeFromAssembly(MOI->GetAssembly(), FVector::OneVector, MOI->GetObjectInverted(), true);
		}
	}

	bool FMOIPortalImpl::SetRelativeTransform(const FVector2D &InRelativePos, const FQuat &InRelativeRot)
	{
		CachedRelativePos = InRelativePos;
		CachedRelativeRot = InRelativeRot;

		auto *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		AActor *portalActor = MOI ? MOI->GetActor() : nullptr;
		if ((parentObj == nullptr) || (portalActor == nullptr))
		{
			return false;
		}

		if (!UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(parentObj,
			CachedRelativePos, CachedRelativeRot, CachedWorldPos, CachedWorldRot))
		{
			return false;
		}

		portalActor->SetActorLocationAndRotation(CachedWorldPos, CachedWorldRot);
		CacheCorners();
		bHaveValidTransform = true;

		return true;
	}

	bool FMOIPortalImpl::CacheCorners()
	{
		CachedCorners.Reset(CachedCorners.Num());

		FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
		if (parentMOI == nullptr)
		{
			return false;
		}

		FTransform portalTransform = MOI->GetActor()->GetActorTransform();
		float parentThickness = MOI->GetParentObject()->CalculateThickness();
		FVector portalNormal = portalTransform.GetUnitAxis(EAxis::Y);
		FVector oppositeSideDelta = -parentThickness * portalNormal;
		int32 numCP = MOI->GetControlPoints().Num();

		for (int32 i = 0; i < 2 * numCP; ++i)
		{
			const FVector &cp = MOI->GetControlPoint(i % numCP);
			FVector corner = portalTransform.TransformPosition(cp);
			if (i >= numCP)
			{
				corner += oppositeSideDelta;
			}

			CachedCorners.Add(MoveTemp(corner));
		}

		return true;
	}

	void FMOIPortalImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		if (!ensure(CachedCorners.Num() == (2 * numCP)))
		{
			return;
		}

		FTransform portalTransform = MOI->GetActor()->GetActorTransform();
		FVector portalNormal = portalTransform.GetUnitAxis(EAxis::Y);

		for (int32 i = 0; i < 2 * numCP; ++i)
		{
			int32 cpIdx = i % numCP;
			int32 nextCPIdx = (cpIdx + 1) % numCP;
			int32 cpOffset = i - cpIdx;
			int32 nextI = nextCPIdx + cpOffset;
			float normalSign = (i < numCP) ? 1.0f : -1.0f;

			outPoints.Add(FStructurePoint(CachedCorners[i], normalSign * portalNormal, i));
			outLines.Add(FStructureLine(CachedCorners[i], CachedCorners[nextI], i, nextI));
		}
	}

	void FMOIPortalImpl::SetRotation(const FQuat &r)
	{
		SetRelativeTransform(CachedRelativePos, r);
	}

	FQuat FMOIPortalImpl::GetRotation() const
	{
		return CachedRelativeRot;
	}

	void FMOIPortalImpl::SetLocation(const FVector &p)
	{
		SetRelativeTransform(FVector2D(p), CachedRelativeRot);
	}

	FVector FMOIPortalImpl::GetCorner(int32 index) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		if (!ensureAlways(CachedCorners.Num() == (2 * numCP)))
		{
			return CachedWorldPos;
		}

		return CachedCorners[index];
	}

	FVector FMOIPortalImpl::GetLocation() const
	{
		return FVector(CachedRelativePos, 0.0f);
	}

	void FMOIPortalImpl::SetWorldTransform(const FTransform &NewTransform)
	{
		FModumateObjectInstance *parentObject = MOI ? MOI->GetParentObject() : nullptr;
		if (parentObject)
		{
			FVector2D newRelativePos(ForceInitToZero);
			FQuat newRelativeRot(ForceInit);
			if (UModumateObjectStatics::GetRelativeTransformOnPlaneHostedObj(parentObject,
				NewTransform.GetLocation(), NewTransform.GetRotation().GetAxisY(),
				0.0f, false, newRelativePos, newRelativeRot))
			{
				SetRelativeTransform(newRelativePos, newRelativeRot);
			}
		}
	}

	FTransform FMOIPortalImpl::GetWorldTransform() const
	{
		return FTransform(CachedWorldRot, CachedWorldPos);
	}

	void FMOIPortalImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (AdjustmentHandles.Num() > 0)
		{
			for (auto &ah : AdjustmentHandles)
			{
				ah->Destroy();
			}
			AdjustmentHandles.Empty();
		}
	}

	void FMOIPortalImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		Controller = controller;
		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}

		auto makeActor = [this, controller](FEditModelAdjustmentHandleBase *impl, UStaticMesh *mesh, const FVector &s, const int32& side, float offsetDist)
		{
			AAdjustmentHandleActor_CPP *actor = MOI->GetActor()->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(mesh);
			//actor->SetHandleScale(s);
			actor->SetHandleScaleScreenSize(s);
			actor->SetWallHandleSide(side, MOI, offsetDist);

			impl->Handle = actor;
			actor->Implementation = impl;
			actor->AttachToActor(MOI->GetActor(), FAttachmentTransformRules::KeepRelativeTransform);
			AdjustmentHandles.Add(actor);
		};

		UStaticMesh *pointAdjusterMesh = controller->EMPlayerState->GetEditModelGameMode()->PointAdjusterMesh;
		UStaticMesh *faceAdjusterMesh = controller->EMPlayerState->GetEditModelGameMode()->FaceAdjusterMesh;
		UStaticMesh *invertHandleMesh = controller->EMPlayerState->GetEditModelGameMode()->InvertHandleMesh;

		for (size_t i = 0; i < 8; ++i)
		{
			makeActor(new FAdjustPortalPointHandle(MOI, i), pointAdjusterMesh, FVector(0.0007f, 0.0007f, 0.0007f), -1, 0.0f);
		}

		makeActor(new FAdjustPortalSideHandle(MOI, 0), faceAdjusterMesh, FVector(0.0015, 0.0015, 0.0015), 0, 12.0);
		makeActor(new FAdjustPortalSideHandle(MOI, 1), faceAdjusterMesh, FVector(0.0015, 0.0015, 0.0015), 1, 12.0);
		makeActor(new FAdjustPortalSideHandle(MOI, 2), faceAdjusterMesh, FVector(0.0015, 0.0015, 0.0015), 2, 12.0);
		makeActor(new FAdjustPortalSideHandle(MOI, 3), faceAdjusterMesh, FVector(0.0015, 0.0015, 0.0015), 3, 12.0);
		//Side needs to be manually calculated in the invertPortalHandle b/c control points aren't following the same as walls
		makeActor(new FAdjustPortalInvertHandle(MOI, 1), invertHandleMesh, FVector(0.003f, 0.003f, 0.003f), -1, 0.f);

		// Flip handle for transverse is removed until functionality is fixed 
		makeActor(new FAdjustPortalInvertHandle(MOI, -1), controller->EMPlayerState->GetEditModelGameMode()->FlipHandleMesh, FVector(0.003f, 0.003f, 0.003f), -1, 0.f);
	}

	void FMOIPortalImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		Controller = controller;
		AModumateObjectInstanceParts_CPP* objPartsActor = nullptr;
		TArray<AActor*> attachActors;
		MOI->GetActor()->GetAttachedActors(attachActors);

		for (int32 i = 0; i < attachActors.Num(); i++)
		{
			if (attachActors[i]->IsA(AModumateObjectInstanceParts_CPP::StaticClass()))
			{
				objPartsActor = Cast<AModumateObjectInstanceParts_CPP>(attachActors[i]);
			}
		}

		if (show)
		{
			if (objPartsActor != nullptr)
			{
				if (objPartsActor->bIsPartBeingSelected == false)
				{
					SetupAdjustmentHandles(controller);
				}
			}
			else
			{
				SetupAdjustmentHandles(controller);
			}
		}

		for (auto &ah : AdjustmentHandles)
		{
			if (ah.IsValid())
			{
				ah->SetEnabled(show);
			}
		}
	}

	void FMOIPortalImpl::GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors)
	{
		outHandleActors = AdjustmentHandles;
	}

	TArray<FModelDimensionString> FMOIPortalImpl::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;

		if (MOI->GetControlPoints().Num() > 3)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(2));
			ds.Point2 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(1));
			ds.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);

			ds.Point1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(3));
			ds.Point2 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->GetControlPoint(0));
			ds.Functionality = EEnterableField::EditableText_ImperialUnits_UserInput;
			ds.Offset = 100;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ret.Add(ds);

		}

		return ret;
	}

	void FMOIPortalImpl::SetFromDataRecordAndRotation(const FMOIDataRecord &dataRec, const FVector &origin, const FQuat &rotation)
	{
		// Don't apply any rotation, we can't meaningfully rotate portals
		SetFromDataRecordAndDisplacement(dataRec, FVector::ZeroVector);
	}

	void FMOIPortalImpl::SetFromDataRecordAndDisplacement(const FMOIDataRecord &dataRec, const FVector &displacement)
	{
		FVector2D recordRelativePos(dataRec.Location);
		FQuat recordRelativeRot(dataRec.Rotation);
		FModumateObjectInstance *parentObj = MOI->GetParentObject();

		FVector displacedWorldPos = CachedWorldPos;
		FQuat displacedWorldRot = CachedWorldRot;
		if (UModumateObjectStatics::GetWorldTransformOnPlaneHostedObj(parentObj,
			recordRelativePos, recordRelativeRot, displacedWorldPos, displacedWorldRot))
		{
			displacedWorldPos += displacement;

			FVector2D displacedRelativePos = CachedRelativePos;
			FQuat displacedRelativeRot = CachedRelativeRot;
			if (UModumateObjectStatics::GetRelativeTransformOnPlaneHostedObj(parentObj,
				displacedWorldPos, displacedWorldRot.GetAxisY(), 0.0f, false, displacedRelativePos, displacedRelativeRot))
			{
				SetRelativeTransform(displacedRelativePos, displacedRelativeRot);
				parentObj->MarkDirty(EObjectDirtyFlags::Visuals);
			}
		}
	}

	void FMOIPortalImpl::InvertObject()
	{
		MOI->MarkDirty(EObjectDirtyFlags::Structure);
	}

	void FMOIPortalImpl::TransverseObject()
	{
		FModumateObjectInstance *parent = MOI ? MOI->GetParentObject() : nullptr;
		if (parent)
		{
			FQuat parentRot = parent->GetObjectRotation();
			FVector parentAxisX = parentRot.GetAxisX();

			FVector portalOriginOffset = MOI->GetControlPoint(0);
			FVector portalSize = MOI->GetControlPoint(2) - MOI->GetControlPoint(0);
			FVector portalXDir = CachedWorldRot.GetAxisX();
			float translationSign = parentAxisX | portalXDir;
			float flipXOffset = 2.0f * portalOriginOffset.X + portalSize.X;
			CachedRelativePos.X += translationSign * flipXOffset;

			bool bPortalRotated = !CachedRelativeRot.IsIdentity(KINDA_SMALL_NUMBER);
			CachedRelativeRot = FQuat(FVector::UpVector, bPortalRotated ? 0.0f : PI);

			MOI->InvertObject();
			MOI->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	FModumateWallMount FMOIPortalImpl::GetWallMountForSelf(int32 originIndex) const
	{
		FModumateWallMount ret;
		ret.OriginIndex = originIndex;
		ret.RelativePosition = FVector::ZeroVector;
		ret.OrientationDelta = FQuat::Identity;

		if (MOI == nullptr)
		{
			return ret;
		}

		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (parent == nullptr)
		{
			return ret;
		}

		// Store the object's position relative to a corner of the parent that isn't expected to change.
		FVector originCornerWorldPos = parent->GetCorner(originIndex);
		FTransform wallTransform(parent->GetObjectRotation(), parent->GetObjectLocation());
		FVector originCornerRelPos = wallTransform.InverseTransformPosition(originCornerWorldPos);

		FVector objectWorldPos = MOI->GetObjectLocation();
		FVector objectRelPos = wallTransform.InverseTransformPosition(objectWorldPos);
		ret.RelativePosition = objectRelPos - originCornerRelPos;

		/*ret.OrientationDelta = MOI->GetObjectRotation() - parent->GetObjectRotation();
		ret.OrientationDelta.Normalize();*/
		ret.OrientationDelta = MOI->GetObjectRotation().Inverse() * parent->GetObjectRotation();
		ret.OriginalControlPoints = MOI->GetControlPoints();

		return ret;
	}

	void FMOIPortalImpl::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		if (MOI)
		{
			FModumateObjectInstance *parent = MOI->GetParentObject();
			if (parent == nullptr || parent->GetObjectType() != EObjectType::OTWallSegment)
			{
				return;
			}

			// First, restore original control points before applying the relative wall mount,
			// in case they are used or modified during location/rotation setting.
			MOI->SetControlPoints(wm.OriginalControlPoints);

			// Use the stored position that was relative to the corner that wasn't supposed to change
			// to set the new position for the wall mounted object.
			FVector originCornerWorldPos = parent->GetCorner(wm.OriginIndex);
			FTransform wallTransform(parent->GetObjectRotation(), parent->GetObjectLocation());
			FVector originCornerRelPos = wallTransform.InverseTransformPosition(originCornerWorldPos);

			FVector objectRelPos = wm.RelativePosition + originCornerRelPos;
			FVector objectWorldPos = wallTransform.TransformPosition(objectRelPos);

			// For doors, constrain the mounted position to be at the wall's base, to prevent unnecessary hole boring errors
			if (MOI->GetObjectType() == EObjectType::OTDoor)
			{
				FVector baseCornerWorldPos = parent->GetCorner(0);
				objectWorldPos.Z = baseCornerWorldPos.Z;
			}

			MOI->SetObjectLocation(objectWorldPos);

			MOI->SetObjectRotation(wm.OrientationDelta * parent->GetObjectRotation());
			MOI->UpdateGeometry();
		}
	}

	void FMOIPortalImpl::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
	{
		ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(MOI->GetActor());

		TArray<TPair<FVector, FVector>> OutEdges;
		actor->ConvertProcMeshToLinesOnPlane(Origin, FVector(Plane), OutEdges);

		OutEdges.Num();

		Units::FThickness defaultThickness = Units::FThickness::Points(0.125f);
		FMColor defaultColor = FMColor::Gray64;
		FMColor swingColor = FMColor(0.0f, 0.0f, 0.0f);
		float defaultDoorOpeningDegrees = 90;

		for (auto& edge : OutEdges)
		{
			FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Key);
			FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Value);

			TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
				Units::FCoordinates2D::WorldCentimeters(start),
				Units::FCoordinates2D::WorldCentimeters(end),
				defaultThickness, defaultColor));
			ParentPage->Children.Add(line);
		}

		// draw door swing lines if the cut plane intersects the door's mesh
		if (MOI->GetObjectType() == EObjectType::OTDoor && OutEdges.Num() > 0)
		{
			auto assembly = MOI->GetAssembly();
			auto portalConfig = assembly.PortalConfiguration;
			auto portalFunction = portalConfig.PortalFunction;

			// we only draw arcs for swing doors, where the cut plane's normal is vertical and the door is vertical
			bool bFloorplanPlane = FVector::Coincident(FVector::DownVector, FVector(Plane));
			FVector rotationAxis = CachedWorldRot.GetRotationAxis();
			bool bWallVertical = FVector::Parallel(FVector::UpVector, rotationAxis) || CachedWorldRot.IsIdentity(KINDA_SMALL_NUMBER);
			if (portalFunction == EPortalFunction::Swing && bFloorplanPlane && bWallVertical)
			{
				auto parent = MOI->GetParentObject();

				// Get amount of panels
				TArray<int32> panelSlotIndices;

				for (int32 idx = 0; idx < portalConfig.Slots.Num(); idx++)
				{
					auto& slot = portalConfig.Slots[idx];
					if (slot.Type == EPortalSlotType::Panel)
					{
						panelSlotIndices.Add(idx);
					}
				}

				int32 numPanels = panelSlotIndices.Num();
				FVector windowDiagonal = MOI->GetControlPoint(2) - MOI->GetControlPoint(0);

				FVector p1 = FVector::PointPlaneProject(GetCorner(0), Plane);
				FVector p2 = FVector::PointPlaneProject(GetCorner(2), Plane);

				float angle = -CachedWorldRot.GetAngle();
				angle *= FVector::DotProduct(FVector::UpVector, rotationAxis);
				// TODO: enable if door swing arcs should show when plans are facing the ceiling
				//angle *= FVector::DotProduct(FVector::DownVector, FVector(Plane));

				for (int32 panelIdx : panelSlotIndices)
				{
					auto& panel = portalConfig.Slots[panelIdx];

					// angle offset represents how much the arc needs to be rotated to accommodate for -
					// the panel being flipped in the assembly, whether it is positioned with RXLeft or RXRight, 
					// and whether the moi is inverted (for portals, transverse flips MOI->GetObjectInverted())
					// angle offset is used when the door swing arc is rotated
					float angleOffset = panel.FlipX ? 0.5f : 0.0f;
					if (MOI->GetObjectInverted()) 
					{
						angleOffset -= panel.FlipX ? -0.5f : 0.5f;
					}

					// TODO: this logic has already been solved in CompoundMeshActor.  Given an understanding of 2D
					// sub-graphs, make sure that the portal transforms work the same way for drafting and for the models
					FVector hingeLocation;
					if (panel.LocationX == FPortalConfiguration::RefPlaneNameMinX.ToString())
					{
						hingeLocation = !MOI->GetObjectInverted() ? p1 : p2;
					}
					else if (panel.LocationX == FPortalConfiguration::RefPlaneNameMaxX.ToString())
					{
						hingeLocation = !MOI->GetObjectInverted() ? p2 : p1;

						angleOffset += 1.0f;
					}
					// TODO: handle panel positions other than RXLeft (RefPlaneNameMinX) and RXRight (RefPlaneNameMaxX)
					else
					{
						continue;
					}

					// TODO: this is only correct as long as each panel is the same size
					float width = windowDiagonal.X / numPanels;
				
					// create arc object
					TSharedPtr<FDraftingArc> doorSwing = MakeShareable(new FDraftingArc(
						Units::FRadius::WorldCentimeters(width),
						Units::FAngle::Degrees(defaultDoorOpeningDegrees),
						defaultThickness,
						swingColor));

					FVector2D hingeLocation2D = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, hingeLocation);
					doorSwing->SetLocalPosition(Units::FCoordinates2D::WorldCentimeters(hingeLocation2D));
					doorSwing->SetLocalOrientation(Units::FAngle::Radians(angle + (PI*(-0.5f + angleOffset))));

					ParentPage->Children.Add(doorSwing);

					TSharedPtr<FDraftingLine> doorLine = MakeShareable(new FDraftingLine(Units::FLength::WorldCentimeters(width), defaultThickness, swingColor));
					doorLine->SetLocalPosition(Units::FCoordinates2D::WorldCentimeters(hingeLocation2D));
					doorLine->SetLocalOrientation(Units::FAngle::Radians(angle + (PI*-0.5f)));

					ParentPage->Children.Add(doorLine);
				}
			}
		}
	}
}
