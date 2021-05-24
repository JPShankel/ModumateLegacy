// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Portal.h"

#include "Algo/Unique.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Objects/MiterNode.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPortalOrientHandle.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateObjectInstanceParts.h"
#include "Quantities/QuantitiesManager.h"


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
	, CachedThickness(0.0f)
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
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		UpdateCachedThickness();

		// When structure (assembly, offset, or plane structure) changes, mark neighboring
		// edges as miter-dirty, so they can re-evaluate details with the new edge conditions.
		if (!MarkEdgesMiterDirty())
		{
			return false;
		}

		MarkDirty(EObjectDirtyFlags::Mitering);
		break;
	}
	case EObjectDirtyFlags::Mitering:
	{
		// Make sure all connected edges have resolved mitering,
		// so we can apply any resolved edge details that might apply to this portal.
		for (AModumateObjectInstance* connectedEdge : CachedConnectedEdges)
		{
			if (connectedEdge->IsDirty(EObjectDirtyFlags::Mitering))
			{
				return false;
			}
		}

		return SetupCompoundActorGeometry() && SetRelativeTransform(CachedRelativePos, CachedRelativeRot);
	}
	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();
	default:
		break;
	}

	return true;
}

void AMOIPortal::UpdateCachedThickness()
{
	CachedThickness = CachedAssembly.GetRiggedAssemblyNativeSize().Y;

	FBIMLayerSpec& proxyLayer = (CachedProxyLayers.Num() == 0) ? CachedProxyLayers.AddDefaulted_GetRef() : CachedProxyLayers[0];
	proxyLayer.ThicknessCentimeters = CachedThickness;

	CachedLayerDims.UpdateLayersFromAssembly(CachedProxyLayers);
}

