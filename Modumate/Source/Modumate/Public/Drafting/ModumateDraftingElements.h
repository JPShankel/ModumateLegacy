// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once
#include "APDFLLib.h"
#include "ModumateUnits.h"
#include "ModumateDraftingDraw.h"

#define DEFAULT_OBJECT_EPSILON 0.1f

namespace Modumate
{
	class IModumateDraftingDraw;

	// class that "leaf" or primitive classes extend.
	// this class has no understanding of its own position, and feeds the
	// positional arguments fed in during Draw to the pdf library
	class FDraftingElement : public TSharedFromThis<FDraftingElement>
	{
	public:
		FDraftingElement() {};
		virtual ~FDraftingElement() { ClearChildren(); };
		virtual void ClearChildren();

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) = 0;
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) { return EDrawError::ErrorNone; };

		void ApplyTransform(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D& position,
			Units::FAngle& orientation,
			float& scale);

		// Some implementations of FDrafting composite are not able to provide
		virtual Units::FCoordinates2D GetDimensions(IModumateDraftingDraw *drawingInterface) {
			return Units::FCoordinates2D();
		};

		// TODO: so far AABB has been implemented for DraftingComposite, LinePrimitive, TextPrimitive, and ArcPrimitive,
		// because those are the primitives that appear in floorplan drawings at the time of creation
		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f);

		Units::FCoordinates2D GetLocalPosition() { return Position; }
		Units::FAngle GetLocalOrientation() { return Orientation; }
		float GetLocalScale() { return Scale; }

		void SetLocalPosition(Units::FCoordinates2D position) { Position = position; }
		void SetLocalPosition(Units::FXCoord x, Units::FYCoord y) { Position.X = x; Position.Y = y; }
		void SetLocalOrientation(Units::FAngle orientation) { Orientation = orientation; }
		void SetLocalScale(float scale) { Scale = scale; }

		void MoveXTo(Units::FXCoord x) { Position.X = x; }
		void MoveYTo(Units::FYCoord y) { Position.Y = y; }

		void TranslateBy(Units::FCoordinates2D position) { Position += position; }
		void TranslateBy(Units::FXCoord x) { Position.X += x; }
		void TranslateBy(Units::FYCoord y) { Position.Y += y; }

		void SetLocalTransform(Units::FCoordinates2D position, Units::FAngle orientation, float scale);

	protected:
		Units::FCoordinates2D Position;
		Units::FAngle Orientation = Units::FAngle::Radians(0);
		// TODO: while scale is applied during Draw, it isn't respected at the primitive level
		float Scale = 1.0f;

	public:
		Units::FCoordinates2D Dimensions;
		DraftingAlignment VerticalAlignment = DraftingAlignment::Bottom;
		DraftingAlignment HorizontalAlignment = DraftingAlignment::Left;

		TArray<TSharedPtr<FDraftingElement>> Children;

		FBox2D BoundingBox = FBox2D(EForceInit::ForceInitToZero);
	};

	// represents a "node" of an object tree
	// applies its own linear transformations during draw, then calls draw on its children
	class FDraftingComposite : public FDraftingElement
	{
	public:
		FDraftingComposite() {};

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual Units::FCoordinates2D GetDimensions(IModumateDraftingDraw *drawingInterface) override;
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	};

	class FTextPrimitive : public FDraftingElement
	{
	public:
		FTextPrimitive(
			FString text = FString(),
			Units::FFontSize fontSize = Units::FFontSize::FontHeight(16),
			FMColor color = FMColor::Black,
			FontType type = FontType::Standard,
			DraftingAlignment justify = DraftingAlignment::Left,
			Units::FAngle angle = Units::FAngle::Radians(0),
			Units::FWidth width = Units::FWidth::FloorplanInches(0)) :
			Text(text), FontSize(fontSize), Color(color), TextFontName(type), TextAlignment(justify), Angle(angle), ContainerWidth(width)
		{

		}
		virtual ~FTextPrimitive() {}
		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

		float GetWidth(IModumateDraftingDraw *drawingInterface);

	public:
		FString Text;
		Units::FFontSize FontSize = Units::FFontSize::FontHeight(9.45f);
		FMColor Color = FMColor::Black;
		FontType TextFontName = FontType::Standard;
		DraftingAlignment TextAlignment;
		Units::FAngle Angle = Units::FAngle::Radians(0);
		Units::FWidth ContainerWidth = Units::FWidth::FloorplanInches(0);
	};

	class FDraftingText : public FDraftingComposite
	{
	public:
		FDraftingText(
			FText text = FText(),
			Units::FFontSize fontSize = Units::FFontSize::FontHeight(16),
			FMColor color = FMColor::Black,
			FontType type = FontType::Standard,
			DraftingAlignment justify = DraftingAlignment::Left,
			Units::FAngle angle = Units::FAngle::Radians(0),
			Units::FWidth width = Units::FWidth::FloorplanInches(0));
		virtual ~FDraftingText() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

		// if width is set to a value greater than 0, DraftingText will attempt to wrap (and split) the text
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

		virtual Units::FCoordinates2D GetDimensions(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FTextPrimitive> Child;

	public:
		FString Delimiter = FString(TEXT(" "));
		float RowMargin = 2.0f / 64.0f;
	};

	class FDraftingTick : public FDraftingComposite
	{
	public:
		FDraftingTick(
			Units::FLength length,
			Units::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			Units::FPhase phase = Units::FPhase::Points(0));
	};

	class FDraftingRectangle : public FDraftingComposite
	{
	public:
		FDraftingRectangle(
			Units::FCoordinates2D dimensions,
			Units::FThickness thickness = Units::FThickness::Points(0.15f),
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			Units::FPhase phase = Units::FPhase::Points(0));
	};

	class FDraftingFraction : public FDraftingComposite
	{
	public:
		FDraftingFraction(
			FText numerator,
			FText denominator,
			Units::FFontSize fontSize,
			Units::FThickness lineWidth,
			FMColor color = FMColor::Black
		);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText Numerator;
		FText Denominator;
		Units::FFontSize FontSize;
		Units::FThickness LineWidth;
		FMColor Color;
	};

	class FDraftingSwingDoor : public FDraftingComposite
	{
	public:
		FDraftingSwingDoor(Units::FLength doorLength, Units::FLength doorDepth);

	public:
		Units::FLength DoorLength;
		Units::FLength DoorDepth;

		Units::FThickness DoorFrameThickness = Units::FThickness::Points(0.5f);
		Units::FThickness DoorThickness = Units::FThickness::WorldInches(2.0f);
	};


	class FLinePrimitive : public FDraftingElement
	{
	public:
		FLinePrimitive(
			Units::FLength length,
			Units::FThickness thickness,
			FMColor color,
			LinePattern &linePattern,
			Units::FPhase phase
		) : Length(length), LineWidth(thickness), Color(color), Pattern(linePattern), Phase(phase)
		{}

		virtual ~FLinePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		Units::FLength Length = Units::FLength::FloorplanInches(0);
		Units::FThickness LineWidth;
		FMColor Color;
		LinePattern Pattern;
		Units::FPhase Phase;
	};

	// TODO: already class called FDrafting line
	class FDraftingLine : public FDraftingComposite
	{
	public:
		FDraftingLine(
			Units::FLength length,
			Units::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			Units::FPhase phase = Units::FPhase::Points(0))
		{
			Children.Add(TSharedPtr<FLinePrimitive>(new FLinePrimitive(length, thickness, color, linePattern, phase)));
		}

		// Legacy line construction - Position is set to P1, length and orientation are derived from P2 - P1
		FDraftingLine(
			Units::FCoordinates2D P1,
			Units::FCoordinates2D P2,
			Units::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			Units::FPhase phase = Units::FPhase::Points(0));
	};

	class FArcPrimitive : public FDraftingElement
	{
	public:
		FArcPrimitive(
			Units::FRadius radius,
			Units::FAngle angle,
			Units::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern()
		) : Radius(radius), Angle(angle), LineWidth(thickness), Color(color), Pattern(linePattern)
		{}
		virtual ~FArcPrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f);

	public:
		Units::FRadius Radius;
		Units::FAngle Angle;
		Units::FThickness LineWidth;
		FMColor Color;
		LinePattern Pattern;
	};

	class FDraftingArc : public FDraftingComposite
	{
	public:
		FDraftingArc(
			Units::FRadius radius,
			Units::FAngle angle,
			Units::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern())
		{
			Children.Add(MakeShareable(new FArcPrimitive(radius, angle, thickness, color, linePattern)));
		}
	};

	class FFilledCirclePrimitive : public FDraftingElement
	{
	public:
		FFilledCirclePrimitive(
			Units::FRadius radius,
			FMColor color = FMColor::Black);

		virtual ~FFilledCirclePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		Units::FRadius Radius;
		FMColor Color;
	};

	class FCirclePrimitive : public FDraftingElement
	{
	public:
		FCirclePrimitive(
			Units::FRadius radius,
			Units::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern()
		);

		virtual ~FCirclePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		Units::FRadius Radius;
		Units::FThickness LineWidth;
		FMColor Color;
		LinePattern Pattern;
	};

	class FFilledRectPrimitive : public FDraftingElement
	{
	public:
		FFilledRectPrimitive(
			Units::FCoordinates2D dimensions,
			FMColor color = FMColor::Black);

		virtual ~FFilledRectPrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		FMColor Color;
	};

	class FFilledPolyPrimitive : public FDraftingElement
	{
	public:
		FFilledPolyPrimitive(
			TArray<FVector2D> polyPoints,
			FMColor color = FMColor::White
		) : PolyPoints(polyPoints), Color(color) {}

		virtual ~FFilledPolyPrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		TArray<FVector2D> PolyPoints;
		FMColor Color;
	};

	class FImagePrimitive : public FDraftingElement
	{
	public:
		FImagePrimitive(
			FString imagePath,
			Units::FCoordinates2D dimensions
		);

		virtual ~FImagePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		FString Path;
	};
}
