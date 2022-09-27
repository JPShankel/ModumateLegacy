// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/FaceHosted.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/Properties/InstPropWidgetCycle.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"
#include "UI/Properties/InstPropWidgetRotation.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Objects/MetaPlaneSpan.h"
#include "DrawingDesigner/DrawingDesignerMeshCache.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "ToolsAndAdjustments/Handles/JustificationHandle.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"


FMOIFaceHostedData::FMOIFaceHostedData()
{}

AMOIFaceHosted::AMOIFaceHosted()
	: AModumateObjectInstance()
{
	FWebMOIProperty prop;

	prop.name = TEXT("OffsetZ");
	prop.type = EWebMOIPropertyType::offset;
	prop.displayName = TEXT("Offset");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("FlipSigns");
	prop.type = EWebMOIPropertyType::flip3D;
	prop.displayName = TEXT("Flip");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("BasisEdge");
	prop.type = EWebMOIPropertyType::button;
	prop.displayName = TEXT("Basis");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

    // TODO: make this a lamba property
	prop.name = TEXT("NumEdges");
	prop.type = EWebMOIPropertyType::button;
	prop.displayName = TEXT("NumEdges");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.name, prop);
}

AActor* AMOIFaceHosted::CreateActor(const FVector& loc, const FQuat& rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

void AMOIFaceHosted::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIFaceHosted::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(true);
}



bool AMOIFaceHosted::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	FVector absDir = AdjustmentDirection.GetAbs();
	FDimensionOffset curOffset;
	EDimensionOffsetType nextOffsetType;
	FMOIFaceHostedData modifiedInstanceData = InstanceData;

	if (absDir.X > absDir.Y && absDir.X > absDir.Z)
	{
		curOffset = InstanceData.OffsetZ;
		nextOffsetType = curOffset.GetNextType(AdjustmentDirection.GetSignVector().X, InstanceData.FlipSigns.Z);
		modifiedInstanceData.OffsetZ.type = nextOffsetType;
	}
	else if (absDir.Y > absDir.X && absDir.Y > absDir.Z)
	{
		curOffset = InstanceData.OffsetZ;
		nextOffsetType = curOffset.GetNextType(AdjustmentDirection.GetSignVector().Y * -1, InstanceData.FlipSigns.Z);
		modifiedInstanceData.OffsetZ.type = nextOffsetType;
	}
	else if (absDir.Z > absDir.X && absDir.Z > absDir.Y)
	{
		curOffset = InstanceData.OffsetZ;
		nextOffsetType = curOffset.GetNextType(AdjustmentDirection.GetSignVector().Z, InstanceData.FlipSigns.Z);
		modifiedInstanceData.OffsetZ.type = nextOffsetType;
	}
	else
	{
		return false;
	}

	OutState = GetStateData();
	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

void AMOIFaceHosted::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const
{
	const ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	FVector cmaOrigin, cmaBoxExtent;
	cma->GetActorBounds(false, cmaOrigin, cmaBoxExtent);
	FQuat rot = GetRotation();

	// This calculates the extent more accurately since it's unaffected by actor rotation
	cmaBoxExtent = cma->CalculateComponentsBoundingBoxInLocalSpace(true).GetSize() * 0.5f;
	cmaBoxExtent.X *= cma->GetActorScale().X;
	cmaBoxExtent.Y *= cma->GetActorScale().Y;
	cmaBoxExtent.Z *= cma->GetActorScale().Z;
	// TODO: Calculate bounding box with instance data flip 
	FModumateSnappingView::GetBoundingBoxPointsAndLines(cmaOrigin, rot, cmaBoxExtent, outPoints, outLines);
}

void AMOIFaceHosted::ToggleAndUpdateCapGeometry(bool bEnableCap)
{
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (cma)
	{
		bEnableCap ? cma->SetupCapGeometry() : cma->ClearCapGeometry();
	}
}

void AMOIFaceHosted::GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane, const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox, TArray<TArray<FVector>>& OutPerimeters) const
{
	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	const ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(GetActor());
	if (bGetFarLines)
	{
		actor->GetFarDraftingLines(ParentPage, Plane, BoundingBox, FModumateLayerType::kPartFaceBeyond);
	}
	else
	{
		actor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, FModumateLayerType::kPartFaceCut);
	}
}

