// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once
#include "Drafting/APDFLLib.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "Drafting/ModumateDraftingElements.h"

namespace Modumate
{
	class IModumateDraftingDraw;
	class FTag : public FDraftingComposite
	{
	public:
		virtual ~FTag() {}
		FTag() {}

	// shared tag formatting sizes
	public:
		const float SmallMargin =	1.0f / 64.0f; // distance between text and subtext
		const float Margin =		2.0f / 64.0f; // distance between elements and bounding lines
		const float LargeMargin =	3.0f / 64.0f; // distance between elements

		const float SmallFontSize = 4.0f / 64.0f; // as listed in spec, previously defined as 6.3f  font height
		const float FontSize =		6.0f / 64.0f; // as listed in spec, previously defined as 9.45f font height
		const float LargeFontSize = 8.0f / 64.0f; // as listed in spec, previously defined as 12.6f font height

		const float LineWidth =		1 / 288.0f;
	};

	class FFFETag : public FTag
	{
	public:
		FFFETag(const FString &label);

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FDraftingText> ChildText;
		TSharedPtr<FDraftingRectangle> BoundingRectangle;

		FString Label;

		const float LineThickness = 0.25f;
	};

	class FAngleTag : public FTag
	{
	public:
		FAngleTag(float radians);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		ModumateUnitParams::FAngle Angle = ModumateUnitParams::FAngle::Radians(0);
		const float Radius = 24.0f / 64.0f; // default radius, first thing to change given sizing requirements
		const float TickLength = 2.0f / 64.0f;
		const float TextOffset = 5.0f / 64.0f; // default text distance from arc

		int32 MaxAngleDigits = 2;
	};

	class FDimensionTag : public FTag
	{
	public:
		FDimensionTag(ModumateUnitParams::FLength witnessLength, ModumateUnitParams::FLength stringLength, FText dimensionText);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;
	public:
		ModumateUnitParams::FLength WitnessLength;
		ModumateUnitParams::FLength StringLength;
		FText DimensionText;
	};

	class FPortalTagBase : public FTag
	{
	public:
		FPortalTagBase(const FString &mark, const FString &submark, const FString &type, const FString &dimensions);

		// The string layout is the same for door and window tags, so that is done here
		// Bounds are set as well, to help the window and door initialize bounds functions with their outlines
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FString Mark, SubMark, Type, Dims;
	};

	class FWindowTag : public FPortalTagBase
	{
	public:
		FWindowTag(const FString &mark, const FString &submark, const FString &type, const FString &dimensions);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;
	};

	class FDoorTag : public FPortalTagBase
	{
	public:
		FDoorTag(const FString &mark, const FString &submark, const FString &type, const FString &dimensions);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;
	};

	class FRoomTag : public FTag
	{
	public:
		FRoomTag(const FModumateUnitCoord2D &coords, const FString &name, const FString &number, const FString &area, const FString &load);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FString Name, Number, Area, Load;

		// the lower text is the name and number, while the upper text is the area and load
		FMColor TextColorDark = FMColor::Gray32;
		FMColor TextColorLight = FMColor::Gray128;

		const LinePattern RectPattern = { DraftingLineStyle::Dashed,{ 1, 2 } };

		const float DRTAreaOccGap = 6.0f / 64.0f;
		const float DRTTextGap = 3.0f / 64.0f;
		const float DRTLineThickness = 0.5f;
	};

	// TODO: remove this, replace all uses with FMaterialTag : public FTag based on code in FWallTag
	struct FMaterialTagElement
	{
		FMaterialTagElement() {};
		FMaterialTagElement(const TCHAR *m, const TCHAR *t, const TCHAR *s = TEXT("")) : Material(m), Thickness(t), Sequence(s) {}
		FMaterialTagElement(const TCHAR *m, TArray<FString> f, const TCHAR *s = TEXT("")) : Material(m), FractionComponents(f), Sequence(s) {}

		FString Material;
		FString Thickness;
		TArray<FString> FractionComponents;
		FString Sequence;
	};

	// FMaterial Tag represents a Layer ID and a thickness
	// this class could extend FDraftingComposite, but extends FTag to access the shared formatting values
	class FMaterialTag : public FTag
	{
	public:
		FMaterialTag() {}

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText Material;
		TArray<FString> ThicknessFraction;
		FText Sequence;
	};

	class FMaterialTagSequence : public FTag
	{
	public:
		FMaterialTagSequence(const FBIMAssemblySpec &AssemblySpec);

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TArray<FMaterialTagElement> Layers;
		float TotalThickness;
	};

