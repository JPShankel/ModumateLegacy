// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Trim.h"

#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "Quantities/QuantitiesManager.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"


FMOITrimData::FMOITrimData()
{
}

FMOITrimData::FMOITrimData(int32 InVersion)
	: Version(InVersion)
{
}


AMOITrim::AMOITrim()
	: AModumateObjectInstance()
	, TrimStartPos(ForceInitToZero)
	, TrimEndPos(ForceInitToZero)
	, TrimNormal(ForceInitToZero)
	, TrimUp(ForceInitToZero)
	, TrimDir(ForceInitToZero)
	, TrimExtrusionFlip(FVector::OneVector)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
	, ProfileOffsetDists(ForceInitToZero)
	, ProfileFlip(ForceInitToZero)
{
}

FVector AMOITrim::GetLocation() const
{
	const AModumateObjectInstance* parentMOI = GetParentObject();
	if (parentMOI)
	{
		return parentMOI->GetLocation();
	}

	return FVector::ZeroVector;
}

FQuat AMOITrim::GetRotation() const
{
	const AModumateObjectInstance *parentMOI = GetParentObject();
	if (parentMOI)
	{
		return parentMOI->GetRotation();
	}

	return FQuat::Identity;
}

FVector AMOITrim::GetNormal() const
{
	return TrimUp;
}

bool AMOITrim::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UpdateCachedStructure())
		{
			return false;
		}

		MarkDirty(EObjectDirtyFlags::Mitering);
		return true;
	}
	case EObjectDirtyFlags::Mitering:
	{
		if (!UpdateMitering())
		{
			return false;
		}

		bool bInPreviewMode = IsInPreviewMode();
		bool bRecreateMesh = !bInPreviewMode;
		bool bCreateCollision = !bInPreviewMode;
		return InternalUpdateGeometry(bRecreateMesh, bCreateCollision);
	}
	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();
	default:
		return true;
	}
}

void AMOITrim::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	// For snapping, we want to attach to the underlying line itself
	if (bForSnapping)
	{
		outPoints.Add(FStructurePoint(TrimStartPos, FVector::ZeroVector, 0));
		outPoints.Add(FStructurePoint(TrimEndPos, FVector::ZeroVector, 1));

		outLines.Add(FStructureLine(TrimStartPos, TrimEndPos, 0, 1));
	}
	// Otherwise, we want the extents of the mesh
	else
	{
		FVector2D profileSize = CachedProfileExtents.GetSize();
		FVector2D profileCenter = CachedProfileExtents.GetCenter();
		FVector lineCenter = 0.5f * (TrimStartPos + TrimEndPos) + (profileCenter.X * TrimNormal) + (profileCenter.Y * TrimUp);
		FVector boxExtents(profileSize.X, FVector::Dist(TrimStartPos, TrimEndPos), profileSize.Y);
		FQuat boxRot = FRotationMatrix::MakeFromXZ(TrimNormal, TrimUp).ToQuat();

		FModumateSnappingView::GetBoundingBoxPointsAndLines(lineCenter, boxRot, 0.5f * boxExtents, outPoints, outLines);
	}
}

void AMOITrim::SetIsDynamic(bool bIsDynamic)
{
	if (DynamicMeshActor.IsValid())
	{
		DynamicMeshActor->SetIsDynamic(bIsDynamic);
	}
}

bool AMOITrim::GetIsDynamic() const
{
	return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
}

bool AMOITrim::GetInvertedState(FMOIStateData& OutState) const
{
	return GetFlippedState(EAxis::Y, OutState);
}

bool AMOITrim::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	if (FlipAxis == EAxis::X)
	{
		return false;
	}

	OutState = GetStateData();

	FMOITrimData modifiedTrimData = InstanceData;
	int32 flipAxisIdx = (FlipAxis == EAxis::Z) ? 0 : 1;
	modifiedTrimData.FlipSigns[flipAxisIdx] *= -1.0f;

	return OutState.CustomData.SaveStructData(modifiedTrimData);
}

bool AMOITrim::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	// For now, we only allow adjusting trim OffsetUp, so we only need to check against one axis.
	float projectedAdjustment = AdjustmentDirection | TrimUp;
	if (FMath::IsNearlyZero(projectedAdjustment, THRESH_NORMALS_ARE_ORTHOGONAL))
	{
		projectedAdjustment = 0.0f;
	}

	float projectedAdjustmentSign = FMath::Sign(projectedAdjustment);
	EDimensionOffsetType nextOffsetType = InstanceData.OffsetUp.GetNextType(projectedAdjustmentSign, InstanceData.FlipSigns.Y);

	FMOITrimData modifiedTrimData = InstanceData;
	modifiedTrimData.OffsetUp.Type = nextOffsetType;
	OutState = GetStateData();

	return OutState.CustomData.SaveStructData(modifiedTrimData);
}

