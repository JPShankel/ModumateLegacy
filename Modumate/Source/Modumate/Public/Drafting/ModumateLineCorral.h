// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDwgDraw.h"

// Eliminate or separate overlying line segments.
class MODUMATE_API FModumateLineCorral: public Modumate::IModumateDraftingDraw
{
public:
		virtual Modumate::EDrawError DrawLine(
		const ModumateUnitParams::FXCoord &x1,
		const ModumateUnitParams::FYCoord &y1,
		const ModumateUnitParams::FXCoord &x2,
		const ModumateUnitParams::FYCoord &y2,
		const ModumateUnitParams::FThickness &thickness,
		const Modumate::FMColor &color,
		const Modumate::LinePattern &linePattern,
		const ModumateUnitParams::FPhase &phase,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError AddText(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		const ModumateUnitParams::FXCoord &xpos,
		const ModumateUnitParams::FYCoord &ypos,
		const ModumateUnitParams::FAngle &rotateByRadians,
		const Modumate::FMColor &color,
		Modumate::DraftingAlignment textJustify,
		const ModumateUnitParams::FWidth &containingRectWidth,
		Modumate::FontType type,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError GetTextLength(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		FModumateUnitValue &textLength,
		Modumate::FontType type = Modumate::FontType::Standard) override;

	virtual Modumate::EDrawError DrawArc(
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FAngle &a1,
		const ModumateUnitParams::FAngle &a2,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const Modumate::FMColor &color,
		const Modumate::LinePattern &linePattern,
		int slices,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError AddImage(
		const TCHAR *imageFileFullPath,
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FWidth &width,
		const ModumateUnitParams::FHeight &height,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError FillPoly(
		const float *points,
		int numPoints,
		const Modumate::FMColor &color,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError DrawCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const Modumate::LinePattern &linePattern,
		const Modumate::FMColor &color,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError FillCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const Modumate::FMColor &color,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName = FString()) override;

	virtual bool SaveDocument(const FString& filename) override;

	explicit FModumateLineCorral(IModumateDraftingDraw * nextDrafting = new Modumate::FModumateDwgDraw);

	static int LinePriorities[];

private:
	static const Modumate::FModumateLayerType LayerTypeOrder[];
	static TAtomic<int32> InitPriorities;
	static const TSet<Modumate::FModumateLayerType> PassedThroughTypes;

	const TUniquePtr<IModumateDraftingDraw> Next;

	struct FLineSegment
	{
		FVector2D StartVert;
		FVector2D EndVert;
		FBox2D BoundingBox { ForceInit };
		int32 Index;
		int32 LineData;
		FLineSegment(float x1, float y1, float x2, float y2, int32 index, int32 lineData);
	};

	struct FLineData
	{
		ModumateUnitParams::FThickness Thickness;
		Modumate::FMColor Color;
		Modumate::LinePattern Pattern;
		ModumateUnitParams::FPhase Phase;
		Modumate::FModumateLayerType LayerType;
		FLineData(ModumateUnitParams::FThickness thickness, Modumate::FMColor color, Modumate::LinePattern linePattern, ModumateUnitParams::FPhase phase,
			Modumate::FModumateLayerType layerType);
	};

	using FLineSegments = TArray<FLineSegment>;
	using FLineDataItems = TArray<FLineData>;
	FLineSegments InLines;
	FLineSegments OutLines;
	FLineDataItems LineDataItems;
	int32 CurrentIndex{ 0 };

	void ProcessLines();
	static constexpr double Epsilon = 0.05;	// 0.5 mm
};
