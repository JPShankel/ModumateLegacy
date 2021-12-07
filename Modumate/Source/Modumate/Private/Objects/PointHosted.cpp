// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/PointHosted.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetOffset.h"
#include "UI/Properties/InstPropWidgetFlip.h"
#include "UI/Properties/InstPropWidgetRotation.h"
#include "DocumentManagement/ModumateSnappingView.h"

FMOIPointHostedData::FMOIPointHostedData()
{}

AMOIPointHosted::AMOIPointHosted()
	: AModumateObjectInstance()
{}

AActor* AMOIPointHosted::CreateActor(const FVector& loc, const FQuat& rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

void AMOIPointHosted::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIPointHosted::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(true);
}

void AMOIPointHosted::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	static const FString flipPropertyName(TEXT("Flip"));
	if (auto flipField = PropertiesUI->RequestPropertyField<UInstPropWidgetFlip>(this, flipPropertyName))
	{
		flipField->RegisterValue(this, EAxisList::XYZ);
		flipField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedFlip);
	}

	static const FString offsetXPropertyName(TEXT("Offset X"));
	if (auto offsetXField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetXPropertyName))
	{
		offsetXField->RegisterValue(this, InstanceData.OffsetX);
		offsetXField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedOffsetX);
	}

	static const FString offsetYPropertyName(TEXT("Offset Y"));
	if (auto offsetYField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetYPropertyName))
	{
		offsetYField->RegisterValue(this, InstanceData.OffsetY);
		offsetYField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedOffsetY);
	}

	static const FString offsetZPropertyName(TEXT("Offset Z"));
	if (auto offsetZField = PropertiesUI->RequestPropertyField<UInstPropWidgetOffset>(this, offsetZPropertyName))
	{
		offsetZField->RegisterValue(this, InstanceData.OffsetZ);
		offsetZField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedOffsetZ);
	}

	static const FString rotationXPropertyName(TEXT("Rotation X"));
	if (auto rotationXField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationXPropertyName))
	{
		rotationXField->RegisterValue(this, InstanceData.Rotation.Roll);
		rotationXField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedRotationX);
	}

	static const FString rotationYPropertyName(TEXT("Rotation Y"));
	if (auto rotationYField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationYPropertyName))
	{
		rotationYField->RegisterValue(this, InstanceData.Rotation.Pitch);
		rotationYField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedRotationY);
	}

	static const FString rotationZPropertyName(TEXT("Rotation Z"));
	if (auto rotationZField = PropertiesUI->RequestPropertyField<UInstPropWidgetRotation>(this, rotationZPropertyName))
	{
		rotationZField->RegisterValue(this, InstanceData.Rotation.Yaw);
		rotationZField->ValueChangedEvent.AddDynamic(this, &AMOIPointHosted::OnInstPropUIChangedRotationZ);
	}
}

bool AMOIPointHosted::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	FVector absDir = AdjustmentDirection.GetAbs();
	FDimensionOffset curOffset;
	EDimensionOffsetType nextOffsetType;
	FMOIPointHostedData modifiedInstanceData = InstanceData;

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

void AMOIPointHosted::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping, bool bForSelection) const
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

void AMOIPointHosted::InternalUpdateGeometry(bool bCreateCollision)
{
	const AModumateObjectInstance* parentObj = GetParentObject();
	ACompoundMeshActor* cma = Cast<ACompoundMeshActor>(GetActor());
	if (parentObj && cma)
	{
		FVector parentLocation = parentObj->GetLocation();
		FQuat parentRotation = parentObj->GetRotation();
		FQuat cmaRot = FQuat(InstanceData.Rotation);

		// Offset
		FVector cmaSize = CachedAssembly.GetRiggedAssemblyNativeSize();
		float cmaOffsetX = InstanceData.OffsetX.GetOffsetDistance(InstanceData.FlipSigns.X, cmaSize.X);
		float cmaOffsetY = InstanceData.OffsetY.GetOffsetDistance(InstanceData.FlipSigns.Y, cmaSize.Y);
		float cmaOffsetZ = InstanceData.OffsetZ.GetOffsetDistance(InstanceData.FlipSigns.Z, cmaSize.Z);
		FVector cmaLocation = parentLocation + FVector(cmaOffsetX, cmaOffsetY, cmaOffsetZ);

		if (CachedAssembly.Parts.Num() > 0)
		{
			cma->MakeFromAssembly(CachedAssembly, FVector::OneVector, false, bCreateCollision);
			FTransform cmaTransform;
			cmaTransform.SetLocation(cmaLocation);
			cmaTransform.SetRotation(cmaRot);
			cmaTransform.SetScale3D(InstanceData.FlipSigns);
			cma->SetActorTransform(cmaTransform);
		}
	}
}

void AMOIPointHosted::OnInstPropUIChangedFlip(int32 FlippedAxisInt)
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

void AMOIPointHosted::OnInstPropUIChangedOffsetX(const FDimensionOffset& NewValue)
{
	if (Document && InstanceData.OffsetX != NewValue)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.OffsetX = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIPointHosted::OnInstPropUIChangedOffsetY(const FDimensionOffset& NewValue)
{
	if (Document && InstanceData.OffsetY != NewValue)
	{
		auto deltaPtr = MakeShared<FMOIDelta>();
		auto& newStateData = deltaPtr->AddMutationState(this);
		auto newInstanceData = InstanceData;
		newInstanceData.OffsetY = NewValue;
		newStateData.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOIPointHosted::OnInstPropUIChangedOffsetZ(const FDimensionOffset& NewValue)
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

void AMOIPointHosted::OnInstPropUIChangedRotationX(float NewValue)
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

void AMOIPointHosted::OnInstPropUIChangedRotationY(float NewValue)
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

void AMOIPointHosted::OnInstPropUIChangedRotationZ(float NewValue)
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
