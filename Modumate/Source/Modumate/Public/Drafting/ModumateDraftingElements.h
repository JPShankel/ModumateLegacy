// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "Drafting/ModumateClippingTriangles.h"

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
		virtual ~FDraftingElement();
		virtual void ClearChildren();

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) = 0;
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) { return EDrawError::ErrorNone; };

		void ApplyTransform(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D& position,
			ModumateUnitParams::FAngle& orientation,
			float& scale);

		// Some implementations of FDrafting composite are not able to provide
		virtual FModumateUnitCoord2D GetDimensions(IModumateDraftingDraw *drawingInterface) {
			return FModumateUnitCoord2D();
		};

		// TODO: so far AABB has been implemented for DraftingComposite, LinePrimitive, TextPrimitive, and ArcPrimitive,
		// because those are the primitives that appear in floorplan drawings at the time of creation
		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f);

		FModumateUnitCoord2D GetLocalPosition() { return Position; }
		ModumateUnitParams::FAngle GetLocalOrientation() { return Orientation; }
		float GetLocalScale() { return Scale; }

		void SetLocalPosition(FModumateUnitCoord2D position) { Position = position; }
		void SetLocalPosition(ModumateUnitParams::FXCoord x, ModumateUnitParams::FYCoord y) { Position.X = x; Position.Y = y; }
		void SetLocalOrientation(ModumateUnitParams::FAngle orientation) { Orientation = orientation; }
		void SetLocalScale(float scale) { Scale = scale; }

		void MoveXTo(ModumateUnitParams::FXCoord x) { Position.X = x; }
		void MoveYTo(ModumateUnitParams::FYCoord y) { Position.Y = y; }

		void TranslateBy(FModumateUnitCoord2D position) { Position += position; }
		void TranslateBy(ModumateUnitParams::FXCoord x) { Position.X += x; }
		void TranslateBy(ModumateUnitParams::FYCoord y) { Position.Y += y; }

		void SetLocalTransform(FModumateUnitCoord2D position, ModumateUnitParams::FAngle orientation, float scale);
		FModumateLayerType GetLayerType() const { return LayerType; }
		void SetLayerTypeRecursive(FModumateLayerType layerType);

	protected:
		FModumateUnitCoord2D Position;
		ModumateUnitParams::FAngle Orientation = ModumateUnitParams::FAngle::Radians(0);
		// TODO: while scale is applied during Draw, it isn't respected at the primitive level
		float Scale = 1.0f;
		FModumateLayerType LayerType { FModumateLayerType::kDefault };

	public:
		FModumateUnitCoord2D Dimensions;
		DraftingAlignment VerticalAlignment = DraftingAlignment::Bottom;
		DraftingAlignment HorizontalAlignment = DraftingAlignment::Left;

		TArray<TSharedPtr<FDraftingElement>> Children;

		FBox2D BoundingBox = FBox2D(EForceInit::ForceInitToZero);

		TUniquePtr<FModumateClippingTriangles> lineClipping;
		// Cut-plane lines that may occlude beyond cut-plane lines.
		TArray<FEdge> inPlaneLines;
	};

	// represents a "node" of an object tree
	// applies its own linear transformations during draw, then calls draw on its children
	class FDraftingComposite : public FDraftingElement
	{
	public:
		FDraftingComposite() {};

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FModumateUnitCoord2D GetDimensions(IModumateDraftingDraw *drawingInterface) override;
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

	};

	class FTextPrimitive : public FDraftingElement
	{
	public:
		FTextPrimitive(
			FString text = FString(),
			ModumateUnitParams::FFontSize fontSize = ModumateUnitParams::FFontSize::FontHeight(16),
			FMColor color = FMColor::Black,
			FontType type = FontType::Standard,
			DraftingAlignment justify = DraftingAlignment::Left,
			ModumateUnitParams::FAngle angle = ModumateUnitParams::FAngle::Radians(0),
			ModumateUnitParams::FWidth width = ModumateUnitParams::FWidth::FloorplanInches(0)) :
			Text(text), FontSize(fontSize), Color(color), TextFontName(type), TextAlignment(justify), Angle(angle), ContainerWidth(width)
		{

		}
		virtual ~FTextPrimitive() {}
		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		float GetWidth(IModumateDraftingDraw *drawingInterface);

	public:
		FString Text;
		ModumateUnitParams::FFontSize FontSize = ModumateUnitParams::FFontSize::FontHeight(9.45f);
		FMColor Color = FMColor::Black;
		FontType TextFontName = FontType::Standard;
		DraftingAlignment TextAlignment;
		ModumateUnitParams::FAngle Angle = ModumateUnitParams::FAngle::Radians(0);
		ModumateUnitParams::FWidth ContainerWidth = ModumateUnitParams::FWidth::FloorplanInches(0);
	};

	class FDraftingText : public FDraftingComposite
	{
	public:
		FDraftingText(
			FText text = FText(),
			ModumateUnitParams::FFontSize fontSize = ModumateUnitParams::FFontSize::FontHeight(16),
			FMColor color = FMColor::Black,
			FontType type = FontType::Standard,
			DraftingAlignment justify = DraftingAlignment::Left,
			ModumateUnitParams::FAngle angle = ModumateUnitParams::FAngle::Radians(0),
			ModumateUnitParams::FWidth width = ModumateUnitParams::FWidth::FloorplanInches(0));
		virtual ~FDraftingText() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		// if width is set to a value greater than 0, DraftingText will attempt to wrap (and split) the text
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

		virtual FModumateUnitCoord2D GetDimensions(IModumateDraftingDraw *drawingInterface) override;

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
			ModumateUnitParams::FLength length,
			ModumateUnitParams::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			ModumateUnitParams::FPhase phase = ModumateUnitParams::FPhase::Points(0));
	};

	class FDraftingRectangle : public FDraftingComposite
	{
	public:
		FDraftingRectangle(
			FModumateUnitCoord2D dimensions,
			ModumateUnitParams::FThickness thickness = ModumateUnitParams::FThickness::Points(0.15f),
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			ModumateUnitParams::FPhase phase = ModumateUnitParams::FPhase::Points(0));
	};

	class FDraftingFraction : public FDraftingComposite
	{
	public:
		FDraftingFraction(
			FText numerator,
			FText denominator,
			ModumateUnitParams::FFontSize fontSize,
			ModumateUnitParams::FThickness lineWidth,
			FMColor color = FMColor::Black
		);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText Numerator;
		FText Denominator;
		ModumateUnitParams::FFontSize FontSize;
		ModumateUnitParams::FThickness LineWidth;
		FMColor Color;
	};

	class FDraftingSwingDoor : public FDraftingComposite
	{
	public:
		FDraftingSwingDoor(ModumateUnitParams::FLength doorLength, ModumateUnitParams::FLength doorDepth);

	public:
		ModumateUnitParams::FLength DoorLength;
		ModumateUnitParams::FLength DoorDepth;

		ModumateUnitParams::FThickness DoorFrameThickness = ModumateUnitParams::FThickness::Points(0.5f);
		ModumateUnitParams::FThickness DoorThickness = ModumateUnitParams::FThickness::WorldInches(2.0f);
	};


	class FLinePrimitive : public FDraftingElement
	{
	public:
		FLinePrimitive(
			ModumateUnitParams::FLength length,
			ModumateUnitParams::FThickness thickness,
			FMColor color,
			LinePattern &linePattern,
			ModumateUnitParams::FPhase phase
		) : Length(length), 
			LineWidth(thickness), 
			Color(color), 
			Pattern(linePattern), 
			Phase(phase)
		{}

		virtual ~FLinePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		ModumateUnitParams::FLength Length = ModumateUnitParams::FLength::FloorplanInches(0);
		ModumateUnitParams::FThickness LineWidth;
		FMColor Color;
		LinePattern Pattern;
		ModumateUnitParams::FPhase Phase;
	};

	// TODO: already class called FDrafting line
	class FDraftingLine : public FDraftingComposite
	{
	public:
		FDraftingLine(
			ModumateUnitParams::FLength length,
			ModumateUnitParams::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			ModumateUnitParams::FPhase phase = ModumateUnitParams::FPhase::Points(0))
		{
			Children.Add(TSharedPtr<FLinePrimitive>(new FLinePrimitive(length, thickness, color, linePattern, phase)));
		}

		// Legacy line construction - Position is set to P1, length and orientation are derived from P2 - P1
		FDraftingLine(
			FModumateUnitCoord2D P1,
			FModumateUnitCoord2D P2,
			ModumateUnitParams::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern(),
			ModumateUnitParams::FPhase phase = ModumateUnitParams::FPhase::Points(0));
	};

	class FArcPrimitive : public FDraftingElement
	{
	public:
		FArcPrimitive(
			ModumateUnitParams::FRadius radius,
			ModumateUnitParams::FAngle angle,
			ModumateUnitParams::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern()
		) : Radius(radius), Angle(angle), LineWidth(thickness), Color(color), Pattern(linePattern)
		{}
		virtual ~FArcPrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual FBox2D MakeAABB(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f);

	public:
		ModumateUnitParams::FRadius Radius;
		ModumateUnitParams::FAngle Angle;
		ModumateUnitParams::FThickness LineWidth;
		FMColor Color;
		LinePattern Pattern;
	};

	class FDraftingArc : public FDraftingComposite
	{
	public:
		FDraftingArc(
			ModumateUnitParams::FRadius radius,
			ModumateUnitParams::FAngle angle,
			ModumateUnitParams::FThickness thickness,
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
			ModumateUnitParams::FRadius radius,
			FMColor color = FMColor::Black);

		virtual ~FFilledCirclePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		ModumateUnitParams::FRadius Radius;
		FMColor Color;
	};

	class FCirclePrimitive : public FDraftingElement
	{
	public:
		FCirclePrimitive(
			ModumateUnitParams::FRadius radius,
			ModumateUnitParams::FThickness thickness,
			FMColor color = FMColor::Black,
			LinePattern linePattern = LinePattern()
		);

		virtual ~FCirclePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		ModumateUnitParams::FRadius Radius;
		ModumateUnitParams::FThickness LineWidth;
		FMColor Color;
		LinePattern Pattern;
	};

	class FFilledRectPrimitive : public FDraftingElement
	{
	public:
		FFilledRectPrimitive(
			FModumateUnitCoord2D dimensions,
			FMColor color = FMColor::Black);

		virtual ~FFilledRectPrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
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
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
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
			FModumateUnitCoord2D dimensions
		);

		virtual ~FImagePrimitive() {}

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		FString Path;
	};
}
