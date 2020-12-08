// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Portal.h"

#include "Algo/Unique.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalInvertHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalReverseHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalJustifyHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalOrientHandle.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/ModumateObjectInstanceParts_CPP.h"


class AEditModelPlayerController_CPP;


FMOIPortalImpl::FMOIPortalImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, Controller(nullptr)
	, CachedRelativePos(ForceInitToZero)
	, CachedWorldPos(ForceInitToZero)
	, CachedRelativeRot(ForceInit)
	, CachedWorldRot(ForceInit)
	, bHaveValidTransform(false)
{
}

FMOIPortalImpl::~FMOIPortalImpl()
{}

AActor *FMOIPortalImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	return world->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
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

void FMOIPortalImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

bool FMOIPortalImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag, OutSideEffectDeltas))
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
			const Modumate::FGraph3DFace * parentFace = MOI->GetDocument()->GetVolumeGraph().FindFace(parentObj->ID);

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
	SetupCompoundActorGeometry();
	SetRelativeTransform(CachedRelativePos, CachedRelativeRot);
}

void FMOIPortalImpl::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

bool FMOIPortalImpl::SetupCompoundActorGeometry()
{
	bool bResult = false;
	if (ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor()))
	{
		FVector scale(FVector::OneVector);
		int32 parentID = MOI->GetParentID();
		if (parentID != MOD_ID_NONE)
		{
			const Modumate::FGraph3DFace * parentFace = MOI->GetDocument()->GetVolumeGraph().FindFace(parentID);
			// Get size of parent metaplane.
			if (parentFace != nullptr)
			{
				float lateralInvertFactor = InstanceData.bLateralInverted ? -1.0f : 1.0f;
				float normalInvertFactor = InstanceData.bNormalInverted ? -1.0f : 1.0f;
				int32 numRotations = (int32)InstanceData.Orientation;
				FQuat localRotation = FQuat::MakeFromEuler(FVector(0.0f, 90.0f * numRotations, 0.0f));
				FBox2D faceSize(parentFace->Cached2DPositions);
				FVector2D planeSize = faceSize.GetSize();
				FVector2D localPosition(lateralInvertFactor * -faceSize.GetExtent().X, faceSize.GetExtent().Y);
				auto localPosition3d = localRotation.RotateVector(FVector(localPosition.Y, 0.0f, localPosition.X));
				localPosition = FVector2D(localPosition3d.Z, localPosition3d.X);


				const FBIMAssemblySpec& assembly = MOI->GetAssembly();
				FVector nativeSize = assembly.GetRiggedAssemblyNativeSize();
				if (!nativeSize.IsZero())
				{	// Assume first part for native size.
					if (numRotations % 2 == 0)
					{
						scale.X = planeSize.X / nativeSize.X * lateralInvertFactor;
						scale.Y *= normalInvertFactor;
						scale.Z = planeSize.Y / nativeSize.Z;
					}
					else
					{
						scale.X = planeSize.Y / nativeSize.X * lateralInvertFactor;
						scale.Y *= normalInvertFactor;
						scale.Z = planeSize.X / nativeSize.Z;
						localPosition.X = localPosition.X * planeSize.X / planeSize.Y;
						localPosition.Y = localPosition.Y * planeSize.Y / planeSize.X;
					}

					SetRelativeTransform(localPosition, localRotation);

					bResult = true;
				}
			}
		}

		cma->MakeFromAssembly(MOI->GetAssembly(), scale, InstanceData.bLateralInverted, true);
	}
	return bResult;
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

	if (!UModumateObjectStatics::GetWorldTransformOnPlanarObj(parentObj,
		CachedRelativePos, CachedRelativeRot, CachedWorldPos, CachedWorldRot))
	{
		return false;
	}

	CachedWorldPos += InstanceData.Justification * parentObj->GetNormal();
	portalActor->SetActorLocationAndRotation(CachedWorldPos, CachedWorldRot);
	bHaveValidTransform = true;

	return true;
}

void FMOIPortalImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	FModumateObjectInstance* parentObject = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObject && (parentObject->GetObjectType() == EObjectType::OTMetaPlane))
	{
		parentObject->GetStructuralPointsAndLines(outPoints, outLines, bForSnapping, bForSelection);
	}
}

FQuat FMOIPortalImpl::GetRotation() const
{
	return CachedRelativeRot;
}

FVector FMOIPortalImpl::GetCorner(int32 index) const
{
	FModumateObjectInstance* parentObject = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObject && (parentObject->GetObjectType() == EObjectType::OTMetaPlane))
	{
		return parentObject->GetCorner(index);
	}

	return FVector::ZeroVector;
}

FVector FMOIPortalImpl::GetLocation() const
{
	return FVector(CachedRelativePos, 0.0f);
}

FTransform FMOIPortalImpl::GetWorldTransform() const
{
	return FTransform(CachedWorldRot, CachedWorldPos);
}

void FMOIPortalImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	FModumateObjectInstance *parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto edgeHandle = MOI->MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetTargetMOI(parent);
	}

	MOI->MakeHandle<AAdjustPortalInvertHandle>();
	MOI->MakeHandle<AAdjustPortalJustifyHandle>();
	MOI->MakeHandle<AAdjustPortalReverseHandle>();
	auto cwOrientHandle = MOI->MakeHandle<AAdjustPortalOrientHandle>();
	cwOrientHandle->CounterClockwise = false;
	auto ccwOrientHandle = MOI->MakeHandle<AAdjustPortalOrientHandle>();
	ccwOrientHandle->CounterClockwise = true;
}