void AMOIFaceHosted::GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines,
	float MinLength /*= 0.0f*/) const
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		const ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(GetActor());
		TArray<FDrawingDesignerLined> linesDouble;
		FVector localViewDirection(GetWorldTransform().InverseTransformVector(ViewDirection));
		gameInstance->GetMeshCache()->GetDesignerLines(CachedAssembly, FVector::OneVector, false, localViewDirection, linesDouble, MinLength);
		// Use actor transform to get scale.
		const FMatrix xform(actor->GetTransform().ToMatrixWithScale());
		for (const auto& l : linesDouble)
		{
			FDrawingDesignerLine& newLine = OutDrawingLines.Emplace_GetRef(xform.TransformPosition(FVector(l.P1)),
				xform.TransformPosition(FVector(l.P2)), xform.TransformPosition(FVector(l.N)));
			newLine.Thickness = 0.053f;
		}
	} 
}

bool AMOIFaceHosted::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.Add(CachedQuantities);
	return true;
}

void AMOIFaceHosted::InternalUpdateGeometry(bool bCreateCollision)
{
	const AModumateObjectInstance* parentObj = GetParentObject();
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (parentObj && cma)
	{
		FVector nativeSize = CachedAssembly.GetCompoundAssemblyNativeSize();
		const FGraph3DFace* parentFace = nullptr;

		FGraph3D* graph = nullptr;
		const AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(parentObj);

		if (ensure(span && span->InstanceData.GraphMembers.Num() > 0))
		{
			parentFace = span->GetPerimeterFace();
			graph = Document->GetVolumeGraph(span->GetCachedGraphID());
		}

		if (!ensure(graph && parentFace))
		{
			return;
		}

		//------ Start rot

		//convert 2d vector to 3d vector, assume z is 0
		//do sanity check that selected edge basis index exists
		if (InstanceData.BasisEdge >= parentFace->CachedEdgeNormals.Num())
		{
			InstanceData.BasisEdge = 0;
		}
		/*
		* JS - use the selected edge and the edge normal to create a basis vector transform
		*/

		const FGraph3DEdge* edge1 = graph->FindEdge(parentFace->EdgeIDs[InstanceData.BasisEdge]);
		const FGraph3DVertex* startVert1 = graph->FindVertex(edge1->StartVertexID);
		const FGraph3DVertex* endVert1 = graph->FindVertex(edge1->EndVertexID);

		FVector zAxis = (endVert1->Position - startVert1->Position);
		zAxis.Normalize();

		FVector yAxis = parentFace->CachedPlane.GetNormal();

		FVector xAxis = FVector::CrossProduct(yAxis, zAxis);
		xAxis.Normalize();

		FQuat cmaRot = FBasisVectorMatrix(xAxis, yAxis, zAxis, FVector::ZeroVector).Inverse().ToQuat();
		//------ End rot
		

		//------ Start scale
		
		//project to 2d plane
		FVector2D xAxis2d = FVector2D(1, 0);
		FVector2D edge1StartVert2D = parentFace->ProjectPosition2D(startVert1->Position);
		FVector2D edge1EndVert2D = parentFace->ProjectPosition2D(endVert1->Position);
		FVector2D edge1AxisVert = edge1EndVert2D - edge1StartVert2D;
		edge1AxisVert.Normalize();
		//find angle between edge and plane X
		float angle = FMath::Acos(FVector2D::DotProduct(xAxis2d, edge1AxisVert));
		float cross = FVector2D::CrossProduct(xAxis2d, edge1AxisVert);
		
		angle = FMath::RadiansToDegrees(angle);
		if (cross > 0)
		{
			angle *= -1;
		}

		TArray<FVector2D> planePoints2D = parentFace->Cached2DPositions;
		for (int i = 0; i < planePoints2D.Num(); i++)
		{
			planePoints2D[i] = planePoints2D[i].GetRotated(angle);
		}
		FBox2D boundsOfRotatedPoints = FBox2D(planePoints2D);


		float width = boundsOfRotatedPoints.GetExtent().Y * 2.f;
		float height = boundsOfRotatedPoints.GetExtent().X * 2.f;
		
		FVector cmaScale = FVector::OneVector;

		//ensure no divide by zero and apply scale based on model size and bounding box of points
		if (!FMath::IsNearlyZero(nativeSize.X, KINDA_SMALL_NUMBER))
			cmaScale.X = width / nativeSize.X;

		if (!FMath::IsNearlyZero(nativeSize.Z, KINDA_SMALL_NUMBER))
			cmaScale.Z = height / nativeSize.Z;

		cmaScale.X *= InstanceData.FlipSigns.Z;
		cmaScale.Y *= InstanceData.FlipSigns.Y;
		cmaScale.Z *= InstanceData.FlipSigns.X;
		//------ End scale


		//------ Start position

		FVector cmaLocation = FVector::ZeroVector;
		
		FVector2D calculatePosition = boundsOfRotatedPoints.Min.GetRotated(angle * -1);
		
		cmaLocation = parentFace->DeprojectPosition(calculatePosition);
		//------ End position
		
		//apply instance data
		cmaRot *= InstanceData.Rotation.Quaternion();
		
		FVector cmaSize = CachedAssembly.GetCompoundAssemblyNativeSize() * cmaScale;
		//cmaLocation.X += InstanceData.OffsetX.GetOffsetDistance(InstanceData.FlipSigns.X, cmaSize.X);
		//cmaLocation.Y += InstanceData.OffsetY.GetOffsetDistance(InstanceData.FlipSigns.Y, cmaSize.Y);
		cmaLocation += (InstanceData.OffsetZ.GetOffsetDistance(InstanceData.FlipSigns.Z, cmaSize.Y) * yAxis);

		if (InstanceData.FlipSigns.X < 0)
		{
			cmaLocation.X += nativeSize.Z * FMath::Abs(cmaScale.Z);
		}
		if (InstanceData.FlipSigns.Y < 0)
		{
			cmaLocation.Y += nativeSize.Y * FMath::Abs(cmaScale.Y);
		}
		if (InstanceData.FlipSigns.Z < 0)
		{
			cmaLocation.Z += nativeSize.X * FMath::Abs(cmaScale.X);
		}

		if (CachedAssembly.Parts.Num() > 0)
		{
			//make compound mesh actor from assembly
			cma->MakeFromAssemblyPartAsync(FAssetRequest(CachedAssembly, nullptr), 0, cmaScale, false, bCreateCollision);

			//set CMA transform based on values calculated above
			FTransform cmaTransform;
			cmaTransform.SetRotation(cmaRot);
			cmaTransform.SetLocation(cmaLocation);
			cmaTransform.SetScale3D(FVector::OneVector);
			cma->SetActorTransform(cmaTransform);
		}		
	}
}

