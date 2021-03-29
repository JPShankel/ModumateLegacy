#include "Drafting/DraftingHUDDraw.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	static const FColor dwgColors[int32(FModumateLayerType::kFinalLayerType) + 1] =
	{
		FColor(0, 0, 0),		// kDefault
		FColor(165, 82, 103),	// kSeparatorCutStructuralLayer
		FColor(255, 0, 107),	// kSeparatorCutOuterSurface
		FColor(165, 82, 124),	// kSeparatorCutMinorLayer
		FColor(255, 0, 63),		// kSeparatorBeyondSurfaceEdges
		FColor(165, 0, 41),		// kSeparatorBeyondModuleEdges
		FColor(0, 191, 255),	// kOpeningSystemCutLine
		FColor(0, 124, 165),	// kOpeningSystemBeyond
		FColor(82, 145, 165),	// kOpeningSystemBehind
		FColor(153, 153, 153),	// kOpeningSystemOperatorLine
		FColor(255, 191, 0),	// kSeparatorCutTrim
		FColor(255, 127, 223),	// kCabinetCutCarcass
		FColor(162, 85, 145),	// kCabinetCutAttachment
		FColor(165, 82,	0),		// kCabinetBeyond
		FColor(165, 124, 82),	// kCabinetBehind
		FColor(165, 103, 82),	// kCabinetBeyondBlockedByCountertop
		FColor(255, 223, 127),	// kCountertopCut
		FColor(165, 145, 82),	// kCountertopBeyond
		FColor(223, 127, 255),	// kFfeOutline
		FColor(191, 0, 255),	// kFfeInteriorEdges
		FColor(255, 127, 223),	// kBeamColumnCut
		FColor(162, 85, 145),	// kBeamColumnBeyond
		FColor(0, 191, 255),	// kMullionCut
		FColor(0, 124, 165),	// kMullionBeyond
		FColor(0, 191, 255),	// kSystemPanelCut
		FColor(0, 124, 165),	// kSystemPanelBeyond
		FColor(255, 191, 127),	// kFinishCut
		FColor(165, 124, 82),	// kFinishBeyond
		FColor(255, 0, 255),	// kDebug1
		FColor(0, 0, 255),		// kDebug2
		FColor(127, 0, 31),		// kSeparatorCutEndCaps
	};

	EDrawError FDraftingHUDDraw::DrawLine(
		const ModumateUnitParams::FXCoord &x1,
		const ModumateUnitParams::FYCoord &y1,
		const ModumateUnitParams::FXCoord &x2,
		const ModumateUnitParams::FYCoord &y2,
		const ModumateUnitParams::FThickness &thickness,
		const FMColor &color,
		const LinePattern &linePattern,
		const ModumateUnitParams::FPhase &phase,
		FModumateLayerType layerType)
	{
		FModumateLines line;
		line.Thickness = thickness.AsWorldCentimeters();

		// currently everything in drafting is opaque
		if (bUseDwgMode)
		{
			int32 intLayerType = int32(FMath::Clamp(layerType, FModumateLayerType::kDefault, FModumateLayerType::kFinalLayerType));
			line.Color = dwgColors[intLayerType];
		}
		else
		{
			line.Color = FLinearColor(color.R, color.G, color.B, 1.0f);
		}

		// TODO: dash pattern is unimplemented here (would use linePattern and phase)

		line.Point1 = FVector(CurrentOrigin) + CurrentAxisX * x1.AsWorldCentimeters() + CurrentAxisY * y1.AsWorldCentimeters();
		line.Point2 = FVector(CurrentOrigin) + CurrentAxisX * x2.AsWorldCentimeters() + CurrentAxisY * y2.AsWorldCentimeters();

		HUDDrawWidget->LinesToDraw.Add(MoveTemp(line));

		return EDrawError::ErrorNone;
	}

	EDrawError FDraftingHUDDraw::AddText(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		const ModumateUnitParams::FXCoord &xpos,
		const ModumateUnitParams::FYCoord &ypos,
		const ModumateUnitParams::FAngle &rotateByRadians,
		const FMColor &color,
		DraftingAlignment textJustify,
		const ModumateUnitParams::FWidth &containingRectWidth,
		FontType type,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FDraftingHUDDraw::GetTextLength(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		FModumateUnitValue &textLength,
		FontType type)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FDraftingHUDDraw::DrawArc(
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FAngle &a1,
		const ModumateUnitParams::FAngle &a2,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const FMColor &color,
		const LinePattern &linePattern,
		int slices,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FDraftingHUDDraw::AddImage(
		const TCHAR *imageFileFullPath,
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FWidth &width,
		const ModumateUnitParams::FHeight &height,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FDraftingHUDDraw::FillPoly(
		const float *points,
		int numPoints,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FDraftingHUDDraw::DrawCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const LinePattern &linePattern,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FDraftingHUDDraw::FillCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}
}