bool FMOIPortalImpl::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = MOI->GetStateData();

	FMOIPortalData modifiedTrimData = InstanceData;
	modifiedTrimData.bNormalInverted = !modifiedTrimData.bNormalInverted;

	return OutState.CustomData.SaveStructData(modifiedTrimData);
}

void FMOIPortalImpl::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
{
	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	const ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (bGetFarLines)
	{
			actor->GetFarDraftingLines(ParentPage, Plane, BoundingBox);
	}
	else
	{
		bool bLinesDrawn = actor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin);

		Modumate::Units::FThickness defaultThickness = Modumate::Units::FThickness::Points(0.1f);
		Modumate::FMColor defaultColor = Modumate::FMColor::Gray64;
		Modumate::FMColor swingColor = Modumate::FMColor::Gray160;
		static constexpr float defaultDoorOpeningDegrees = 90.0f;

		// draw door swing lines if the cut plane intersects the door's mesh
		// TODO: door swings for DDL 2.0 openings
		if (MOI->GetObjectType() == EObjectType::OTDoor && bLinesDrawn)
		{
			const FBIMAssemblySpec& assembly = MOI->GetAssembly();
			const auto& parts = assembly.Parts;

			const FVector doorUpVector = CachedWorldRot.RotateVector(FVector::UpVector);
			bool bOrthogonalCut = FVector::Parallel(doorUpVector, Plane);
			// Draw opening lines where the door axis is normal to the cut plane.
			if (bOrthogonalCut)
			{
				bool bCollinear = (doorUpVector | Plane) > 0.0f;
				bool bLeftHanded = ((AxisX ^ AxisY) | Plane) < 0.0f;  // True for interactive drafting.
				bool bPositiveSwing = bCollinear ^ InstanceData.bLateralInverted ^ InstanceData.bNormalInverted ^ bLeftHanded;
				auto parent = MOI->GetParentObject();

				// Get amount of panels
				TArray<int32> panelSlotIndices;

				static const FBIMTagPath panelTag("Part_Panel");
				for (int32 slot = 0; slot < parts.Num(); ++slot)
				{
					if (parts[slot].NodeCategoryPath == panelTag)
					{
						panelSlotIndices.Add(slot);
					}
				}

				int32 numPanels = panelSlotIndices.Num();

				for (int32 panelIdx : panelSlotIndices)
				{
					const UStaticMeshComponent* meshComponent = actor->StaticMeshComps[panelIdx];
					if (!meshComponent)
					{
						continue;
					}
					FVector meshMin, meshMax;
					meshComponent->GetLocalBounds(meshMin, meshMax);

					const FTransform componentXfrom = meshComponent->GetRelativeTransform();
					const FTransform panelXfrom = componentXfrom * actor->GetTransform();
					FVector axis = panelXfrom.TransformPosition(FVector(meshMin.X, meshMax.Y, 0.0f));
					FVector panelEnd = panelXfrom.TransformPosition(FVector(meshMax.X, meshMax.Y, 0.0f));
					float panelLength = (panelEnd - axis).Size();
					FVector2D planStart = UModumateGeometryStatics::ProjectPoint2D(axis, AxisX, AxisY, Origin);
					FVector2D planEnd = UModumateGeometryStatics::ProjectPoint2D(panelEnd, AxisX, AxisY, Origin);

					const FRotator doorSwing(0, bPositiveSwing ? +defaultDoorOpeningDegrees : -defaultDoorOpeningDegrees, 0);
					planEnd = FVector2D(doorSwing.RotateVector(FVector(planEnd - planStart, 0)) ) + planStart;
					FVector2D planDelta = planEnd - planStart;

					FVector2D clippedStart, clippedEnd;
					if (UModumateFunctionLibrary::ClipLine2DToRectangle(planStart, planEnd, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
							defaultThickness, swingColor);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kOpeningSystemOperatorLine);

						auto arcAngle = FMath::Atan2(planDelta.Y, planDelta.X);

						if (bPositiveSwing)
						{
							arcAngle -= Modumate::Units::FUnitValue::Degrees(defaultDoorOpeningDegrees).AsRadians();
						}
						TSharedPtr<Modumate::FDraftingArc> doorArc = MakeShared<Modumate::FDraftingArc>(
							Modumate::Units::FLength::WorldCentimeters(panelLength),
							Modumate::Units::FRadius::Degrees(defaultDoorOpeningDegrees),
							defaultThickness, swingColor);
						doorArc->SetLocalPosition(Modumate::Units::FCoordinates2D::WorldCentimeters(planStart));
						doorArc->SetLocalOrientation(Modumate::Units::FUnitValue::Radians(arcAngle));
						ParentPage->Children.Add(doorArc);
						doorArc->SetLayerTypeRecursive(Modumate::FModumateLayerType::kOpeningSystemOperatorLine);
					}
				}
			}
		}
	}

}

void FMOIPortalImpl::SetIsDynamic(bool bIsDynamic)
{
	auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (meshActor)
	{
		meshActor->SetIsDynamic(bIsDynamic);
	}
}

bool FMOIPortalImpl::GetIsDynamic() const
{
	auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (meshActor)
	{
		return meshActor->GetIsDynamic();
	}
	return false;
}