void AMOIFaceHosted::SetupAdjustmentHandles(AEditModelPlayerController* Controller)
{
	AModumateObjectInstance* parent = GetParentObject();
	auto parentOT = parent->GetObjectType();
	bool parentCompatible = UModumateObjectStatics::IsValidParentObjectType(parentOT);
	if (!parent || !parentCompatible)
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		// Don't allow adjusting wall corners, since they're more likely to be edited edge-by-edge.
		if (GetObjectType() != EObjectType::OTWallSegment)
		{
			auto cornerHandle = MakeHandle<AAdjustPolyEdgeHandle>();
			cornerHandle->SetTargetIndex(i);
			cornerHandle->SetTargetMOI(parent);
		}

		auto edgeHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetTargetMOI(parent);
	}
}

bool AMOIFaceHosted::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	// We know this tracks perfectly. It would normally be cached, but it's okay to update it directly.
	const AModumateObjectInstance* parentObj = GetParentObject();
	check(parentObj->GetObjectType() == EObjectType::OTMetaPlaneSpan);
	const FGraph3DFace* parentFace = UModumateObjectStatics::GetFaceFromSpanObject(Document, parentObj->ID);
	if (parentFace != nullptr)
	{
		//adding numEdges to the Instance Data to be able to be serialized and communicated to the web side
		InstanceData.NumEdges = parentFace->EdgeIDs.Num();
	}
	return AModumateObjectInstance::CleanObject(DirtyFlag, OutSideEffectDeltas);
}