bool AMOIPortal::MarkEdgesMiterDirty()
{
	CachedParentConnectedMOIs.Reset();
	CachedConnectedEdges.Reset();

	const AModumateObjectInstance* planeParent = GetParentObject();
	if (planeParent == nullptr)
	{
		return false;
	}

	planeParent->GetConnectedMOIs(CachedParentConnectedMOIs);

	for (AModumateObjectInstance* planeConnectedMOI : CachedParentConnectedMOIs)
	{
		if (planeConnectedMOI && (planeConnectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
		{
			CachedConnectedEdges.Add(planeConnectedMOI);
			planeConnectedMOI->MarkDirty(EObjectDirtyFlags::Mitering);
		}
	}

	return (CachedConnectedEdges.Num() > 0);
}

bool AMOIPortal::GetOffsetFaceBounds(FBox2D& OutOffsetBounds, FVector2D& OutOffset)
{
	OutOffsetBounds.Init();
	OutOffset = FVector2D::ZeroVector;

	const FGraph3DFace* parentFace = Document->GetVolumeGraph().FindFace(GetParentID());
	if (parentFace == nullptr)
	{
		return false;
	}

	// Get size of parent metaplane, accounting for edge details that may have extended/retracted neighboring edges
	int32 numEdges = parentFace->EdgeIDs.Num();
	TArray<FVector2D> facePoints = parentFace->Cached2DPositions;

	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 edgeID = FMath::Abs(parentFace->EdgeIDs[edgeIdx]);
		AModumateObjectInstance* edgeObj = Document->GetObjectById(edgeID);
		const IMiterNode* miterNode = edgeObj ? edgeObj->GetMiterInterface() : nullptr;
		if (!ensure(miterNode))
		{
			return false;
		}

		const FMiterData& miterData = miterNode->GetMiterData();
		const FMiterParticipantData* participantData = miterData.ParticipantsByID.Find(ID);
		if (!ensure(participantData))
		{
			return false;
		}

		const FVector2D& edgeDir = parentFace->Cached2DEdgeNormals[edgeIdx];

		// Portals are only affected by edge details that extend one "layer" of the participant, which in this case is the whole side of the assembly.
		FVector2D edgeExtension(ForceInitToZero);
		if (participantData->LayerExtensions.Num() == 1)
		{
			const FVector2D& layerExtension = participantData->LayerExtensions[0];
			edgeExtension = -0.5f * (layerExtension.X + layerExtension.Y) * edgeDir;
		}

		FVector2D& curPoint = facePoints[edgeIdx];
		FVector2D& nextPoint = facePoints[(edgeIdx + 1) % numEdges];
		curPoint += edgeExtension;
		nextPoint += edgeExtension;
		OutOffset += edgeExtension;
	}

	OutOffsetBounds = FBox2D(facePoints);

	return true;
}

bool AMOIPortal::SetupCompoundActorGeometry()
{
	bool bResult = false;

	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (cma == nullptr)
	{
		return false;
	}

	FBox2D offsetBounds;
	FVector2D positionOffset;
	if (!GetOffsetFaceBounds(offsetBounds, positionOffset))
	{
		return false;
	}

	FVector scale(FVector::OneVector);
	float lateralInvertFactor = InstanceData.bLateralInverted ? -1.0f : 1.0f;
	float normalInvertFactor = InstanceData.bNormalInverted ? -1.0f : 1.0f;
	int32 numRotations = (int32)InstanceData.Orientation;
	FQuat localRotation = FQuat::MakeFromEuler(FVector(0.0f, 90.0f * numRotations, 0.0f));
	FVector2D offsetFaceSize = offsetBounds.GetSize();
	FVector2D offsetFaceExtent = offsetBounds.GetExtent();

	FVector2D localPosition(lateralInvertFactor * -offsetFaceExtent.X, offsetFaceExtent.Y);
	localPosition += 0.5f * positionOffset;

	auto localPosition3d = localRotation.RotateVector(FVector(localPosition.Y, 0.0f, localPosition.X));
	localPosition = FVector2D(localPosition3d.Z, localPosition3d.X);

	const FBIMAssemblySpec& assembly = GetAssembly();
	FVector nativeSize = assembly.GetRiggedAssemblyNativeSize();
	if (!nativeSize.IsZero())
	{	// Assume first part for native size.
		if (numRotations % 2 == 0)
		{
			scale.X = offsetFaceSize.X / nativeSize.X * lateralInvertFactor;
			scale.Y *= normalInvertFactor;
			scale.Z = offsetFaceSize.Y / nativeSize.Z;
		}
		else
		{
			scale.X = offsetFaceSize.Y / nativeSize.X * lateralInvertFactor;
			scale.Y *= normalInvertFactor;
			scale.Z = offsetFaceSize.X / nativeSize.Z;
			localPosition.X = localPosition.X * offsetFaceSize.X / offsetFaceSize.Y;
			localPosition.Y = localPosition.Y * offsetFaceSize.Y / offsetFaceSize.X;
		}

		CachedRelativePos = localPosition;
		CachedRelativeRot = localRotation;
		bResult = true;
	}

	cma->MakeFromAssembly(GetAssembly(), scale, InstanceData.bLateralInverted, true);

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
	float offsetDist = (-0.5f * normalFlipSign * CachedThickness) + InstanceData.Offset.GetOffsetDistance(normalFlipSign, CachedThickness);
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

void AMOIPortal::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
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

		ModumateUnitParams::FThickness defaultThickness = ModumateUnitParams::FThickness::Points(0.3f);
		FMColor defaultColor = FMColor::Gray64;
		FMColor swingColor = FMColor::Gray160;
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
							TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
								FModumateUnitCoord2D::WorldCentimeters(clippedStart),
								FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
								defaultThickness, swingColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemOperatorLine);

							auto arcAngle = FMath::Atan2(planDelta.Y, planDelta.X);

							if (bPositiveSwing)
							{
								arcAngle -= FModumateUnitValue::Degrees(defaultDoorOpeningDegrees).AsRadians();
							}
							TSharedPtr<FDraftingArc> doorArc = MakeShared<FDraftingArc>(
								ModumateUnitParams::FLength::WorldCentimeters(panelLength),
								ModumateUnitParams::FRadius::Degrees(defaultDoorOpeningDegrees),
								defaultThickness, swingColor);
							doorArc->SetLocalPosition(FModumateUnitCoord2D::WorldCentimeters(planStart));
							doorArc->SetLocalOrientation(FModumateUnitValue::Radians(arcAngle));
							ParentPage->Children.Add(doorArc);
							doorArc->SetLayerTypeRecursive(FModumateLayerType::kOpeningSystemOperatorLine);
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

bool AMOIPortal::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.Add(CachedQuantities);
	return true;
}

void AMOIPortal::UpdateQuantities()
{
	const FBIMAssemblySpec& assembly = CachedAssembly;
	auto assemblyGuid = assembly.UniqueKey();
	const FGraph3D& graph = Document->GetVolumeGraph();
	const FGraph3DFace* hostingFace = graph.FindFace(GetParentID());

	if (!hostingFace)
	{
		return;
	}

	CachedQuantities.Empty();
	float width = (hostingFace->Cached2DPositions[2] - hostingFace->Cached2DPositions[1]).Size();
	float height = (hostingFace->Cached2DPositions[1] - hostingFace->Cached2DPositions[0]).Size();
	int32 namingWidth = FMath::RoundToInt(width * UModumateDimensionStatics::CentimetersToInches);
	int32 namingHeight = FMath::RoundToInt(height * UModumateDimensionStatics::CentimetersToInches);
	FString name = FString::FromInt(namingWidth) + TEXT("x") + FString::FromInt(namingHeight);

	CachedQuantities.AddPartsQuantity(name, assembly.Parts, assemblyGuid);
	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}

void AMOIPortal::PreDestroy()
{
	MarkEdgesMiterDirty();
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
