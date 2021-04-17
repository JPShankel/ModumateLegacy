// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/StructureLine.h"

#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/Properties/InstPropWidgetRotation.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Quantities/QuantitiesManager.h"


FMOIStructureLineData::FMOIStructureLineData()
{
}

FMOIStructureLineData::FMOIStructureLineData(int32 InVersion)
	: Version(InVersion)
{
}


AMOIStructureLine::AMOIStructureLine()
	: AModumateObjectInstance()
	, LineStartPos(ForceInitToZero)
	, LineEndPos(ForceInitToZero)
	, LineDir(ForceInitToZero)
	, LineNormal(ForceInitToZero)
	, LineUp(ForceInitToZero)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
	, CachedProfileExtents(ForceInitToZero)
{
}

FQuat AMOIStructureLine::GetRotation() const
{
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (parentObj)
	{
		return parentObj->GetRotation();
	}

	return FQuat::Identity;
}

FVector AMOIStructureLine::GetLocation() const
{
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (parentObj)
	{
		return parentObj->GetLocation();
	}

	return FVector::ZeroVector;
}

void AMOIStructureLine::SetupDynamicGeometry()
{
	UpdateCachedGeometry(true, true);
}

void AMOIStructureLine::UpdateDynamicGeometry()
{
	UpdateCachedGeometry(false, true);
}

bool AMOIStructureLine::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIStructureLineData modifiedInstanceData = InstanceData;

	float curFlipSign = modifiedInstanceData.FlipSigns.GetComponentForAxis(FlipAxis);
	modifiedInstanceData.FlipSigns.SetComponentForAxis(FlipAxis, -curFlipSign);

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

bool AMOIStructureLine::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	FVector2D projectedAdjustments(
		AdjustmentDirection | LineNormal,
		AdjustmentDirection | LineUp
	);
	int32 targetProfileAxisIdx = (FMath::Abs(projectedAdjustments.X) > FMath::Abs(projectedAdjustments.Y)) ? 0 : 1;
	float projectedAdjustment = projectedAdjustments[targetProfileAxisIdx];
	if (FMath::IsNearlyZero(projectedAdjustment, THRESH_NORMALS_ARE_ORTHOGONAL))
	{
		projectedAdjustment = 0.0f;
	}

	float targetAdjustmentSign = FMath::Sign(projectedAdjustment);
	float targetFlipSign = ProfileFlip[targetProfileAxisIdx];

	const FDimensionOffset& curOffset = (targetProfileAxisIdx == 0) ? InstanceData.OffsetNormal : InstanceData.OffsetUp;
	EDimensionOffsetType nextOffsetType = curOffset.GetNextType(targetAdjustmentSign, targetFlipSign);

	FMOIStructureLineData modifiedInstanceData = InstanceData;
	FDimensionOffset& nextOffset = (targetProfileAxisIdx == 0) ? modifiedInstanceData.OffsetNormal : modifiedInstanceData.OffsetUp;
	nextOffset.Type = nextOffsetType;
	OutState = GetStateData();

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

void AMOIStructureLine::RegisterInstanceDataUI(UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Flip"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetFlip>(this, flipPropertyName))
	{
		flipField->RegisterValue(this, EAxisList::XYZ);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOIStructureLine::OnInstPropUIChangedFlip);
	}

	static const FString offsetNormalPropertyName(TEXT("Offset X"));
	if (auto offsetNormalField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetNormalPropertyName))
	{
		offsetNormalField->RegisterValue(this, InstanceData.OffsetNormal);
		offsetNormalField->ValueChangedEvent.AddDynamic(this, &AMOIStructureLine::OnInstPropUIChangedOffsetNormal);
	}

	static const FString offsetUpPropertyName(TEXT("Offset Y"));
	if (auto offsetUpField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetUpPropertyName))
	{
		offsetUpField->RegisterValue(this, InstanceData.OffsetUp);
		offsetUpField->ValueChangedEvent.AddDynamic(this, &AMOIStructureLine::OnInstPropUIChangedOffsetUp);
	}

	static const FString rotationPropertyName(TEXT("Rotation"));
	if (auto rotationField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationPropertyName))
	{
		rotationField->RegisterValue(this, InstanceData.Rotation);
		rotationField->ValueChangedEvent.AddDynamic(this, &AMOIStructureLine::OnInstPropUIChangedRotation);
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

	extensionFields[0]->ValueChangedEvent.AddDynamic(this, &AMOIStructureLine::OnInstPropUIChangedExtensionStart);
	extensionFields[1]->ValueChangedEvent.AddDynamic(this, &AMOIStructureLine::OnInstPropUIChangedExtensionEnd);
}

