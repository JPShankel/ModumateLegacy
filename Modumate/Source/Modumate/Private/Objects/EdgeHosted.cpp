// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeHosted.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"
#include "UI/Properties/InstPropWidgetRotation.h"
#include "Objects/MetaEdge.h"
#include "Objects/MetaEdgeSpan.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "DrawingDesigner/DrawingDesignerMeshCache.h"
#include "Quantities/QuantitiesManager.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"


FMOIEdgeHostedData::FMOIEdgeHostedData()
{}

AMOIEdgeHosted::AMOIEdgeHosted()
	: AModumateObjectInstance()
	, LineStartPos(ForceInitToZero)
	, LineEndPos(ForceInitToZero)
	, LineDir(ForceInitToZero)
	, LineNormal(ForceInitToZero)
	, LineUp(ForceInitToZero)
{
	FWebMOIProperty prop;

	prop.name = TEXT("FlipSigns");
	prop.type = EWebMOIPropertyType::flip3D;
	prop.displayName = TEXT("Flip");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("OffsetUp");
	prop.type = EWebMOIPropertyType::offset;
	prop.displayName = TEXT("Offset Y");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("OffsetNormal");
	prop.type = EWebMOIPropertyType::offset;
	prop.displayName = TEXT("Offset X");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("Extensions");
	prop.type = EWebMOIPropertyType::extension;
	prop.displayName = TEXT("Extensions");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("Rotation");
	prop.type = EWebMOIPropertyType::text;
	prop.displayName = TEXT("Rotation");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);
}

AActor* AMOIEdgeHosted::CreateActor(const FVector& loc, const FQuat& rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

void AMOIEdgeHosted::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIEdgeHosted::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIEdgeHosted::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Flip"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetFlip>(this, flipPropertyName))
	{
		flipField->RegisterValue(this, EAxisList::XYZ);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOIEdgeHosted::OnInstPropUIChangedFlip);
	}

	static const FString offsetNormalPropertyName(TEXT("Offset X"));
	if (auto offsetNormalField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetNormalPropertyName))
	{
		offsetNormalField->RegisterValue(this, InstanceData.OffsetNormal);
		offsetNormalField->ValueChangedEvent.AddDynamic(this, &AMOIEdgeHosted::OnInstPropUIChangedOffsetNormal);
	}

	static const FString offsetUpPropertyName(TEXT("Offset Y"));
	if (auto offsetUpField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetUpPropertyName))
	{
		offsetUpField->RegisterValue(this, InstanceData.OffsetUp);
		offsetUpField->ValueChangedEvent.AddDynamic(this, &AMOIEdgeHosted::OnInstPropUIChangedOffsetUp);
	}

	static const FString rotationPropertyName(TEXT("Rotation"));
	if (auto rotationField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationPropertyName))
	{
		rotationField->RegisterValue(this, InstanceData.Rotation);
		rotationField->ValueChangedEvent.AddDynamic(this, &AMOIEdgeHosted::OnInstPropUIChangedRotation);
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

	extensionFields[0]->ValueChangedEvent.AddDynamic(this, &AMOIEdgeHosted::OnInstPropUIChangedExtensionStart);
	extensionFields[1]->ValueChangedEvent.AddDynamic(this, &AMOIEdgeHosted::OnInstPropUIChangedExtensionEnd);
}

bool AMOIEdgeHosted::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIEdgeHostedData modifiedInstanceData = InstanceData;

	float curFlipSign = modifiedInstanceData.FlipSigns.GetComponentForAxis(FlipAxis);
	modifiedInstanceData.FlipSigns.SetComponentForAxis(FlipAxis, -curFlipSign);

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

bool AMOIEdgeHosted::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	// Similar to StructureLine
	// Determine which axis is being flip
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
	float targetFlipSign = (targetProfileAxisIdx == 0) ? InstanceData.FlipSigns.X : InstanceData.FlipSigns.Y;

	const FDimensionOffset& curOffset = (targetProfileAxisIdx == 0) ? InstanceData.OffsetNormal : InstanceData.OffsetUp;
	EDimensionOffsetType nextOffsetType = curOffset.GetNextType(targetAdjustmentSign, targetFlipSign);

	FMOIEdgeHostedData modifiedInstanceData = InstanceData;
	FDimensionOffset& nextOffset = (targetProfileAxisIdx == 0) ? modifiedInstanceData.OffsetNormal : modifiedInstanceData.OffsetUp;
	nextOffset.type = nextOffsetType;
	OutState = GetStateData();

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

void AMOIEdgeHosted::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const
{
	const ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	FVector cmaOrigin, cmaBoxExtent;
	cma->GetActorBounds(false, cmaOrigin, cmaBoxExtent);
	FQuat rot = GetRotation();

	// This calculates the extent more accurately since it's unaffected by actor rotation
	cmaBoxExtent = cma->CalculateComponentsBoundingBoxInLocalSpace(true).GetSize() * 0.5f;

	// TODO: Calculate bounding box with instance data flip 
	FModumateSnappingView::GetBoundingBoxPointsAndLines(cmaOrigin, rot, cmaBoxExtent, outPoints, outLines);
}

void AMOIEdgeHosted::ToggleAndUpdateCapGeometry(bool bEnableCap)
{
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (cma)
	{
		bEnableCap ? cma->SetupCapGeometry() : cma->ClearCapGeometry();
	}
}

