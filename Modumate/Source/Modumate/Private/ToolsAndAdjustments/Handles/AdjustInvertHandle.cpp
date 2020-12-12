#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

const float AAdjustInvertHandle::MaxScreenDist = 2000.0f;

bool AAdjustInvertHandle::BeginUse()
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	FMOIStateData invertedState;
	if (TargetMOI->GetInvertedState(invertedState))
	{
		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(TargetMOI, TargetMOI->GetStateData(), invertedState);
		GameState->Document->ApplyDeltas({ delta }, GetWorld());
	}

	return false;
}

FVector AAdjustInvertHandle::GetHandlePosition() const
{
	if (!ensure(Controller && TargetMOI))
	{
		return FVector::ZeroVector;
	}

	FVector objectPos = TargetMOI->GetLocation();
	FVector objectNormal = TargetMOI->GetNormal();
	FVector attachDirection(ForceInitToZero);
	float faceExtent = 0.0f;

	auto parent = TargetMOI->GetParentObject();
	if (parent)
	{
		attachDirection = -parent->GetRotation().GetAxisY();
		int32 numCorners = parent->GetNumCorners();
		for (int32 c = 0; c < numCorners; ++c)
		{
			faceExtent = FMath::Max(faceExtent, (parent->GetCorner(c) - objectPos) | attachDirection);
		}
	}
	
	float desiredWorldDist = 0.5f * faceExtent;

	FVector attachPosWorld;
	FVector2D attachPosScreen;
	if (Controller->GetScreenScaledDelta(objectPos, attachDirection, desiredWorldDist, MaxScreenDist, attachPosWorld, attachPosScreen))
	{
		return attachPosWorld;
	}

	return FVector::ZeroVector;
}

FVector AAdjustInvertHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? TargetMOI->GetNormal() : FVector::ZeroVector;
}

bool AAdjustInvertHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);

	return true;
}