void AMOIStructureLine::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	// For snapping, we want to attach to the underlying line itself
	if (bForSnapping)
	{
		outPoints.Add(FStructurePoint(LineStartPos, FVector::ZeroVector, 0));
		outPoints.Add(FStructurePoint(LineEndPos, FVector::ZeroVector, 1));

		outLines.Add(FStructureLine(LineStartPos, LineEndPos, 0, 1));
	}
	// Otherwise, we want the extents of the mesh
	else
	{
		FVector2D profileSize = CachedProfileExtents.GetSize();
		FVector2D profileCenter = CachedProfileExtents.GetCenter();
		FVector lineCenter = 0.5f * (LineStartPos + LineEndPos) + (profileCenter.X * LineNormal) + (profileCenter.Y * LineUp);
		FVector boxExtents(profileSize.X, FVector::Dist(LineStartPos, LineEndPos), profileSize.Y);
		FQuat boxRot = FRotationMatrix::MakeFromXZ(LineNormal, LineUp).ToQuat();

		FModumateSnappingView::GetBoundingBoxPointsAndLines(lineCenter, boxRot, 0.5f * boxExtents, outPoints, outLines);
	}
}

void AMOIStructureLine::PostLoadInstanceData()
{
	bool bFixedInstanceData = false;

	if (InstanceData.Version < InstanceData.CurrentVersion)
	{
		if (InstanceData.Version < 1)
		{
			for (int32 justificationAxisIdx = 0; justificationAxisIdx < 2; ++justificationAxisIdx)
			{
				float flipValue = InstanceData.FlipSigns[justificationAxisIdx == 0 ? 2 : 0];
				float oldJustification = InstanceData.Justification_DEPRECATED[justificationAxisIdx];
				float flippedJustification = (flipValue * (oldJustification - 0.5f)) + 0.5f;
				FDimensionOffset& offsetValue = (justificationAxisIdx == 0) ? InstanceData.OffsetNormal : InstanceData.OffsetUp;

				if (FMath::IsNearlyEqual(flippedJustification, 0.0f))
				{
					offsetValue.Type = EDimensionOffsetType::Negative;
				}
				else if (FMath::IsNearlyEqual(flippedJustification, 1.0f))
				{
					offsetValue.Type = EDimensionOffsetType::Positive;
				}
				else
				{
					offsetValue.Type = EDimensionOffsetType::Centered;
				}
			}
		}

		if (InstanceData.Version < 2)
		{
			Swap(InstanceData.FlipSigns.Y, InstanceData.FlipSigns.Z);
		}

		// In order to support flat (direction in XY) linear extrusions with a basis Y axis in Z+, we needed to negate the computed basis axes.
		// This means that old linear extrusions offsets need to be negated.
		if (InstanceData.Version < 3)
		{
			InstanceData.OffsetNormal.Invert();
			InstanceData.OffsetUp.Invert();
		}

		InstanceData.Version = InstanceData.CurrentVersion;
		bFixedInstanceData = true;
	}

	if (bFixedInstanceData)
	{
		StateData.CustomData.SaveStructData(InstanceData);
	}
}

bool AMOIStructureLine::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.Add(CachedQuantities);
	return true;
}

void AMOIStructureLine::UpdateQuantities()
{
	const FBIMAssemblySpec& assembly = CachedAssembly;
	auto assemblyGuid = assembly.UniqueKey();
	const Modumate::FGraph3D& graph = Document->GetVolumeGraph();
	const Modumate::FGraph3DEdge* hostingEdge = graph.FindEdge(GetParentID());
	if (!hostingEdge)
	{
		return;
	}

	CachedQuantities.Empty();
	const Modumate::FGraph3DVertex* startVertex = graph.FindVertex(hostingEdge->StartVertexID);
	const Modumate::FGraph3DVertex* endVertex = graph.FindVertex(hostingEdge->EndVertexID);
	float length = (startVertex->Position - endVertex->Position).Size();
	CachedQuantities.AddQuantity(assemblyGuid, 1.0f, length);

	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}

