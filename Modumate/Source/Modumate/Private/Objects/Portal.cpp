// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Portal.h"

#include "Algo/Unique.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalInvertHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalReverseHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalJustifyHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalOrientHandle.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateObjectInstanceParts.h"
#include "Quantities/QuantitiesVisitor.h"


class AEditModelPlayerController;


FMOIPortalData::FMOIPortalData()
{
}

FMOIPortalData::FMOIPortalData(int32 InVersion)
	: Version(InVersion)
{
}


AMOIPortal::AMOIPortal()
	: AModumateObjectInstance()
	, Controller(nullptr)
	, CachedRelativePos(ForceInitToZero)
	, CachedWorldPos(ForceInitToZero)
	, CachedRelativeRot(ForceInit)
	, CachedWorldRot(ForceInit)
	, bHaveValidTransform(false)
{
}

AActor *AMOIPortal::CreateActor(const FVector &loc, const FQuat &rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

FVector AMOIPortal::GetNormal() const
{
	const AActor *portalActor = GetActor();
	if (!ensure(portalActor != nullptr))
	{
		return FVector::ZeroVector;
	}

	FTransform portalTransform = portalActor->GetActorTransform();
	return portalTransform.GetUnitAxis(EAxis::Y);
}

bool AMOIPortal::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (!AModumateObjectInstance::CleanObject(DirtyFlag, OutSideEffectDeltas))
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto *parentObj = GetParentObject();
		if (parentObj)
		{
			const Modumate::FGraph3DFace * parentFace = GetDocument()->GetVolumeGraph().FindFace(parentObj->ID);

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

void AMOIPortal::SetupDynamicGeometry()
{
	SetupCompoundActorGeometry();
	SetRelativeTransform(CachedRelativePos, CachedRelativeRot);
}

void AMOIPortal::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

bool AMOIPortal::SetupCompoundActorGeometry()
{
	bool bResult = false;
	if (ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(GetActor()))
	{
		FVector scale(FVector::OneVector);
		int32 parentID = GetParentID();
		if (parentID != MOD_ID_NONE)
		{
			const Modumate::FGraph3DFace * parentFace = GetDocument()->GetVolumeGraph().FindFace(parentID);
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


				const FBIMAssemblySpec& assembly = GetAssembly();
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

		cma->MakeFromAssembly(GetAssembly(), scale, InstanceData.bLateralInverted, true);
	}
	return bResult;
}

bool AMOIPortal::SetRelativeTransform(const FVector2D &InRelativePos, const FQuat &InRelativeRot)
{
	CachedRelativePos = InRelativePos;
	CachedRelativeRot = InRelativeRot;

	auto *parentObj = GetParentObject();
	AActor *portalActor = GetActor();
	if ((parentObj == nullptr) || (portalActor == nullptr))
	{
		return false;
	}

	if (!UModumateObjectStatics::GetWorldTransformOnPlanarObj(parentObj,
		CachedRelativePos, CachedRelativeRot, CachedWorldPos, CachedWorldRot))
	{
		return false;
	}

	float normalFlipSign = InstanceData.bNormalInverted ? -1.0f : 1.0f;
	float assemblyThickness = CachedAssembly.GetRiggedAssemblyNativeSize().Y;
	float offsetDist = (-0.5f * normalFlipSign * assemblyThickness) + InstanceData.Offset.GetOffsetDistance(normalFlipSign, assemblyThickness);
	CachedWorldPos += offsetDist * parentObj->GetNormal();
	portalActor->SetActorLocationAndRotation(CachedWorldPos, CachedWorldRot);
	bHaveValidTransform = true;

	return true;
}

void AMOIPortal::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const AModumateObjectInstance* parentObject = GetParentObject();
	if (parentObject && (parentObject->GetObjectType() == EObjectType::OTMetaPlane))
	{
		parentObject->GetStructuralPointsAndLines(outPoints, outLines, bForSnapping, bForSelection);
	}
}

FQuat AMOIPortal::GetRotation() const
{
	return CachedRelativeRot;
}

FVector AMOIPortal::GetCorner(int32 index) const
{
	const AModumateObjectInstance* parentObject = GetParentObject();
	if (parentObject && (parentObject->GetObjectType() == EObjectType::OTMetaPlane))
	{
		return parentObject->GetCorner(index);
	}

	return FVector::ZeroVector;
}

FVector AMOIPortal::GetLocation() const
{
	return FVector(CachedRelativePos, 0.0f);
}

FTransform AMOIPortal::GetWorldTransform() const
{
	return FTransform(CachedWorldRot, CachedWorldPos);
}

void AMOIPortal::SetupAdjustmentHandles(AEditModelPlayerController *controller)
{
	AModumateObjectInstance *parent = GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto edgeHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetTargetMOI(parent);
	}

	MakeHandle<AAdjustPortalInvertHandle>();
	MakeHandle<AAdjustPortalJustifyHandle>();
	MakeHandle<AAdjustPortalReverseHandle>();
	auto cwOrientHandle = MakeHandle<AAdjustPortalOrientHandle>();
	cwOrientHandle->CounterClockwise = false;
	auto ccwOrientHandle = MakeHandle<AAdjustPortalOrientHandle>();
	ccwOrientHandle->CounterClockwise = true;
}

bool AMOIPortal::GetInvertedState(FMOIStateData& OutState) const
{
	return GetFlippedState(EAxis::Y, OutState);
}

bool AMOIPortal::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIPortalData modifiedPortalData = InstanceData;
	switch (FlipAxis)
	{
	case EAxis::X:
		modifiedPortalData.bLateralInverted = !modifiedPortalData.bLateralInverted;
		break;
	case EAxis::Y:
		modifiedPortalData.bNormalInverted = !modifiedPortalData.bNormalInverted;
		break;
	case EAxis::Z:
		break;
	default:
		return false;
	}

	return OutState.CustomData.SaveStructData(modifiedPortalData);
}