void AMOITrim::RegisterInstanceDataUI(UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Flip"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetFlip>(this, flipPropertyName))
	{
		flipField->RegisterValue(this, EAxisList::YZ);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOITrim::OnInstPropUIChangedFlip);
	}

	static const FString offsetUpPropertyName(TEXT("Offset Y"));
	if (auto offsetUpField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetUpPropertyName))
	{
		offsetUpField->RegisterValue(this, InstanceData.OffsetUp);
		offsetUpField->ValueChangedEvent.AddDynamic(this, &AMOITrim::OnInstPropUIChangedOffsetUp);
	}

	static const FString extensionPropertyNames[] = { FString(TEXT("Start Extension")), FString(TEXT("End Extension")) };
	UInstPropWidgetLinearDimension* extensionFields[] = { nullptr, nullptr };
	for (int32 extensionIdx = 0; extensionIdx < 2; ++extensionIdx)
	{
		const FString& extensionPropertyName = extensionPropertyNames[extensionIdx];
		auto extensionField = PropertiesUI->RequestPropertyField<UInstPropWidgetLinearDimension>(this, extensionPropertyName);
		if (ensure(extensionField))
		{
			extensionField->RegisterValue(this, InstanceData.Extensions[extensionIdx]);
			extensionFields[extensionIdx] = extensionField;
		}
		else
		{
			return;
		}
	}

	extensionFields[0]->ValueChangedEvent.AddDynamic(this, &AMOITrim::OnInstPropUIChangedExtensionStart);
	extensionFields[1]->ValueChangedEvent.AddDynamic(this, &AMOITrim::OnInstPropUIChangedExtensionEnd);
}

void AMOITrim::GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	TArray<FVector> perimeter;
	if (!UModumateObjectStatics::GetExtrusionObjectPoints(CachedAssembly, TrimNormal, TrimUp,
		InstanceData.OffsetNormal, InstanceData.OffsetUp, ProfileFlip, perimeter))
	{
		return;
	}

	if (bGetFarLines)
	{   // Beyond lines.
		TArray<FEdge> beyondLines = UModumateObjectStatics::GetExtrusionBeyondLinesFromMesh(Plane, perimeter, TrimStartPos, TrimEndPos);

		TArray<FEdge> clippedLines;
		for (const auto& edge : beyondLines)
		{
			int32 savedCount = edge.Count;
			auto lines = ParentPage->lineClipping->ClipWorldLineToView(edge);
			// Restore count (silhouette flag).
			Algo::ForEach(lines, [savedCount](FEdge& e) {e.Count = savedCount; });
			clippedLines.Append(MoveTemp(lines));
		}

		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& clippedLine : clippedLines)
		{
			FVector2D vert0(clippedLine.Vertex[0]);
			FVector2D vert1(clippedLine.Vertex[1]);

			if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
			{
				TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
					FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
					FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
					ModumateUnitParams::FThickness::Points(bool(clippedLine.Count) ? 0.15f : 0.05f),
					FMColor::Gray128);
				ParentPage->Children.Add(line);
				line->SetLayerTypeRecursive(FModumateLayerType::kSeparatorBeyondModuleEdges);
			}
		}
	}
	else
	{   // In-plane lines.
	 UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox,
			perimeter, TrimStartPos, TrimEndPos, FModumateLayerType::kSeparatorCutTrim, 0.3f);
	}
}

void AMOITrim::PostLoadInstanceData()
{
	bool bFixedInstanceData = false;

	if (InstanceData.Version < InstanceData.CurrentVersion)
	{
		if (InstanceData.Version < 1)
		{
			if (InstanceData.bUpInverted_DEPRECATED)
			{
				InstanceData.FlipSigns.Y = -1.0f;
			}
		}
		if (InstanceData.Version < 2)
		{
			float flippedJustification = (InstanceData.FlipSigns.Y * (InstanceData.UpJustification_DEPRECATED - 0.5f)) + 0.5f;
			if (FMath::IsNearlyEqual(flippedJustification, 0.0f))
			{
				InstanceData.OffsetUp.Type = EDimensionOffsetType::Negative;
			}
			else if (FMath::IsNearlyEqual(flippedJustification, 1.0f))
			{
				InstanceData.OffsetUp.Type = EDimensionOffsetType::Positive;
			}
			else
			{
				InstanceData.OffsetUp.Type = EDimensionOffsetType::Centered;
			}
		}

		InstanceData.Version = InstanceData.CurrentVersion;
		bFixedInstanceData = true;
	}

	// Check for invalid flip signs regardless of version numbers, due to non-serialization-version bugs
	for (int32 axisIdx = 0; axisIdx < 2; ++axisIdx)
	{
		float& flipSign = InstanceData.FlipSigns[axisIdx];
		if (FMath::Abs(flipSign) != 1.0f)
		{
			flipSign = 1.0f;
			bFixedInstanceData = true;
		}
	}

	if (bFixedInstanceData)
	{
		StateData.CustomData.SaveStructData(InstanceData);
	}
}

void AMOITrim::PreDestroy()
{
	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}

bool AMOITrim::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	const FBIMAssemblySpec& assembly = CachedAssembly;
	auto assemblyGuid = assembly.UniqueKey();

	float trimLength = (TrimEndPos - TrimStartPos).Size();
	QuantitiesVisitor.AddQuantity(assemblyGuid, 1.0f, trimLength);

	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();

	return true;
}