void AMOIStructureLine::OnInstPropUIChangedFlip(int32 FlippedAxisInt)
{
	EAxis::Type flippedAxis = static_cast<EAxis::Type>(FlippedAxisInt);
	if (Document)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.FlipSigns.SetComponentForAxis(flippedAxis, -newInstanceData.FlipSigns.GetComponentForAxis(flippedAxis));
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIStructureLine::OnInstPropUIChangedOffsetUp(const FDimensionOffset& NewValue)
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

void AMOIStructureLine::OnInstPropUIChangedOffsetNormal(const FDimensionOffset& NewValue)
{
	if (Document && (InstanceData.OffsetNormal != NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.OffsetNormal = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIStructureLine::OnInstPropUIChangedRotation(float NewValue)
{
	if (Document && (InstanceData.Rotation != NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Rotation = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIStructureLine::OnInstPropUIChangedExtension(float NewValue, int32 ExtensionIdx)
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

void AMOIStructureLine::OnInstPropUIChangedExtensionStart(float NewValue)
{
	OnInstPropUIChangedExtension(NewValue, 0);
}

void AMOIStructureLine::OnInstPropUIChangedExtensionEnd(float NewValue)
{
	OnInstPropUIChangedExtension(NewValue, 1);
}

bool AMOIStructureLine::UpdateCachedGeometry(bool bRecreate, bool bCreateCollision)
{
	const FSimplePolygon* profile = nullptr;
	if (!ensure(DynamicMeshActor.IsValid() && UModumateObjectStatics::GetPolygonProfile(&CachedAssembly, profile)))
	{
		return false;
	}

	// This can be an expected error, if the object is still getting set up before it has a parent assigned.
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (parentObj == nullptr)
	{
		return false;
	}

	LineStartPos = parentObj->GetCorner(0);
	LineEndPos = parentObj->GetCorner(1);
	LineDir = (LineEndPos - LineStartPos).GetSafeNormal();

	// FindBasisVectors is intended to generate Y-axes that align with -Z, if the input Z-axis is on the XY-axis.
	// StructureLines are choosing their basis vectors arbitrarily, but we know that they would prefer their Y-axis to align with +Z if their extrusion doesn't already align with the Z axis.
	UModumateGeometryStatics::FindBasisVectors(LineNormal, LineUp, LineDir);
	LineNormal *= -1.0f;
	LineUp *= -1.0f;

	if (InstanceData.Rotation != 0.0f)
	{
		FQuat rotation(InstanceData.FlipSigns.Z * LineDir, FMath::DegreesToRadians(InstanceData.Rotation));
		LineNormal = rotation * LineNormal;
		LineUp = rotation * LineUp;
	}

	ProfileFlip.Set(InstanceData.FlipSigns.X, InstanceData.FlipSigns.Y);

	if (!UModumateObjectStatics::GetExtrusionProfilePoints(CachedAssembly, InstanceData.OffsetNormal, InstanceData.OffsetUp, ProfileFlip, CachedProfilePoints, CachedProfileExtents))
	{
		return false;
	}

	DynamicMeshActor->SetupExtrudedPolyGeometry(CachedAssembly, LineStartPos, LineEndPos, LineNormal, LineUp,
		InstanceData.OffsetNormal, InstanceData.OffsetUp, InstanceData.Extensions, InstanceData.FlipSigns, bRecreate, bCreateCollision);

	return true;
}

void AMOIStructureLine::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	OutPerimeters.Reset();

	TArray<FVector> perimeter;
	if (!UModumateObjectStatics::GetExtrusionObjectPoints(CachedAssembly, LineNormal, LineUp,
		InstanceData.OffsetNormal, InstanceData.OffsetUp, ProfileFlip, perimeter))
	{
		return;
	}

	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{   // In cut-plane lines.
		const Modumate::FModumateLayerType layerType =
			GetObjectType() == EObjectType::OTMullion ? Modumate::FModumateLayerType::kMullionCut : Modumate::FModumateLayerType::kBeamColumnCut;
		UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox,
			perimeter, LineStartPos, LineEndPos, layerType);
	}
	else
	{   // Beyond cut-plane lines.
		const Modumate::FModumateLayerType layerType =
			GetObjectType() == EObjectType::OTMullion ? Modumate::FModumateLayerType::kMullionBeyond : Modumate::FModumateLayerType::kBeamColumnBeyond;
		TArray<FEdge> beyondLines = UModumateObjectStatics::GetExtrusionBeyondLinesFromMesh(Plane, perimeter, LineStartPos, LineEndPos);

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
				TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
					FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
					FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
					ModumateUnitParams::FThickness::Points(bool(clippedLine.Count) ? 0.15f : 0.05f),
					Modumate::FMColor::Gray128);
				ParentPage->Children.Add(line);
				line->SetLayerTypeRecursive(layerType);
			}
		}

	}
}
