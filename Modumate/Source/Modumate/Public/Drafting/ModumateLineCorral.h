// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDwgDraw.h"

using namespace Modumate;

// Eliminate or separate overlying line segments.
class MODUMATE_API FModumateLineCorral: public IModumateDraftingDraw
{
public:
		virtual EDrawError DrawLine(
		const Units::FXCoord &x1,
		const Units::FYCoord &y1,
		const Units::FXCoord &x2,
		const Units::FYCoord &y2,
		const Units::FThickness &thickness,
		const FMColor &color,
		const LinePattern &linePattern,
		const Units::FPhase &phase,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual EDrawError AddText(
		const TCHAR *text,
		const Units::FFontSize &fontSize,
		const Units::FXCoord &xpos,
		const Units::FYCoord &ypos,
		const Units::FAngle &rotateByRadians,
		const FMColor &color,
		DraftingAlignment textJustify,
		const Units::FWidth &containingRectWidth,
		FontType type,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual EDrawError GetTextLength(
		const TCHAR *text,
		const Units::FFontSize &fontSize,
		Units::FUnitValue &textLength,
		FontType type = FontType::Standard) override;

	virtual EDrawError DrawArc(
		const Units::FXCoord &x,
		const Units::FYCoord &y,
		const Units::FAngle &a1,
		const Units::FAngle &a2,
		const Units::FRadius &radius,
		const Units::FThickness &lineWidth,
		const FMColor &color,
		const LinePattern &linePattern,
		int slices,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual EDrawError AddImage(
		const TCHAR *imageFileFullPath,
		const Units::FXCoord &x,
		const Units::FYCoord &y,
		const Units::FWidth &width,
		const Units::FHeight &height,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual EDrawError FillPoly(
		const float *points,
		int numPoints,
		const FMColor &color,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual EDrawError DrawCircle(
		const Units::FXCoord &cx,
		const Units::FYCoord &cy,
		const Units::FRadius &radius,
		const Units::FThickness &lineWidth,
		const LinePattern &linePattern,
		const FMColor &color,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual EDrawError FillCircle(
		const Units::FXCoord &cx,
		const Units::FYCoord &cy,
		const Units::FRadius &radius,
		const FMColor &color,
		FModumateLayerType layerType = FModumateLayerType::kDefault
	) override;

	virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches) override;

	virtual bool SaveDocument(const FString& filename) override;

	explicit FModumateLineCorral(IModumateDraftingDraw * nextDrafting = new FModumateDwgDraw);

	static int LinePriorities[];

private:
	static const FModumateLayerType LayerTypeOrder[];
	static TAtomic<int32> InitPriorities;
	static const TSet<FModumateLayerType> PassedThroughTypes;

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
		Units::FThickness Thickness;
		FMColor Color;
		LinePattern Pattern;
		Units::FPhase Phase;
		FModumateLayerType LayerType;
		FLineData(Units::FThickness thickness, FMColor color, LinePattern linePattern, Units::FPhase phase,
			FModumateLayerType layerType);
	};

	using FLineSegments = TArray<FLineSegment>;
	using FLineDataItems = TArray<FLineData>;
	FLineSegments InLines;
	FLineSegments OutLines;
	FLineDataItems LineDataItems;
	int32 CurrentIndex{ 0 };

	void ProcessLines();
	static constexpr float Epsilon = 1.5f;	// World units
};