	class FWallTag : public FTag
	{
	public:
		FWallTag(const FBIMAssemblySpec& AssemblySpec);

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FMaterialTagSequence> MaterialTags;
	};

	class FFilledRoundedRectTag : public FTag
	{
	public:
		FFilledRoundedRectTag(TSharedPtr<FDraftingComposite> content) : Content(content) {};

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FDraftingComposite> Content;

		ModumateUnitParams::FRadius CornerRadius; //FRadius::FloorplanInches(2.0f * Margin);
		// default yellow color used in schedules
		// FMColor(254.0f / 255.0f, 247.0f / 255.0f, 194.0f / 255.0f);
		FMColor FillColor = FMColor::White;
		FModumateUnitCoord2D OuterMargins;
	};

	// FHeaderTag expects an initialized object as its argument, and it will wrap the object with a header text object
	class FHeaderTag : public FTag
	{
	public:
		FHeaderTag(FText header, TSharedPtr<FDraftingComposite> content) : HeaderText(header), Content(content) {};

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText HeaderText;
		TSharedPtr<FDraftingComposite> Content;
	};

	class FMatchLineTag : public FTag
	{
	public:
		FMatchLineTag(FText topDrawing, FText bottomDrawing, ModumateUnitParams::FLength pageLength) :
			TopDrawing(topDrawing), BottomDrawing(bottomDrawing), PageLength(pageLength) {};

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText TopDrawing;
		FText BottomDrawing;
		ModumateUnitParams::FLength PageLength;

	public:
		ModumateUnitParams::FXCoord PageOffset = ModumateUnitParams::FXCoord::FloorplanInches(1.0f);					// match line offset from page dimension
		ModumateUnitParams::FXCoord TextOffset = ModumateUnitParams::FXCoord::FloorplanInches(0.5f);					// additional text offset from match line offset
		ModumateUnitParams::FYCoord MatchLineMargin = ModumateUnitParams::FYCoord::FloorplanInches(6.0f / 64.0f);		// distance between text objects and dashed line

		float MatchLineWidth = 1.0f / 24.0f;
		ModumateUnitParams::FFontSize TextFontSize = ModumateUnitParams::FFontSize::FloorplanInches(12.0f / 64.0f);	// match line text font size

		FMColor Color = FMColor::Gray32;
		LinePattern Pattern = { DraftingLineStyle::Dashed, { 16, 8 } };
	};

	class FTitleTag : public FTag
	{
	public:
		FTitleTag(FText number, FText title, FText scale) : NumberText(number), TitleText(title), ScaleText(scale) {};

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText NumberText;
		FText TitleText;
		FText ScaleText;

		ModumateUnitParams::FRadius Radius = ModumateUnitParams::FRadius::FloorplanInches(1.0f / 6.0f);
		ModumateUnitParams::FLength LineOverrunRight = ModumateUnitParams::FLength::FloorplanInches(1.0f / 3.0f);

		FMColor Color = FMColor::Gray32;
	};

	class FSectionTag : public FTag
	{
	public:
		FSectionTag(FText view, FText sheet, ModumateUnitParams::FAngle angle = ModumateUnitParams::FAngle::Degrees(0)) : ViewText(view), SheetText(sheet), ArrowAngle(angle) {};

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		FText ViewText;
		FText SheetText;
		ModumateUnitParams::FAngle ArrowAngle;

		ModumateUnitParams::FRadius Radius = ModumateUnitParams::FRadius::FloorplanInches(1.0f / 4.0f);

		FMColor Color = FMColor::Gray32;
	};

	class FScaleTag : public FTag
	{
	public:
		FScaleTag(TArray<ModumateUnitParams::FLength> lengths) : Lengths(lengths) {};

		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TArray<ModumateUnitParams::FLength> Lengths;
	};

	// TODO: Previous implementations of the following tags were removed in Change 2354:
	// - Floor/Roof tags
	// - Spot elevation tags
	// - Elevation tags
	// - Stair tags
	// - Finish tags
	// - Gridline tags
	// - Section tags
	// - SpotSlope tags
	// - LevelHead tags

	// These tags were removed for the purpose of also removing the pdf interface functions that supported them,
	// so while the specific change mentioned above can be reviewed to see the previous implementation, a new implementation may not be similar.

	// An accurate design specification describing the formatting for all tags can be found at:
	// G:/Modumate/ENGINEERING/Assets/2D/Drafting Specs/SPEC - Drafting Tags.pdf
}