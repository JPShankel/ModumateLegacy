#include "UI/CabinetDimensionActor.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Objects/Cabinet.h"
#include "UnrealClasses/DimensionWidget.h"

ACabinetDimensionActor::ACabinetDimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACabinetDimensionActor::BeginPlay()
{
	Super::BeginPlay();

	DimensionText->Measurement->OnTextCommitted.AddDynamic(this, &ACabinetDimensionActor::OnMeasurementTextCommitted);
	DimensionText->Measurement->SetIsReadOnly(false);
}

void ACabinetDimensionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DimensionText->Measurement->OnTextCommitted.RemoveDynamic(this, &ACabinetDimensionActor::OnMeasurementTextCommitted);

	Super::EndPlay(EndPlayReason);
}

void ACabinetDimensionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto cabinet = Cast<AMOICabinet>(Document->GetObjectById(CabinetID));
	if (!cabinet)
	{
		return;
	}

	FVector startPosition = cabinet->GetCorner(0);

	FVector endPosition = startPosition + cabinet->GetNormal() * cabinet->InstanceData.ExtrusionDist;
	FVector midpoint = (startPosition + endPosition) / 2.0f;

	auto controller = DimensionText->GetOwningPlayer();

	FVector2D targetScreenPosition;
	if (controller && controller->ProjectWorldLocationToScreen(midpoint, targetScreenPosition))
	{
		// Calculate angle/offset
		// if the target object is a face, determine the offset direction from the cached edge normals 
		// so that the measurement doesn't overlap with the handles
		FVector2D projStart, projEnd, edgeDirection, offsetDirection;
		controller->ProjectWorldLocationToScreen(startPosition, projStart);
		controller->ProjectWorldLocationToScreen(endPosition, projEnd);

		edgeDirection = projEnd - projStart;
		edgeDirection.Normalize();

		// rotate by 90 degrees in 2D
		offsetDirection = FVector2D(edgeDirection.Y, -edgeDirection.X);

		if (DimensionText != nullptr)
		{
			float length = (endPosition - startPosition).Size();
			DimensionText->UpdateLengthTransform(targetScreenPosition, edgeDirection, offsetDirection, length);
		}
	}
}

void ACabinetDimensionActor::OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		const FDocumentSettings& settings = Document->GetCurrentSettings();

		float lengthValue = UModumateDimensionStatics::StringToSettingsFormattedDimension(Text.ToString(), settings).Centimeters;

		if (lengthValue > 0.0f)
		{
			auto moi = Document->GetObjectById(CabinetID);
			auto originalState = moi->GetStateData();
			auto modifiedState = originalState;

			auto delta = MakeShared<FMOIDelta>();
			FMOICabinetData modifiedCabinetData;
			if (ensure(modifiedState.CustomData.LoadStructData(modifiedCabinetData)))
			{
				modifiedCabinetData.ExtrusionDist = lengthValue;
				modifiedState.CustomData.SaveStructData(modifiedCabinetData);
			}
			delta->AddMutationState(moi, originalState, modifiedState);

			Document->ApplyDeltas({ delta }, GetWorld());
		}
	}
}
