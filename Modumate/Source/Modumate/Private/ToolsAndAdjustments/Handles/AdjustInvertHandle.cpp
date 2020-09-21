#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

const float AAdjustInvertHandle::DesiredWorldDist = 25.0f;
const float AAdjustInvertHandle::MaxScreenDist = 30.0f;

bool AAdjustInvertHandle::BeginUse()
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	TargetMOI->BeginPreviewOperation();
	TargetMOI->SetObjectInverted(!TargetMOI->GetObjectInverted());
	auto delta = MakeShareable(new FMOIDelta({ TargetMOI }));
	TargetMOI->EndPreviewOperation();

	GameState->Document.ApplyDeltas({ delta }, GetWorld());
	return false;
}

FVector AAdjustInvertHandle::GetHandlePosition() const
{
	if (!ensure(Controller && TargetMOI))
	{
		return FVector::ZeroVector;
	}

	FVector objectPos = TargetMOI->GetObjectLocation();
	FVector attachNormal = -FVector::UpVector;

	FVector attachPosWorld;
	FVector2D attachPosScreen;
	if (Controller->GetScreenScaledDelta(objectPos, attachNormal, DesiredWorldDist, MaxScreenDist, attachPosWorld, attachPosScreen))
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