bool AMOIPortal::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	float projectedAdjustment = AdjustmentDirection | GetNormal();
	if (FMath::IsNearlyZero(projectedAdjustment, THRESH_NORMALS_ARE_ORTHOGONAL))
	{
		projectedAdjustment = 0.0f;
	}

	float projectedAdjustmentSign = FMath::Sign(projectedAdjustment);
	float normalFlipSign = InstanceData.bNormalInverted ? -1.0f : 1.0f;
	EDimensionOffsetType nextOffsetType = InstanceData.Offset.GetNextType(projectedAdjustmentSign, normalFlipSign);

	FMOIPortalData modifiedPortalData = InstanceData;
	modifiedPortalData.Offset.Type = nextOffsetType;
	OutState = GetStateData();

	return OutState.CustomData.SaveStructData(modifiedPortalData);
}

void AMOIPortal::RegisterInstanceDataUI(UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Flip"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetFlip>(this, flipPropertyName))
	{
		flipField->RegisterValue(this, EAxisList::XY);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOIPortal::OnInstPropUIChangedFlip);
	}

	static const FString offsetPropertyName(TEXT("Offset"));
	if (auto offsetField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetPropertyName))
	{
		offsetField->RegisterValue(this, InstanceData.Offset);
		offsetField->ValueChangedEvent.AddDynamic(this, &AMOIPortal::OnInstPropUIChangedOffset);
	}
}

void AMOIPortal::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
{
	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	const ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(GetActor());
	if (bGetFarLines)
	{
		actor->GetFarDraftingLines(ParentPage, Plane, BoundingBox);
	}
	else
	{
		bool bLinesDrawn = actor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin);

		ModumateUnitParams::FThickness defaultThickness = ModumateUnitParams::FThickness::Points(0.1f);
		Modumate::FMColor defaultColor = Modumate::FMColor::Gray64;
		Modumate::FMColor swingColor = Modumate::FMColor::Gray160;
		static constexpr float defaultDoorOpeningDegrees = 90.0f;

		// draw door swing lines if the cut plane intersects the door's mesh
		// TODO: door swings for DDL 2.0 openings
		if (GetObjectType() == EObjectType::OTDoor && bLinesDrawn)
		{

			const FVector doorUpVector = CachedWorldRot.RotateVector(FVector::UpVector);
			bool bOrthogonalCut = FVector::Parallel(doorUpVector, Plane);
			// Draw opening lines where the door axis is normal to the cut plane.
			if (bOrthogonalCut)
			{
				bool bCollinear = (doorUpVector | Plane) > 0.0f;
				bool bLeftHanded = ((AxisX ^ AxisY) | Plane) < 0.0f;  // True for interactive drafting.
				bool bPositiveSwing = bCollinear ^ InstanceData.bLateralInverted ^ InstanceData.bNormalInverted ^ bLeftHanded;

				const FBIMAssemblySpec& assembly = GetAssembly();
				const auto& parts = assembly.Parts;
				EDoorOperationType operationType = GetDoorType();

				if (operationType == EDoorOperationType::Swing)
				{
					// Get amount of panels
					TArray<int32> panelSlotIndices;

					static const FString panelTag("Panel");
					for (int32 slot = 0; slot < parts.Num(); ++slot)
					{
						const auto& ncp = parts[slot].NodeCategoryPath;
						if (ncp.Contains(panelTag))
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
						planEnd = FVector2D(doorSwing.RotateVector(FVector(planEnd - planStart, 0))) + planStart;
						FVector2D planDelta = planEnd - planStart;

						FVector2D clippedStart, clippedEnd;
						if (UModumateFunctionLibrary::ClipLine2DToRectangle(planStart, planEnd, BoundingBox, clippedStart, clippedEnd))
						{
							TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
								FModumateUnitCoord2D::WorldCentimeters(clippedStart),
								FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
								defaultThickness, swingColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kOpeningSystemOperatorLine);

							auto arcAngle = FMath::Atan2(planDelta.Y, planDelta.X);

							if (bPositiveSwing)
							{
								arcAngle -= FModumateUnitValue::Degrees(defaultDoorOpeningDegrees).AsRadians();
							}
							TSharedPtr<Modumate::FDraftingArc> doorArc = MakeShared<Modumate::FDraftingArc>(
								ModumateUnitParams::FLength::WorldCentimeters(panelLength),
								ModumateUnitParams::FRadius::Degrees(defaultDoorOpeningDegrees),
								defaultThickness, swingColor);
							doorArc->SetLocalPosition(FModumateUnitCoord2D::WorldCentimeters(planStart));
							doorArc->SetLocalOrientation(FModumateUnitValue::Radians(arcAngle));
							ParentPage->Children.Add(doorArc);
							doorArc->SetLayerTypeRecursive(Modumate::FModumateLayerType::kOpeningSystemOperatorLine);
						}
					}
				}
			}
		}
	}

}

