// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/CabinetFrontFaceHandle.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Objects/Cabinet.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameState_CPP.h"


const float ACabinetFrontFaceHandle::BaseCenterOffset = 5.0f;

bool ACabinetFrontFaceHandle::BeginUse()
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	auto delta = MakeShared<FMOIDelta>();
	auto& modifiedStateData = delta->AddMutationState(TargetMOI);

	FMOICabinetData modifiedCustomData;
	if (ensure(modifiedStateData.CustomData.LoadStructData(modifiedCustomData)))
	{
		if (modifiedCustomData.FrontFaceIndex == TargetIndex)
		{
			modifiedCustomData.FrontFaceIndex = INDEX_NONE;
		}
		else
		{
			modifiedCustomData.FrontFaceIndex = TargetIndex;
		}

		modifiedStateData.CustomData.SaveStructData(modifiedCustomData);
		GameState->Document.ApplyDeltas({ delta }, GetWorld());
	}

	return false;
}

FVector ACabinetFrontFaceHandle::GetHandlePosition() const
{
	const FModumateObjectInstance* cabinetParent = TargetMOI ? TargetMOI->GetParentObject() : nullptr;
	if (!ensure(cabinetParent && (cabinetParent->GetObjectType() == EObjectType::OTSurfacePolygon) && (TargetIndex != INDEX_NONE)))
	{
		return FVector::ZeroVector;
	}

	int32 numBasePoints = cabinetParent->GetNumCorners();
	if (!ensure((numBasePoints > 0) && (TargetIndex != INDEX_NONE) && (TargetIndex <= numBasePoints)))
	{
		return FVector::ZeroVector;
	}

	FVector extrusionDir = TargetMOI->GetNormal();
	FVector extrusionDelta(ForceInitToZero);
	FMOICabinetData cabinetInstanceData;
	if (TargetMOI->GetStateData().CustomData.LoadStructData(cabinetInstanceData))
	{
		extrusionDelta = cabinetInstanceData.ExtrusionDist * extrusionDir;
	}

	if (TargetIndex < numBasePoints)
	{
		FVector edgeCenter = 0.5f * (cabinetParent->GetCorner(TargetIndex) + cabinetParent->GetCorner((TargetIndex + 1) % numBasePoints));
		return edgeCenter + (0.5f * extrusionDelta);
	}
	else
	{
		FVector baseCenter(ForceInitToZero);
		for (int32 cornerIdx = 0; cornerIdx < numBasePoints; ++cornerIdx)
		{
			baseCenter += cabinetParent->GetCorner(cornerIdx);
		}
		baseCenter /= numBasePoints;

		FVector baseAxisX, baseAxisY;
		UModumateGeometryStatics::FindBasisVectors(baseAxisX, baseAxisY, extrusionDir);

		FVector centerOffset = ACabinetFrontFaceHandle::BaseCenterOffset * baseAxisY;

		return baseCenter + extrusionDelta + centerOffset;
	}
}

FVector ACabinetFrontFaceHandle::GetHandleDirection() const
{
	return FVector::ZeroVector;
}

bool ACabinetFrontFaceHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D& OutWidgetSize, FVector2D& OutMainButtonOffset) const
{
	FMOICabinetData cabinetInstanceData;
	if (!TargetMOI->GetStateData().CustomData.LoadStructData(cabinetInstanceData))
	{
		return false;
	}

	OutButtonStyle = (cabinetInstanceData.FrontFaceIndex == INDEX_NONE) ? PlayerHUD->HandleAssets->RoofCreateFacesStyle : PlayerHUD->HandleAssets->RoofRetractFacesStyle;
	OutWidgetSize = FVector2D(24.0f, 24.0f);
	return true;
}