void AMOIEdgeHosted::GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane, const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox, TArray<TArray<FVector>>& OutPerimeters) const
{
	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	const ACompoundMeshActor* actor = Cast<ACompoundMeshActor>(GetActor());
	if (bGetFarLines)
	{
		actor->GetFarDraftingLines(ParentPage, Plane, BoundingBox, FModumateLayerType::kPartEdgeBeyond);
	}
	else
	{
		actor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, FModumateLayerType::kPartFaceCut);
	}
}

void AMOIEdgeHosted::GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength /*= 0.0f*/) const
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		TArray<FDrawingDesignerLined> linesDouble;
		FVector localViewDirection(GetWorldTransform().InverseTransformVector(ViewDirection));
		gameInstance->GetMeshCache()->GetDesignerLines(GetAssembly(), CachedScale * InstanceData.FlipSigns, false, localViewDirection,
			linesDouble, MinLength);
		const FMatrix xform(GetWorldTransform().ToMatrixWithScale());
		for (const auto& l : linesDouble)
		{
			FDrawingDesignerLine& newLine = OutDrawingLines.Emplace_GetRef(xform.TransformPosition(FVector(l.P1)),
				xform.TransformPosition(FVector(l.P2)), xform.TransformPosition(FVector(l.N)));
			newLine.Thickness = 0.053f;
		}
	}
}

bool AMOIEdgeHosted::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.Add(CachedQuantities);
	return true;
}

void AMOIEdgeHosted::InternalUpdateGeometry(bool bCreateCollision)
{
	const AModumateObjectInstance* parentObj = GetParentObject();

	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (parentObj && cma)
	{
		LineStartPos = parentObj->GetCorner(0);
		LineEndPos = parentObj->GetCorner(1);
		LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
		LineStartPos -= InstanceData.Extensions.X * LineDir;
		LineEndPos += InstanceData.Extensions.Y * LineDir;

		// Obj dir and rotation
		UModumateGeometryStatics::FindBasisVectors(LineNormal, LineUp, LineDir);
		LineNormal = LineNormal.RotateAngleAxis(InstanceData.Rotation, LineDir);
		LineUp = LineUp.RotateAngleAxis(InstanceData.Rotation, LineDir);
		FQuat cmaRotation = FRotationMatrix::MakeFromXY(LineNormal, LineUp).ToQuat();

		// Obj scale, currently not 9-sliced, scale according to native size to match with edge length
		float lineLength = (LineStartPos - LineEndPos).Size();
		float cmaLength = GetAssembly().GetCompoundAssemblyNativeSize().Z;
		float lengthScale = lineLength / cmaLength;
		FVector cmaScale = FVector(1.f, 1.f, lengthScale);
		CachedScale = cmaScale;

		// Obj pivot at midpoint and offset
		FVector objSize = GetAssembly().GetCompoundAssemblyNativeSize() * cmaScale;
		float cmaOffsetX = InstanceData.OffsetNormal.GetOffsetDistance(InstanceData.FlipSigns.X, objSize.X);
		float cmaOffsetY = InstanceData.OffsetUp.GetOffsetDistance(InstanceData.FlipSigns.Y, objSize.Y);
		FVector cmaLocation = (LineStartPos + LineEndPos) / 2.f;
		cmaLocation += (cmaOffsetX * LineNormal);
		cmaLocation += (cmaOffsetY * LineUp);

		if (GetAssembly().Parts.Num() > 0)
		{
			cma->MakeFromAssemblyPartAsync(FAssetRequest(GetAssembly(), nullptr), 0, cmaScale, false, bCreateCollision);
			FTransform cmaTransform;
			cmaTransform.SetLocation(cmaLocation);
			cmaTransform.SetRotation(cmaRotation);
			cmaTransform.SetScale3D(InstanceData.FlipSigns);
			cma->SetActorTransform(cmaTransform);
		}
	}

	Document->DirtyAllCutPlanes();
}

void AMOIEdgeHosted::OnInstPropUIChangedExtension(float NewValue, int32 ExtensionIdx)
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

void AMOIEdgeHosted::OnInstPropUIChangedFlip(int32 FlippedAxisInt)
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

void AMOIEdgeHosted::OnInstPropUIChangedOffsetUp(const FDimensionOffset& NewValue)
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

void AMOIEdgeHosted::OnInstPropUIChangedOffsetNormal(const FDimensionOffset& NewValue)
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

void AMOIEdgeHosted::OnInstPropUIChangedRotation(float NewValue)
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

void AMOIEdgeHosted::OnInstPropUIChangedExtensionStart(float NewValue)
{
	OnInstPropUIChangedExtension(NewValue, 0);
}

void AMOIEdgeHosted::OnInstPropUIChangedExtensionEnd(float NewValue)
{
	OnInstPropUIChangedExtension(NewValue, 1);
}

void AMOIEdgeHosted::UpdateQuantities()
{
	const FBIMAssemblySpec& assembly = GetAssembly();
	auto assemblyGuid = assembly.UniqueKey();
	const AModumateObjectInstance* parentObject = GetParentObject();
	if (!ensure(parentObject))
	{
		return;
	}

	CachedQuantities.Empty();
	const float length = (parentObject->GetCorner(1) - parentObject->GetCorner(0)).Size();
	CachedQuantities.AddQuantity(assemblyGuid, 1.0f, length);

	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}
