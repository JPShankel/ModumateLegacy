#include "UI/PendingAngleActor.h"

#include "Components/EditableTextBox.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"

APendingAngleActor::APendingAngleActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void APendingAngleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto controller = DimensionText->GetOwningPlayer();

	FVector2D targetScreenPosition, projEnd, offsetDirection;
	if (controller && DimensionText && controller->ProjectWorldLocationToScreen(WorldPosition, targetScreenPosition))
	{
		controller->ProjectWorldLocationToScreen(WorldPosition + WorldDirection, projEnd);
		offsetDirection = projEnd - targetScreenPosition;
		offsetDirection.Normalize();

		DimensionText->UpdateDegreeTransform(targetScreenPosition, offsetDirection, CurrentAngle);
	}
}