void AMOIFaceHosted::OnInstPropUIChangedOffsetZ(const FDimensionOffset& NewValue)
{
	if (Document && InstanceData.OffsetZ != NewValue)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.OffsetZ = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIFaceHosted::OnInstPropUIChangedRotationX(float NewValue)
{
	if (Document && !FMath::IsNearlyEqual(InstanceData.Rotation.Roll, NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Rotation.Roll = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIFaceHosted::OnInstPropUIChangedRotationY(float NewValue)
{
	if (Document && !FMath::IsNearlyEqual(InstanceData.Rotation.Pitch, NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Rotation.Pitch = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIFaceHosted::OnInstPropUIChangedRotationZ(float NewValue)
{
	if (Document && !FMath::IsNearlyEqual(InstanceData.Rotation.Yaw, NewValue))
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.Rotation.Yaw = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIFaceHosted::UpdateQuantities()
{
	const FBIMAssemblySpec& assembly = CachedAssembly;
	const int32 numLayers = assembly.Layers.Num();
	auto assemblyGuid = assembly.UniqueKey();
	auto* parentObject = GetParentObject();

	if (!ensure(parentObject))
	{
		return;
	}

	const FGraph3DFace* hostingFace = nullptr;
	switch (parentObject->GetObjectType())
	{
	case EObjectType::OTMetaPlaneSpan:
		hostingFace = UModumateObjectStatics::GetFaceFromSpanObject(Document, parentObject->ID);
		break;

	case EObjectType::OTMetaPlane:
	{
		int32 graph3dID = Document->FindGraph3DByObjID(parentObject->ID);
		hostingFace = Document->GetVolumeGraph(graph3dID)->FindFace(parentObject->ID);
		break;
	}

	default:
		ensureMsgf(false, TEXT("Face-hosted object not hosted by meta-plane or meta-span."));
		break;
	}

	if (!ensure(hostingFace))
	{
		return;
	}

	CachedQuantities.Empty();
	float assemblyArea = hostingFace->CalculateArea();

	CachedQuantities.AddQuantity(assemblyGuid, 1.0f, 0.0f, assemblyArea);
}

void AMOIFaceHosted::OnInstPropUIChangedCycle(int32 BasisValue)
{
	//TODO: make this instance data
	int32 newValue = InstanceData.BasisEdge + 1;
	const AModumateObjectInstance* parentObj = GetParentObject();
	const FGraph3DFace* parentFace = UModumateObjectStatics::GetFaceFromSpanObject(Document, parentObj->ID);
	if (newValue >= parentFace->EdgeIDs.Num())
		newValue = 0;
	if (Document && InstanceData.BasisEdge != newValue)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.BasisEdge = newValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIFaceHosted::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Cycle"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetCycle>(this, flipPropertyName))
	{
		flipField->RegisterValue(this);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOIFaceHosted::OnInstPropUIChangedCycle);
	}

	static const FString offsetZPropertyName(TEXT("Offset Z"));
	if (auto offsetZField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetZPropertyName))
	{
		offsetZField->RegisterValue(this, InstanceData.OffsetZ);
		offsetZField->ValueChangedEvent.AddDynamic(this, &AMOIFaceHosted::OnInstPropUIChangedOffsetZ);
	}

	static const FString rotationXPropertyName(TEXT("Rotation X"));
	if (auto rotationXField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationXPropertyName))
	{
		rotationXField->RegisterValue(this, InstanceData.Rotation.Roll);
		rotationXField->ValueChangedEvent.AddDynamic(this, &AMOIFaceHosted::OnInstPropUIChangedRotationX);
	}

	static const FString rotationYPropertyName(TEXT("Rotation Y"));
	if (auto rotationYField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationYPropertyName))
	{
		rotationYField->RegisterValue(this, InstanceData.Rotation.Pitch);
		rotationYField->ValueChangedEvent.AddDynamic(this, &AMOIFaceHosted::OnInstPropUIChangedRotationY);
	}

	static const FString rotationZPropertyName(TEXT("Rotation Z"));
	if (auto rotationZField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationZPropertyName))
	{
		rotationZField->RegisterValue(this, InstanceData.Rotation.Yaw);
		rotationZField->ValueChangedEvent.AddDynamic(this, &AMOIFaceHosted::OnInstPropUIChangedRotationZ);
	}
}

bool AMOIFaceHosted::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIFaceHostedData modifiedInstanceData = InstanceData;

	float curFlipSign = modifiedInstanceData.FlipSigns.GetComponentForAxis(FlipAxis);
	modifiedInstanceData.FlipSigns.SetComponentForAxis(FlipAxis, -curFlipSign);

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}