void AMOITrim::OnInstPropUIChangedFlip(int32 FlippedAxisInt)
{
	EAxis::Type flippedAxis = static_cast<EAxis::Type>(FlippedAxisInt);
	if (Document)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		float& newFlipSign = (flippedAxis == EAxis::X) ? newInstanceData.FlipSigns.X : newInstanceData.FlipSigns.Y;
		newFlipSign *= -1.0f;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOITrim::OnInstPropUIChangedOffsetUp(const FDimensionOffset& NewValue)
{
	if (Document && (InstanceData.OffsetUp != NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.OffsetUp = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOITrim::OnInstPropUIChangedExtension(float NewValue, int32 ExtensionIdx)
{
	if (Document && (ExtensionIdx >= 0) && (ExtensionIdx <= 1) && (InstanceData.Extensions[ExtensionIdx] != NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Extensions[ExtensionIdx] = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOITrim::OnInstPropUIChangedExtensionStart(float NewValue)
{
	OnInstPropUIChangedExtension(NewValue, 0);
}

void AMOITrim::OnInstPropUIChangedExtensionEnd(float NewValue)
{
	OnInstPropUIChangedExtension(NewValue, 1);
}

bool AMOITrim::UpdateCachedStructure()
{
	const UModumateDocument* doc = GetDocument();

	// Find the parent surface edge MOI to set up mounting.
	// This can fail gracefully, if the object is still getting set up before it has a parent assigned.
	const AModumateObjectInstance* surfaceEdgeMOI = GetParentObject();
	if ((surfaceEdgeMOI == nullptr) || (surfaceEdgeMOI->GetObjectType() != EObjectType::OTSurfaceEdge) || surfaceEdgeMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	const AModumateObjectInstance* surfaceGraphMOI = surfaceEdgeMOI->GetParentObject();
	if ((surfaceGraphMOI == nullptr) || (surfaceGraphMOI->GetObjectType() != EObjectType::OTSurfaceGraph) || surfaceGraphMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	auto surfaceGraph = doc->FindSurfaceGraph(surfaceGraphMOI->ID);
	auto surfaceEdge = surfaceGraph ? surfaceGraph->FindEdge(surfaceEdgeMOI->ID) : nullptr;
	if (!surfaceGraph.IsValid() || (surfaceEdge == nullptr))
	{
		return false;
	}

	// See if the trim should be offset based on its neighboring polygons
	float maxNeighboringThickness = 0.0f;
	const FGraph2DPolygon *surfacePolyLeft, *surfacePolyRight;
	if (!surfaceEdge->GetAdjacentInteriorPolygons(surfacePolyLeft, surfacePolyRight))
	{
		return false;
	}

	const AModumateObjectInstance* leftPolyMOI = surfacePolyLeft ? doc->GetObjectById(surfacePolyLeft->ID) : nullptr;
	const AModumateObjectInstance* rightPolyMOI = surfacePolyRight ? doc->GetObjectById(surfacePolyRight->ID) : nullptr;
	for (auto neighborPolyMOI : { leftPolyMOI, rightPolyMOI })
	{
		if (neighborPolyMOI && (neighborPolyMOI->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			for (auto neighborPolyChild : neighborPolyMOI->GetChildObjects())
			{
				if (neighborPolyChild->GetObjectType() == EObjectType::OTFinish)
				{
					if (neighborPolyChild->IsDirty(EObjectDirtyFlags::Structure))
					{
						return false;
					}

					float neighborFinishThickness = neighborPolyChild->CalculateThickness();
					maxNeighboringThickness = FMath::Max(maxNeighboringThickness, neighborFinishThickness);
				}
			}
		}
	}

	TrimStartPos = surfaceEdgeMOI->GetCorner(0);
	TrimEndPos = surfaceEdgeMOI->GetCorner(1);
	TrimDir = (TrimEndPos - TrimStartPos).GetSafeNormal();
	TrimNormal = surfaceGraphMOI->GetNormal();

	TrimUp = (TrimDir ^ TrimNormal);
	TrimExtrusionFlip.Set(1.0f, InstanceData.FlipSigns.Y, InstanceData.FlipSigns.X);
	ProfileFlip.Set(1.0f, InstanceData.FlipSigns.Y);

	return UModumateObjectStatics::GetExtrusionProfilePoints(CachedAssembly, InstanceData.OffsetNormal, InstanceData.OffsetUp, ProfileFlip, CachedProfilePoints, CachedProfileExtents);
}

bool AMOITrim::UpdateMitering()
{
	// TODO: miter with neighboring trim, automatically based on graph connectivity and/or based on saved mitering preferences
	UpperExtensions = OuterExtensions = FVector2D::ZeroVector;
	return true;
}

bool AMOITrim::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
{
	return DynamicMeshActor->SetupExtrudedPolyGeometry(CachedAssembly, TrimStartPos, TrimEndPos,
		TrimNormal, TrimUp, InstanceData.OffsetNormal, InstanceData.OffsetUp, InstanceData.Extensions, TrimExtrusionFlip, bRecreate, bCreateCollision);
}

void AMOITrim::UpdateQuantities()
{
	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}
