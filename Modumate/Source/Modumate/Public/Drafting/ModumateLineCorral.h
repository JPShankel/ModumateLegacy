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
		const Modumate::Units::FXCoord &x1,
		const Modumate::Units::FYCoord &y1,
		const Modumate::Units::FXCoord &x2,
		const Modumate::Units::FYCoord &y2,
		const Modumate::Units::FThickness &thickness,
		const Modumate::FMColor &color,
		const Modumate::LinePattern &linePattern,
		const Modumate::Units::FPhase &phase,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError AddText(
		const TCHAR *text,
		const Modumate::Units::FFontSize &fontSize,
		const Modumate::Units::FXCoord &xpos,
		const Modumate::Units::FYCoord &ypos,
		const Modumate::Units::FAngle &rotateByRadians,
		const Modumate::FMColor &color,
		Modumate::DraftingAlignment textJustify,
		const Modumate::Units::FWidth &containingRectWidth,
		Modumate::FontType type,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError GetTextLength(
		const TCHAR *text,
		const Modumate::Units::FFontSize &fontSize,
		Modumate::Units::FUnitValue &textLength,
		Modumate::FontType type = Modumate::FontType::Standard) override;

	virtual Modumate::EDrawError DrawArc(
		const Modumate::Units::FXCoord &x,
		const Modumate::Units::FYCoord &y,
		const Modumate::Units::FAngle &a1,
		const Modumate::Units::FAngle &a2,
		const Modumate::Units::FRadius &radius,
		const Modumate::Units::FThickness &lineWidth,
		const Modumate::FMColor &color,
		const Modumate::LinePattern &linePattern,
		int slices,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError AddImage(
		const TCHAR *imageFileFullPath,
		const Modumate::Units::FXCoord &x,
		const Modumate::Units::FYCoord &y,
		const Modumate::Units::FWidth &width,
		const Modumate::Units::FHeight &height,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError FillPoly(
		const float *points,
		int numPoints,
		const Modumate::FMColor &color,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError DrawCircle(
		const Modumate::Units::FXCoord &cx,
		const Modumate::Units::FYCoord &cy,
		const Modumate::Units::FRadius &radius,
		const Modumate::Units::FThickness &lineWidth,
		const Modumate::LinePattern &linePattern,
		const Modumate::FMColor &color,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual Modumate::EDrawError FillCircle(
		const Modumate::Units::FXCoord &cx,
		const Modumate::Units::FYCoord &cy,
		const Modumate::Units::FRadius &radius,
		const Modumate::FMColor &color,
		Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDefault
	) override;

	virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches) override;

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
		Modumate::Units::FThickness Thickness;
		Modumate::FMColor Color;
		Modumate::LinePattern Pattern;
		Modumate::Units::FPhase Phase;
		Modumate::FModumateLayerType LayerType;
		FLineData(Modumate::Units::FThickness thickness, Modumate::FMColor color, Modumate::LinePattern linePattern, Modumate::Units::FPhase phase,
			Modumate::FModumateLayerType layerType);
	};

	using FLineSegments = TArray<FLineSegment>;
	using FLineDataItems = TArray<FLineData>;
	FLineSegments InLines;
	FLineSegments OutLines;
	FLineDataItems LineDataItems;
	int32 CurrentIndex{ 0 };

	void ProcessLines();
	static constexpr float Epsilon = 0.6f;	// World units
};
