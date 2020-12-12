#include "ToolsAndAdjustments/Handles/JustificationHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "Objects/PlaneHostedObj.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

const float AJustificationHandle::DesiredWorldDist = 30.0f;
const float AJustificationHandle::MaxScreenDist = 50.0f;

bool AJustificationHandle::BeginUse()
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	if ((HandleChildren.Num() > 0) && !bRootExpanded)
	{
		// If we're expanding the root justification handle, then no operations will happen on the target MOI,
		// so BeginUse and End/AbortUse aren't necessary.
		for (AAdjustmentHandleActor *handleChild : HandleChildren)
		{
			if (handleChild)
			{
				handleChild->SetEnabled(true);
			}
		}

		bRootExpanded = true;
		ApplyWidgetStyle();
	}
	else
	{
		// If this handle is performing an operation on the target MOI,
		// then just create a delta and apply it, without using BeginUse or End/AbortUse,
		// to avoid unnecessary state changes like handle visibility.
		auto delta = MakeShared<FMOIDelta>();
		auto& modifiedStateData = delta->AddMutationState(TargetMOI);

		FMOIPlaneHostedObjData modifiedCustomData;
		if (ensure(modifiedStateData.CustomData.LoadStructData(modifiedCustomData)))
		{
			modifiedCustomData.Justification = JustificationValue;
			modifiedStateData.CustomData.SaveStructData(modifiedCustomData);
		}

		GameState->Document->ApplyDeltas({ delta }, GetWorld());
	}

	return false;
}

void AJustificationHandle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// TODO: replace with line actors, rather than immediate-mode HUD lines
	if (bEnabled && (HandleParent != nullptr))
	{
		FVector handleDirection = TargetMOI->GetNormal();
		FVector attachLocation = GetHandlePosition();

		// Draw line from attachLocation to center of ParentMOI
		AModumateObjectInstance* parentMetaplane = TargetMOI->GetParentObject();
		if (parentMetaplane != nullptr && parentMetaplane->GetObjectType() == EObjectType::OTMetaPlane)
		{
			FVector metaPlaneMidpoint = parentMetaplane->GetLocation();
			if (!metaPlaneMidpoint.Equals(attachLocation))
			{
				// Make new line
				FModumateLines newLine;
				newLine.Point1 = metaPlaneMidpoint;
				newLine.Point2 = attachLocation;
				newLine.Color = FLinearColor::White;
				newLine.Thickness = 2.f;
				Controller->HUDDrawWidget->LinesToDraw.Add(newLine);
			}
		}
	}
}

FVector AJustificationHandle::GetHandlePosition() const
{
	if (!ensure(Controller && TargetMOI))
	{
		return FVector::ZeroVector;
	}

	FVector objectPos = TargetMOI->GetLocation();
	FVector objectNormalWorld = TargetMOI->GetNormal();

	FVector worldP0, worldP1;
	FVector2D screenP0, screenP1;
	if (Controller->GetScreenScaledDelta(objectPos, objectNormalWorld, -DesiredWorldDist, MaxScreenDist, worldP0, screenP0) &&
		Controller->GetScreenScaledDelta(objectPos, objectNormalWorld, DesiredWorldDist, MaxScreenDist, worldP1, screenP1))
	{
		return FMath::Lerp(worldP0, worldP1, JustificationValue);
	}

	return FVector::ZeroVector;
}

FVector AJustificationHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? TargetMOI->GetNormal() : FVector::ZeroVector;
}

void AJustificationHandle::SetJustification(float InJustificationValue)
{
	JustificationValue = InJustificationValue;
}

void AJustificationHandle::SetEnabled(bool bNewEnabled)
{
	Super::SetEnabled(bNewEnabled);

	if (bNewEnabled)
	{
		ApplyWidgetStyle();
	}
	else
	{
		bRootExpanded = false;
	}
}

bool AJustificationHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	if ((HandleChildren.Num() > 0) && !bRootExpanded)
	{
		OutButtonStyle = PlayerHUD->HandleAssets->JustificationRootStyle;
		OutWidgetSize = FVector2D(32.0f, 32.0f);
	}
	else
	{
		OutButtonStyle = PlayerHUD->HandleAssets->GenericPointStyle;
		OutWidgetSize = FVector2D(12.0f, 12.0f);
	}

	return true;
}