void AMOIPortal::SetIsDynamic(bool bIsDynamic)
{
	auto meshActor = Cast<ACompoundMeshActor>(GetActor());
	if (meshActor)
	{
		meshActor->SetIsDynamic(bIsDynamic);
	}
}

bool AMOIPortal::GetIsDynamic() const
{
	auto meshActor = Cast<ACompoundMeshActor>(GetActor());
	if (meshActor)
	{
		return meshActor->GetIsDynamic();
	}
	return false;
}

void AMOIPortal::PostLoadInstanceData()
{
	bool bFixedInstanceData = false;

	if (InstanceData.Version < InstanceData.CurrentVersion)
	{
		if (InstanceData.Version < 1)
		{
			float flippedJustification = 1.0f - InstanceData.Justification_DEPRECATED;
			if (FMath::IsNearlyEqual(flippedJustification, 0.0f))
			{
				InstanceData.Offset.Type = EDimensionOffsetType::Negative;
			}
			else if (FMath::IsNearlyEqual(flippedJustification, 1.0f))
			{
				InstanceData.Offset.Type = EDimensionOffsetType::Positive;
			}
			else
			{
				InstanceData.Offset.Type = EDimensionOffsetType::Centered;
			}
		}

		InstanceData.Version = InstanceData.CurrentVersion;
		bFixedInstanceData = true;
	}

	if (bFixedInstanceData)
	{
		StateData.CustomData.SaveStructData(InstanceData);
	}
}

bool AMOIPortal::ProcessQuantities(FQuantitiesVisitor& QuantitiesVisitor) const
{
	const FBIMAssemblySpec& assembly = CachedAssembly;
	auto assemblyGuid = assembly.UniqueKey();
	const Modumate::FGraph3D& graph = Document->GetVolumeGraph();
	const Modumate::FGraph3DFace* hostingFace = graph.FindFace(GetParentID());

	if (!hostingFace)
	{
		return false;
	}

	float width = (hostingFace->Cached2DPositions[2] - hostingFace->Cached2DPositions[1]).Size();
	float height = (hostingFace->Cached2DPositions[1] - hostingFace->Cached2DPositions[0]).Size();
	int32 namingWidth = FMath::RoundToInt(width * UModumateDimensionStatics::CentimetersToInches);
	int32 namingHeight = FMath::RoundToInt(height * UModumateDimensionStatics::CentimetersToInches);
	FString name = FString::FromInt(namingWidth) + TEXT("x") + FString::FromInt(namingHeight);
	QuantitiesVisitor.AddPartsQuantity(name, assembly.Parts, assemblyGuid);
	return true;
}

EDoorOperationType AMOIPortal::GetDoorType() const
{
	if (GetObjectType() == EObjectType::OTDoor)
	{
		const FBIMAssemblySpec& assembly = GetAssembly();
		const auto& configTags = assembly.SlotConfigTagPath.Tags;
		for (const FString& configTag: configTags)
		{
			int64 doorType = StaticEnum<EDoorOperationType>()->GetValueByNameString(configTag);
			if (doorType != INDEX_NONE)
			{
				return EDoorOperationType(doorType);
			}
		}
	}

	return EDoorOperationType::None;
}

void AMOIPortal::OnInstPropUIChangedFlip(int32 FlippedAxisInt)
{
	EAxis::Type flippedAxis = static_cast<EAxis::Type>(FlippedAxisInt);
	if (Document)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		switch (flippedAxis)
		{
		case EAxis::X:
			newInstanceData.bLateralInverted = !newInstanceData.bLateralInverted;
			break;
		case EAxis::Y:
			newInstanceData.bNormalInverted = !newInstanceData.bNormalInverted;
			break;
		default:
			break;
		}
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIPortal::OnInstPropUIChangedOffset(const FDimensionOffset& NewValue)
{
	if (Document && (InstanceData.Offset != NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Offset = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}
