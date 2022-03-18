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
#include "DrawingDesigner/DrawingDesignerMeshCache.h"

FMOIFaceHostedData::FMOIFaceHostedData()
{}

AMOIFaceHosted::AMOIFaceHosted()
	: AModumateObjectInstance()
{
	FWebMOIProperty prop;

	prop.Name = TEXT("OffsetZ");
	prop.Type = EWebMOIPropertyType::offset;
	prop.DisplayName = TEXT("Offset");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("FlipSigns");
	prop.Type = EWebMOIPropertyType::flip;
	prop.DisplayName = TEXT("Flip");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("BasisEdge");
	prop.Type = EWebMOIPropertyType::button;
	prop.DisplayName = TEXT("Basis");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

    // TODO: make this a lamba property
	prop.Name = TEXT("NumEdges");
	prop.Type = EWebMOIPropertyType::button;
	prop.DisplayName = TEXT("NumEdges");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.Name, prop);
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
		curOffset = InstanceData.OffsetX;
		nextOffsetType = curOffset.GetNextType(AdjustmentDirection.GetSignVector().X, InstanceData.FlipSigns.X);
		modifiedInstanceData.OffsetX.Type = nextOffsetType;
	}
	else if (absDir.Y > absDir.X && absDir.Y > absDir.Z)
	{
		curOffset = InstanceData.OffsetY;
		nextOffsetType = curOffset.GetNextType(AdjustmentDirection.GetSignVector().Y, InstanceData.FlipSigns.Y);
		modifiedInstanceData.OffsetY.Type = nextOffsetType;
	}
	else if (absDir.Z > absDir.X && absDir.Z > absDir.Y)
	{
		curOffset = InstanceData.OffsetZ;
		nextOffsetType = curOffset.GetNextType(AdjustmentDirection.GetSignVector().Z, InstanceData.FlipSigns.Z);
		modifiedInstanceData.OffsetZ.Type = nextOffsetType;
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
		actor->GetFarDraftingLines(ParentPage, Plane, BoundingBox, FModumateLayerType::kDefault);
	}
	else
	{
		actor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, FModumateLayerType::kDefault);
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
			newLine.Thickness = 0.15f;
			newLine.GreyValue = 144 / 255.0f;
		}
	} 
}

void AMOIFaceHosted::InternalUpdateGeometry(bool bCreateCollision)
{
	const AModumateObjectInstance* parentObj = GetParentObject();
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (parentObj && cma)
	{
		FVector nativeSize = CachedAssembly.GetCompoundAssemblyNativeSize();
		FGraph3D * graph = Document->FindVolumeGraph(parentObj->ID);
		const FGraph3DFace* parentFace = graph->FindFace(parentObj->ID);
		

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
		FVector testCross = FVector::CrossProduct(parentFace->CachedEdgeNormals[InstanceData.BasisEdge], parentFace->CachedPlane.GetNormal());
		testCross.Normalize();

		FVector edgeDir = endVert1->Position - startVert1->Position;
		edgeDir.Normalize();
		FVector zAxis = endVert1->Position - startVert1->Position;;
		bool bEdgeReversed = false;
		if (!testCross.Equals(edgeDir, KINDA_SMALL_NUMBER))
		{
			zAxis = startVert1->Position - endVert1->Position;
			bEdgeReversed = true;
		}
		
		zAxis.Normalize();
		FVector xAxis = parentFace->CachedEdgeNormals[InstanceData.BasisEdge];
		FVector yAxis = FVector::CrossProduct(zAxis, xAxis);
		yAxis.Normalize();
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
		//------ End scale


		//------ Start position

		FVector cmaLocation = FVector::ZeroVector;
		
		FVector2D calculatePosition;
		if (bEdgeReversed)
		{
			calculatePosition = boundsOfRotatedPoints.Max.GetRotated(angle * -1);
		}
		else {
			calculatePosition = boundsOfRotatedPoints.Min.GetRotated(angle * -1);
		}
		
		cmaLocation = parentFace->DeprojectPosition(calculatePosition);
		//------ End position
		
		//apply instance data
		cmaRot *= InstanceData.Rotation.Quaternion();
		FVector cmaSize = CachedAssembly.GetCompoundAssemblyNativeSize() * cmaScale;
		//cmaLocation.X += InstanceData.OffsetX.GetOffsetDistance(InstanceData.FlipSigns.X, cmaSize.X);
		//cmaLocation.Y += InstanceData.OffsetY.GetOffsetDistance(InstanceData.FlipSigns.Y, cmaSize.Y);
		cmaLocation += (InstanceData.OffsetZ.GetOffsetDistance(InstanceData.FlipSigns.Z, cmaSize.Z) * yAxis);

		if (CachedAssembly.Parts.Num() > 0)
		{
			//make compound mesh actor from assembly
			cma->MakeFromAssembly(CachedAssembly, FVector::OneVector, false, bCreateCollision);

			//set CMA transform based on values calculated above
			FTransform cmaTransform;
			cmaTransform.SetRotation(cmaRot);
			cmaTransform.SetLocation(cmaLocation);
			cmaTransform.SetScale3D(cmaScale);
			cma->SetActorTransform(cmaTransform);
		}
	}
}

bool AMOIFaceHosted::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	// We know this tracks perfectly. It would normally be cached, but it's okay to update it directly.
	// TODO: Adapt this to use spans when they are ready
	const AModumateObjectInstance* parentObj = GetParentObject();
	const FGraph3DFace* parentFace = Document->GetVolumeGraph()->FindFace(parentObj->ID);
	InstanceData.NumEdges = parentFace->EdgeIDs.Num();
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

void AMOIFaceHosted::OnInstPropUIChangedCycle(int32 BasisValue)
{
	//TODO: make this instance data
	int32 newValue = InstanceData.BasisEdge + 1;
	const AModumateObjectInstance* parentObj = GetParentObject();
	const FGraph3DFace* parentFace = Document->GetVolumeGraph()->FindFace(parentObj->ID);
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