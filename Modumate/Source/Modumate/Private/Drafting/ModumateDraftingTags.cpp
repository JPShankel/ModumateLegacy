// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDraftingTags.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Drafting/ModumateDraftingView.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "CoreMinimal.h"
#include "UnrealClasses/Modumate.h"

using namespace Modumate::Units;
using namespace Modumate::PDF;

namespace Modumate { 


FFFETag::FFFETag(const FString &label) :
	Label(label)
{}

EDrawError FFFETag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	EDrawError error = EDrawError::ErrorNone;

	ClearChildren();

	ChildText = MakeShareable(new FDraftingText(FText::FromString(Label), Units::FFontSize::FloorplanInches(FontSize), FMColor::Gray32));

	FCoordinates2D margins = FCoordinates2D(FXCoord::FloorplanInches(Margin), FYCoord::FloorplanInches(Margin));
	FCoordinates2D textDimensions = ChildText->GetDimensions(drawingInterface);

	// define the dimensions of the tag based on the layout
	Dimensions = (2.0f * margins) + textDimensions;
	HorizontalAlignment = DraftingAlignment::Center;
	VerticalAlignment = DraftingAlignment::Center;

	ChildText->SetLocalPosition(margins);

	BoundingRectangle = MakeShareable(new FDraftingRectangle(Dimensions, Units::FThickness::Points(LineThickness), FMColor::Gray32));

	TSharedPtr<FFilledRectPrimitive> FillPoly = MakeShareable(new FFilledRectPrimitive(Dimensions, FMColor::White));

	Children.Add(FillPoly);
	Children.Add(BoundingRectangle);
	Children.Add(ChildText);

	return error;
}

FAngleTag::FAngleTag(float radians)
{
	Angle = Units::FAngle::Radians(radians);
}

EDrawError FAngleTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	ClearChildren();

	TSharedPtr<FDraftingArc> arc = MakeShareable(new FDraftingArc(
		FRadius::FloorplanInches(Radius),
		Angle,
		FThickness::FloorplanInches(LineWidth),
		FMColor::Gray32));

	arc->SetLocalOrientation(FAngle::Degrees(Angle.AsDegrees() * -0.5f));

	FNumberFormattingOptions options = FNumberFormattingOptions();
	options.MaximumFractionalDigits = MaxAngleDigits;
	static const FText AngleFormat = FText::FromString(TEXT("{0}°"));
	FText angleText = FText::Format(AngleFormat, FText::AsNumber(Angle.AsDegrees(), &options));

	TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(angleText, FFontSize::FloorplanInches(FontSize), FMColor::Gray32));

	text->MoveXTo(FXCoord::FloorplanInches(Radius + TextOffset));
	text->VerticalAlignment = DraftingAlignment::Center;

	TSharedPtr<FDraftingComposite> startTickParent = MakeShareable(new FDraftingComposite());
	TSharedPtr<FDraftingComposite> endTickParent = MakeShareable(new FDraftingComposite());

	TSharedPtr<FDraftingTick> tick1 = MakeShareable(new FDraftingTick(FLength::FloorplanInches(TickLength), FThickness::FloorplanInches(LineWidth), FMColor::Gray32));
	TSharedPtr<FDraftingTick> tick2 = MakeShareable(new FDraftingTick(FLength::FloorplanInches(TickLength), FThickness::FloorplanInches(LineWidth), FMColor::Gray32));

	tick1->MoveXTo(FXCoord::FloorplanInches(Radius));
	tick2->MoveXTo(FXCoord::FloorplanInches(Radius));

	startTickParent->SetLocalOrientation(FAngle::Degrees(Angle.AsDegrees() * -0.5f));
	endTickParent->SetLocalOrientation(FAngle::Degrees(Angle.AsDegrees() * 0.5f));

	startTickParent->Children.Add(tick1);
	endTickParent->Children.Add(tick2);

	Children.Add(arc);
	Children.Add(text);
	Children.Add(startTickParent);
	Children.Add(endTickParent);

	return EDrawError::ErrorNone;
}

FDimensionTag::FDimensionTag(FLength witnessLength, FLength stringLength, FText dimensionText)
	: WitnessLength(witnessLength), StringLength(stringLength), DimensionText(dimensionText)
{

}

EDrawError FDimensionTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	FMColor Color = FMColor::Gray32;
	FLength OverrunLength = FLength::FloorplanInches(LargeMargin);
	FLength WitnessLineLength = WitnessLength + OverrunLength;
	FLength StringLineLength = StringLength + (OverrunLength * 2.0f);

	// three lines - two vertical witness lines separated by the string length, and a horizontal line connecting them
	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(WitnessLineLength, FThickness::FloorplanInches(LineWidth), Color));
	line->SetLocalOrientation(Units::FAngle::Degrees(90));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(WitnessLineLength, FThickness::FloorplanInches(LineWidth), Color));
	line->MoveXTo(FXCoord::FloorplanInches(StringLength.AsFloorplanInches()));
	line->SetLocalOrientation(Units::FAngle::Degrees(90));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(StringLineLength, FThickness::FloorplanInches(LineWidth), Color));
	line->SetLocalPosition(	FXCoord::FloorplanInches(OverrunLength.AsFloorplanInches() * -1.0f),
							FYCoord::FloorplanInches(WitnessLength.AsFloorplanInches()));
	line->SetLocalOrientation(Units::FAngle::Degrees(0));
	Children.Add(line);

	// ticks in each corner where the three lines intersect
	TSharedPtr<FDraftingTick> tick = MakeShareable(new FDraftingTick(FLength::FloorplanInches(Margin), FThickness::FloorplanInches(LineWidth), Color));
	tick->MoveYTo(FYCoord::FloorplanInches(WitnessLength.AsFloorplanInches()));
	Children.Add(tick);

	tick = MakeShareable(new FDraftingTick(FLength::FloorplanInches(Margin), FThickness::FloorplanInches(LineWidth), Color));
	tick->SetLocalPosition(	FXCoord::FloorplanInches(StringLength.AsFloorplanInches()),
							FYCoord::FloorplanInches(WitnessLength.AsFloorplanInches()));
	Children.Add(tick);

	// length text
	TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(DimensionText, FFontSize::FloorplanInches(FontSize), Color));
	text->SetLocalPosition(	FXCoord::FloorplanInches(StringLength.AsFloorplanInches() / 2.0f),
							FYCoord::FloorplanInches(WitnessLength.AsFloorplanInches() + SmallMargin));
	text->HorizontalAlignment = DraftingAlignment::Center;
	Children.Add(text);

	Dimensions.X = FXCoord::FloorplanInches(StringLineLength.AsFloorplanInches());
	Dimensions.Y = text->GetLocalPosition().Y + text->GetDimensions(drawingInterface).Y;

	return EDrawError::ErrorNone;
}

FPortalTagBase::FPortalTagBase(const FString &mark, const FString &submark, const FString &type, const FString &dimensions)
	: Mark(mark), SubMark(submark), Type(type), Dims(dimensions)
{

}

EDrawError FPortalTagBase::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	TSharedPtr<FDraftingText> mark = MakeShareable(new FDraftingText(FText::FromString(Mark), FFontSize::FloorplanInches(FontSize), FMColor::Gray32));
	TSharedPtr<FDraftingText> type = MakeShareable(new FDraftingText(FText::FromString(Type), FFontSize::FloorplanInches(SmallFontSize), FMColor::Gray128));
	TSharedPtr<FDraftingText> dims = MakeShareable(new FDraftingText(FText::FromString(Dims), FFontSize::FloorplanInches(SmallFontSize), FMColor::Gray128));

	FCoordinates2D markDims = mark->GetDimensions(drawingInterface);
	FCoordinates2D typeDims = type->GetDimensions(drawingInterface);
	FCoordinates2D dimsDims = dims->GetDimensions(drawingInterface);

	TSharedPtr<FDraftingComposite> topRow = MakeShareable(new FDraftingComposite());
	TSharedPtr<FDraftingComposite> bottomRow = MakeShareable(new FDraftingComposite());

	topRow->Children.Add(mark);
	bottomRow->Children.Add(type);
	bottomRow->Children.Add(dims);

	float markWidth = markDims.X.AsFloorplanInches();
	if (SubMark.Len() > 0)
	{
		TSharedPtr<FDraftingText> submark = MakeShareable(new FDraftingText(FText::FromString(SubMark), FFontSize::FloorplanInches(FontSize), FMColor::Gray32));
		markWidth += submark->GetDimensions(drawingInterface).X.AsFloorplanInches() + Margin;

		submark->MoveXTo(FXCoord::FloorplanInches(markWidth));
		submark->HorizontalAlignment = DraftingAlignment::Right;

		topRow->Children.Add(submark);
	}

	float typeWidth = typeDims.X.AsFloorplanInches() + dimsDims.X.AsFloorplanInches() + LargeMargin;

	float width = FMath::Max(typeWidth, markWidth) + 2 * Margin;
	float height = FontSize + SmallFontSize + 3 * Margin;

	Dimensions = FCoordinates2D(FXCoord::FloorplanInches(width), FYCoord::FloorplanInches(height));

	topRow->Dimensions = FCoordinates2D(FXCoord::FloorplanInches(markWidth), FYCoord::FloorplanInches(FontSize));
	bottomRow->Dimensions = FCoordinates2D(FXCoord::FloorplanInches(typeWidth), FYCoord::FloorplanInches(SmallFontSize));

	bottomRow->SetLocalPosition(FXCoord::FloorplanInches(width / 2.0f), FYCoord::FloorplanInches(Margin));
	bottomRow->HorizontalAlignment = DraftingAlignment::Center;
	dims->MoveXTo(FXCoord::FloorplanInches(typeWidth));
	dims->HorizontalAlignment = DraftingAlignment::Right;

	topRow->SetLocalPosition(FXCoord::FloorplanInches(width / 2.0f), FYCoord::FloorplanInches(height - Margin));
	topRow->HorizontalAlignment = DraftingAlignment::Center;
	topRow->VerticalAlignment = DraftingAlignment::Top;

	Children.Add(topRow);
	Children.Add(bottomRow);

	HorizontalAlignment = DraftingAlignment::Center;
	VerticalAlignment = DraftingAlignment::Center;

	return EDrawError::ErrorNone;
}

FWindowTag::FWindowTag(const FString &mark, const FString &submark, const FString &type, const FString &dimensions)
	: FPortalTagBase(mark, submark, type, dimensions)
{

}

EDrawError FWindowTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	ClearChildren();

	FPortalTagBase::InitializeBounds(drawingInterface);

	FThickness lineThickness = FThickness::FloorplanInches(LineWidth);
	float width = Dimensions.X.AsFloorplanInches();
	float height = Dimensions.Y.AsFloorplanInches();
	float horizontalLineLength = width - Margin * 2.0f;

	// bottom/top - horizontal lines
	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(horizontalLineLength), lineThickness, FMColor::Gray32));
	line->MoveXTo(FXCoord::FloorplanInches(Margin));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(horizontalLineLength), lineThickness, FMColor::Gray32));
	line->SetLocalPosition(FXCoord::FloorplanInches(Margin), Dimensions.Y);
	Children.Add(line);

	// four angled lines from the center of the left and right sides to the bottom and top horizontal lines
	// derive length and angle from margin and height
	float angledLineLength = (FVector2D(0.0f, height / 2.0f) - FVector2D(Margin, 0.0f)).Size();
	float angle = FMath::Atan2(height / 2.0f, Margin);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(angledLineLength), lineThickness, FMColor::Gray32));
	line->MoveYTo(FYCoord::FloorplanInches(height / 2.0f));
	line->SetLocalOrientation(FAngle::Radians(angle));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(angledLineLength), lineThickness, FMColor::Gray32));
	line->MoveYTo(FYCoord::FloorplanInches(height / 2.0f));
	line->SetLocalOrientation(FAngle::Radians(-angle));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(angledLineLength), lineThickness, FMColor::Gray32));
	line->SetLocalPosition(Dimensions.X, FYCoord::FloorplanInches(height / 2.0f));
	line->SetLocalOrientation(FAngle::Radians(PI - angle));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(angledLineLength), lineThickness, FMColor::Gray32));
	line->SetLocalPosition(Dimensions.X, FYCoord::FloorplanInches(height / 2.0f));
	line->SetLocalOrientation(FAngle::Radians(PI + angle));
	Children.Add(line);

	return EDrawError::ErrorNone;
}

FDoorTag::FDoorTag(
	const FString &mark,
	const FString &submark,
	const FString &type,
	const FString &dimensions) : FPortalTagBase(mark, submark, type, dimensions)
{}

EDrawError FDoorTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	ClearChildren();

	FPortalTagBase::InitializeBounds(drawingInterface);

	FThickness lineThickness = FThickness::FloorplanInches(LineWidth);
	float width = Dimensions.X.AsFloorplanInches();
	float height = Dimensions.Y.AsFloorplanInches();

	// total width minus twice the margin on both sides of the tag
	float horizontalLineLength = width - (Margin * 2.0f) * 2.0f;
	float verticalLineLength = height - (Margin * 2.0f) * 2.0f;
	float radius = 2.0f * Margin;
	FAngle  angle = FAngle::Degrees(90.0f);

	// bottom/top - horizontal lines
	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(horizontalLineLength), lineThickness, FMColor::Gray32));
	line->MoveXTo(FXCoord::FloorplanInches(radius));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(horizontalLineLength), lineThickness, FMColor::Gray32));
	line->SetLocalPosition(FXCoord::FloorplanInches(radius), Dimensions.Y);
	Children.Add(line);

	// left/right = vertical lines
	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(verticalLineLength), lineThickness, FMColor::Gray32));
	line->SetLocalPosition(Dimensions.X, FYCoord::FloorplanInches(radius));
	line->SetLocalOrientation(FAngle::Degrees(90.0f));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(FLength::FloorplanInches(verticalLineLength), lineThickness, FMColor::Gray32));
	line->MoveYTo(FYCoord::FloorplanInches(radius));
	line->SetLocalOrientation(FAngle::Degrees(90.0f));
	Children.Add(line);

	// corner arcs
	TSharedPtr<FDraftingArc> arc = MakeShareable(new FDraftingArc(FRadius::FloorplanInches(radius), angle, lineThickness, FMColor::Gray32));
	arc->SetLocalPosition(FXCoord::FloorplanInches(width - radius), FYCoord::FloorplanInches(height - radius));
	Children.Add(arc);

	arc = MakeShareable(new FDraftingArc(FRadius::FloorplanInches(radius), angle, lineThickness, FMColor::Gray32));
	arc->SetLocalPosition(FXCoord::FloorplanInches(radius), FYCoord::FloorplanInches(height - radius));
	arc->SetLocalOrientation(FAngle::Degrees(90.0f));
	Children.Add(arc);

	arc = MakeShareable(new FDraftingArc(FRadius::FloorplanInches(radius), angle, lineThickness, FMColor::Gray32));
	arc->SetLocalPosition(FXCoord::FloorplanInches(radius), FYCoord::FloorplanInches(radius));
	arc->SetLocalOrientation(FAngle::Degrees(180.0f));
	Children.Add(arc);

	arc = MakeShareable(new FDraftingArc(FRadius::FloorplanInches(radius), angle, lineThickness, FMColor::Gray32));
	arc->SetLocalPosition(FXCoord::FloorplanInches(width - radius), FYCoord::FloorplanInches(radius));
	arc->SetLocalOrientation(FAngle::Degrees(270.0f));
	Children.Add(arc);

	return EDrawError::ErrorNone;
}

FRoomTag::FRoomTag(
	const FCoordinates2D &coords,
	const FString &name,
	const FString &number,
	const FString &area,
	const FString &load) : Name(name),Number(number),Area(area),Load(load)
{
	Position = coords;
}

EDrawError FRoomTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	ClearChildren();

	TSharedPtr<FDraftingText> name = MakeShareable(new FDraftingText(FText::FromString(Name), FFontSize::FloorplanInches(LargeFontSize), TextColorDark));
	TSharedPtr<FDraftingText> number = MakeShareable(new FDraftingText(FText::FromString(Number), FFontSize::FloorplanInches(FontSize), TextColorDark));
	TSharedPtr<FDraftingText> load = MakeShareable(new FDraftingText(FText::FromString(Load), FFontSize::FloorplanInches(FontSize), TextColorLight));
	TSharedPtr<FDraftingText> area = MakeShareable(new FDraftingText(FText::FromString(Area), FFontSize::FloorplanInches(FontSize), TextColorLight));

	FCoordinates2D nameDims = name->GetDimensions(drawingInterface);
	FCoordinates2D numberDims = number->GetDimensions(drawingInterface);
	FCoordinates2D loadDims = load->GetDimensions(drawingInterface);
	FCoordinates2D areaDims = area->GetDimensions(drawingInterface);

	float occAreaWidth = loadDims.X.AsFloorplanInches() + areaDims.X.AsFloorplanInches() + DRTAreaOccGap;
	float totalWidth = FMath::Max(nameDims.X.AsFloorplanInches(), FMath::Max(numberDims.X.AsFloorplanInches(), occAreaWidth));
	totalWidth += 2 * Margin;

	float totalHeight = 2 * DRTTextGap + 2 * Margin + nameDims.Y.AsFloorplanInches() + loadDims.Y.AsFloorplanInches()*2.0f;

	name->SetLocalPosition(FXCoord::FloorplanInches(totalWidth / 2.0f), FYCoord::FloorplanInches(totalHeight / 2.0f));
	name->HorizontalAlignment = DraftingAlignment::Center;
	name->VerticalAlignment = DraftingAlignment::Center;

	number->SetLocalPosition(FXCoord::FloorplanInches(totalWidth / 2.0f), FYCoord::FloorplanInches(totalHeight - Margin - FontSize / 2.0f));
	number->HorizontalAlignment = DraftingAlignment::Center;
	number->VerticalAlignment = DraftingAlignment::Center;

	TSharedPtr<FDraftingComposite> bottomRow = MakeShareable(new FDraftingComposite());
	bottomRow->SetLocalPosition(FXCoord::FloorplanInches(totalWidth / 2.0f), FYCoord::FloorplanInches(Margin));
	bottomRow->Dimensions = FCoordinates2D(
		FXCoord::FloorplanInches(occAreaWidth),
		FYCoord::FloorplanInches(FontSize));
	bottomRow->HorizontalAlignment = DraftingAlignment::Center;

	load->MoveXTo(FXCoord::FloorplanInches(occAreaWidth));
	load->HorizontalAlignment = DraftingAlignment::Right;

	bottomRow->Children.Add(load);
	bottomRow->Children.Add(area);

	TSharedPtr<FDraftingRectangle> boundingRectangle = MakeShareable(new FDraftingRectangle(
		FCoordinates2D(FXCoord::FloorplanInches(totalWidth), FYCoord::FloorplanInches(totalHeight)),
		FThickness::Points(DRTLineThickness),
		TextColorDark,
		RectPattern));

	Children.Add(name);
	Children.Add(number);
	Children.Add(boundingRectangle);

	Children.Add(bottomRow);

	Dimensions = boundingRectangle->GetDimensions(drawingInterface);
	HorizontalAlignment = DraftingAlignment::Center;
	VerticalAlignment = DraftingAlignment::Center;

	return EDrawError::ErrorNone;
}

EDrawError FMaterialTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	ClearChildren();

	TSharedPtr<FDraftingText> material = MakeShareable(new FDraftingText(Material, Units::FFontSize::FloorplanInches(FontSize), FMColor::Gray32));

	// align material text to the left with the old width value
	material->MoveYTo(FYCoord::FloorplanInches(LargeMargin));
	Children.Add(material);

	// add width of current material tag
	float width = material->GetDimensions(drawingInterface).X.AsFloorplanInches();

	// thickness fraction is three elements: [0] is the whole number, [1] is the numerator, [2] is the denominator
	if (ThicknessFraction[0] != "0")
	{
		TSharedPtr<FDraftingText> thickness = MakeShareable(new FDraftingText(FText::FromString(ThicknessFraction[0]), Units::FFontSize::FloorplanInches(FontSize), FMColor::Gray128));

		width += SmallMargin / 2.0f;
		thickness->MoveXTo(Units::FXCoord::FloorplanInches(width));
		Children.Add(thickness);

		width += thickness->GetDimensions(drawingInterface).X.AsFloorplanInches();
	}

	if (ThicknessFraction[1] != "0" && ThicknessFraction[2] != "0")
	{
		TSharedPtr<FDraftingFraction> fraction = MakeShareable(new FDraftingFraction(
			FText::FromString(ThicknessFraction[1]),
			FText::FromString(ThicknessFraction[2]),
			FFontSize::FloorplanInches(SmallFontSize),
			FThickness::FloorplanInches(LineWidth),
			FMColor::Gray128
		));

		fraction->InitializeBounds(drawingInterface);

		width += SmallMargin / 2.0f;
		fraction->MoveXTo(FXCoord::FloorplanInches(width));
		Children.Add(fraction);

		width += fraction->Dimensions.X.AsFloorplanInches();
	}

	if (Sequence.ToString() != FString(""))
	{
		TSharedPtr<FDraftingText> sequence = MakeShareable(new FDraftingText(Sequence, Units::FFontSize::FloorplanInches(FontSize), FMColor::Gray128));

		width += SmallMargin;
		sequence->MoveXTo(Units::FXCoord::FloorplanInches(width));
		Children.Add(sequence);

		width += sequence->GetDimensions(drawingInterface).X.AsFloorplanInches();
	}

	Dimensions.X = FXCoord::FloorplanInches(width);

	// total height is the fontsize plus the displacement
	Dimensions.Y = FYCoord::FloorplanInches(FontSize + LargeMargin);

	return EDrawError::ErrorNone;
}

FMaterialTagSequence::FMaterialTagSequence(const FBIMAssemblySpec &Assembly)
{
	TotalThickness = 0.0f;

	for (auto layer : Assembly.Layers)
	{
		Modumate::Units::FUnitValue t = layer.Thickness;
		FString imperial = UModumateDimensionStatics::DecimalToFractionString(t.AsWorldInches());
		auto imperialList = UModumateDimensionStatics::DecimalToFraction(t.AsWorldInches());
		TotalThickness += t.AsWorldInches();

		// crafting preset namecode/namesequence
		Layers.Add(FMaterialTagElement(*layer.CodeName, imperialList, *layer.PresetSequence));
	}
}

EDrawError FMaterialTagSequence::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{

	float width = 0.0f;

	for (int32 i = 0; i < Layers.Num(); i++)
	{
		auto tag = Layers[i];

		TSharedPtr<FMaterialTag> materialTag = MakeShareable(new FMaterialTag());
		materialTag->Material = FText::FromString(tag.Material);

		// TODO: when the fraction object is made, FMaterialTag will have a separate fraction object
		// and a text object for the sequence value.
		materialTag->ThicknessFraction = tag.FractionComponents;
		materialTag->Sequence = FText::FromString(tag.Sequence);
		materialTag->MoveXTo(FXCoord::FloorplanInches(width));
		materialTag->InitializeBounds(drawingInterface);
		if (i == 0)
		{
			Dimensions.Y = materialTag->Dimensions.Y;
		}

		width += materialTag->Dimensions.X.AsFloorplanInches();

		// Add distance between material tags
		if (i != Layers.Num() - 1)
		{
			width += SmallMargin;
		}

		Children.Add(materialTag);
	}

	Dimensions.X = FXCoord::FloorplanInches(width);

	return EDrawError::ErrorNone;
}

FWallTag::FWallTag(const FBIMAssemblySpec &Assembly)
{
	MaterialTags = MakeShareable(new FMaterialTagSequence(Assembly));
}

EDrawError FWallTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	float width = 0.0f;

	MaterialTags->InitializeBounds(drawingInterface);

	width += MaterialTags->Dimensions.X.AsFloorplanInches();
	width += 2 * Margin;
	float height = MaterialTags->Dimensions.Y.AsFloorplanInches() + 2.0f * Margin;
	Dimensions = FCoordinates2D(FXCoord::FloorplanInches(width), FYCoord::FloorplanInches(height));

	TSharedPtr<FDraftingRectangle> boundingRectangle = MakeShareable(new FDraftingRectangle(Dimensions, FThickness::FloorplanInches(LineWidth), FMColor::Gray32));
	MaterialTags->SetLocalPosition(FXCoord::FloorplanInches(Margin), FYCoord::FloorplanInches(Margin));

	Children.Add(MaterialTags);
	Children.Add(boundingRectangle);

	return EDrawError::ErrorNone;
}

EDrawError FFilledRoundedRectTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	Dimensions.X = Content->Dimensions.X + (OuterMargins.X * 2.0f);
	Dimensions.Y = Content->Dimensions.Y + (OuterMargins.Y * 2.0f);

	// four circles representing the corners of the rounded rect
	TSharedPtr<FFilledCirclePrimitive> filledCircle = MakeShareable(new FFilledCirclePrimitive(CornerRadius, FillColor));
	filledCircle->SetLocalPosition(FXCoord(CornerRadius), FYCoord(CornerRadius));
	Children.Add(filledCircle);

	filledCircle = MakeShareable(new FFilledCirclePrimitive(CornerRadius, FillColor));
	filledCircle->SetLocalPosition(Dimensions.X - FXCoord(CornerRadius), FYCoord(CornerRadius));
	Children.Add(filledCircle);

	filledCircle = MakeShareable(new FFilledCirclePrimitive(CornerRadius, FillColor));
	filledCircle->SetLocalPosition(Dimensions.X - FXCoord(CornerRadius), Dimensions.Y - FYCoord(CornerRadius));
	Children.Add(filledCircle);

	filledCircle = MakeShareable(new FFilledCirclePrimitive(CornerRadius, FillColor));
	filledCircle->SetLocalPosition(FXCoord(CornerRadius), Dimensions.Y - FYCoord(CornerRadius));
	Children.Add(filledCircle);

	// five rectangles filling the remaining space between the circles -
	// one large one that fills the content space between the circles,
	// and two each that fill the remaining horizontal and vertical space between the top and bottom pairs
	FCoordinates2D middleDimensions;
	middleDimensions.X = Content->Dimensions.X;
	middleDimensions.Y = Content->Dimensions.Y;
	TSharedPtr<FFilledRectPrimitive> filledRect = MakeShareable(new FFilledRectPrimitive(middleDimensions, FillColor));
	filledRect->SetLocalPosition(OuterMargins);
	Children.Add(filledRect);

	FCoordinates2D outsideDimensions;
	outsideDimensions.X = Content->Dimensions.X;
	outsideDimensions.Y = OuterMargins.Y;
	filledRect = MakeShareable(new FFilledRectPrimitive(outsideDimensions, FillColor));
	filledRect->SetLocalPosition(FXCoord(CornerRadius), FYCoord::FloorplanInches(0.0f));
	Children.Add(filledRect);

	filledRect = MakeShareable(new FFilledRectPrimitive(outsideDimensions, FillColor));
	filledRect->SetLocalPosition(FXCoord(CornerRadius), Content->Dimensions.Y + OuterMargins.Y);
	Children.Add(filledRect);

	outsideDimensions.X = OuterMargins.X;
	outsideDimensions.Y = Content->Dimensions.Y;
	filledRect = MakeShareable(new FFilledRectPrimitive(outsideDimensions, FillColor));
	filledRect->SetLocalPosition(FXCoord::FloorplanInches(0.0f), FYCoord(CornerRadius));
	Children.Add(filledRect);

	filledRect = MakeShareable(new FFilledRectPrimitive(outsideDimensions, FillColor));
	filledRect->SetLocalPosition(Dimensions.X - FXCoord(CornerRadius), FYCoord(CornerRadius));
	Children.Add(filledRect);

	// force bottom-left alignment of the content and add it
	// the content is added after the fill regions so that it draws afterwards and renders on top
	Content->HorizontalAlignment = DraftingAlignment::Left;
	Content->VerticalAlignment = DraftingAlignment::Bottom;
	Content->SetLocalPosition(OuterMargins);
	Children.Add(Content);

	return EDrawError::ErrorNone;
}

EDrawError FHeaderTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	Content->HorizontalAlignment = DraftingAlignment::Right;
	Content->VerticalAlignment = DraftingAlignment::Bottom;
	Children.Add(Content);

	TSharedPtr<FDraftingText> headerText = MakeShareable(new FDraftingText(HeaderText, FFontSize::FloorplanInches(FontSize), FMColor::Gray144, FontType::Italic));
	headerText->MoveYTo(Content->Dimensions.Y + FYCoord::FloorplanInches(Margin)*2.0f);
	headerText->HorizontalAlignment = DraftingAlignment::Right;
	headerText->VerticalAlignment = DraftingAlignment::Bottom;
	headerText->InitializeBounds(drawingInterface);
	Children.Add(headerText);

	Dimensions.X = headerText->Dimensions.X > Content->Dimensions.X ? headerText->Dimensions.X : Content->Dimensions.X;
	Dimensions.Y = headerText->Dimensions.Y + FYCoord::FloorplanInches(Margin)*2.0f + Content->Dimensions.Y;

	return EDrawError::ErrorNone;
}

EDrawError FMatchLineTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	TSharedPtr<FDraftingLine> matchLine = MakeShareable(new FDraftingLine(PageLength, FThickness::FloorplanInches(MatchLineWidth), Color, Pattern));
	Children.Add(matchLine);

	// four text objects - a reference to the bottom and top drawing at the start and end of the line
	// top drawing text is bottom/left aligned and bottom drawing text is top/left aligned with the same offset from the match line
	TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(TopDrawing,TextFontSize,Color));
	text->SetLocalPosition(PageOffset + TextOffset, MatchLineMargin);
	Children.Add(text);

	text = MakeShareable(new FDraftingText(BottomDrawing,TextFontSize,Color));
	text->SetLocalPosition(PageOffset + TextOffset, MatchLineMargin * -1.0f);
	text->VerticalAlignment = DraftingAlignment::Top;
	Children.Add(text);

	text = MakeShareable(new FDraftingText(TopDrawing,TextFontSize,Color));
	text->SetLocalPosition(FXCoord(PageLength) - (PageOffset + TextOffset), MatchLineMargin);
	text->HorizontalAlignment = DraftingAlignment::Right;
	Children.Add(text);

	text = MakeShareable(new FDraftingText(BottomDrawing,TextFontSize,Color));
	text->SetLocalPosition(FXCoord(PageLength) - (PageOffset + TextOffset), MatchLineMargin * -1.0f);
	text->HorizontalAlignment = DraftingAlignment::Right;
	text->VerticalAlignment = DraftingAlignment::Top;
	Children.Add(text);

	Dimensions.X = FXCoord(PageLength);
	Dimensions.Y = (TextFontSize*2.0f) + (MatchLineMargin*2.0f);

	return EDrawError::ErrorNone;
}

EDrawError FTitleTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	TSharedPtr<FCirclePrimitive> circle = MakeShareable(new FCirclePrimitive(Radius,FThickness::FloorplanInches(LineWidth*2.0f),Color));
	circle->SetLocalPosition(FCoordinates2D(Radius, Radius));
	Children.Add(circle);

	FXCoord maxTextWidth;

	TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(TitleText, FFontSize::FloorplanInches(LargeFontSize),Color));
	text->SetLocalPosition(FCoordinates2D(
		FXCoord::FloorplanInches(LargeMargin) + FXCoord(Radius)*2.0f,
		FYCoord::FloorplanInches(Margin) + FYCoord(Radius)));
	text->InitializeBounds(drawingInterface);
	Children.Add(text);
	maxTextWidth = text->Dimensions.X;

	text = MakeShareable(new FDraftingText(ScaleText, FFontSize::FloorplanInches(FontSize),Color));
	text->SetLocalPosition(FCoordinates2D(
		FXCoord::FloorplanInches(LargeMargin) + FXCoord(Radius)*2.0f,
		FYCoord(Radius) - FYCoord::FloorplanInches(Margin)));
	text->VerticalAlignment = DraftingAlignment::Top;
	text->InitializeBounds(drawingInterface);
	Children.Add(text);
	maxTextWidth = maxTextWidth > text->Dimensions.X ? maxTextWidth : text->Dimensions.X;

	text = MakeShareable(new FDraftingText(NumberText, FFontSize::FloorplanInches(LargeFontSize),Color));
	text->SetLocalPosition(Radius, Radius);
	text->HorizontalAlignment = DraftingAlignment::Center;
	text->VerticalAlignment = DraftingAlignment::Center;
	Children.Add(text);

	FLength lineLength = maxTextWidth + FXCoord::FloorplanInches(LargeMargin) + LineOverrunRight;
	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(lineLength, FThickness::FloorplanInches(LineWidth*2.0f), Color));
	line->SetLocalPosition(FCoordinates2D(Radius*2.0f, Radius));
	Children.Add(line);

	Dimensions.X = lineLength + Radius * 2.0f;
	Dimensions.Y = Radius * 2.0f;

	return EDrawError::ErrorNone;
}

EDrawError FSectionTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{

	// the arrow is two 45/45/90 triangles where the long side is the diameter of the interior circle
	float arrowSide = Radius.AsFloorplanInches()*2 / FMath::Sqrt(2);
	TArray<FVector2D> polyPoints = { FVector2D(0.0f, -arrowSide), FVector2D(arrowSide, 0.0f), FVector2D(0.0f, arrowSide) };
	TSharedPtr<FFilledPolyPrimitive> filledPoly = MakeShareable(new FFilledPolyPrimitive(polyPoints, Color));
	filledPoly->SetLocalOrientation(ArrowAngle);
	Children.Add(filledPoly);

	TSharedPtr<FFilledCirclePrimitive> circleBackground = MakeShareable(new FFilledCirclePrimitive(Radius, FMColor::White));
	Children.Add(circleBackground);

	TSharedPtr<FCirclePrimitive> circle = MakeShareable(new FCirclePrimitive(Radius, FThickness::FloorplanInches(LineWidth), Color));
	Children.Add(circle);

	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(FLength(Radius)*2.0f, FThickness::FloorplanInches(LineWidth), Color));
	line->MoveXTo(Radius * -1.0f);
	Children.Add(line);

	TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(ViewText, FFontSize::FloorplanInches(FontSize), Color));
	text->MoveYTo(FYCoord::FloorplanInches(Margin)*2.0f);
	text->HorizontalAlignment = DraftingAlignment::Center;
	text->InitializeBounds(drawingInterface);
	Children.Add(text);

	text = MakeShareable(new FDraftingText(SheetText, FFontSize::FloorplanInches(FontSize), Color));
	text->MoveYTo(FYCoord::FloorplanInches(Margin)*-2.0f);
	text->HorizontalAlignment = DraftingAlignment::Center;
	text->VerticalAlignment = DraftingAlignment::Center;
	text->InitializeBounds(drawingInterface);
	Children.Add(text);

	Dimensions = FCoordinates2D::FloorplanInches(FVector2D(arrowSide * 2, arrowSide * 2));

	return EDrawError::ErrorNone;
}

EDrawError FScaleTag::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	Units::FXCoord xOffset = Units::FXCoord::FloorplanInches(0.0f);
	Units::FYCoord polyHeight = Units::FYCoord::FloorplanInches(4.0f / 64.0f);
	int lengthTextValue = 0;

	for (int32 idx = 0; idx < Lengths.Num(); idx++)
	{
		Units::FLength length = Lengths[idx];
		Units::FLength scaledLength = length / drawingInterface->DrawingScale;

		Units::FYCoord yOffset = idx % 2 == 0 ? Units::FYCoord::FloorplanInches(0.0f) : polyHeight;

		// filled rectangle, with heights that alternate
		TSharedPtr<FFilledRectPrimitive> filledRect = MakeShareable(new FFilledRectPrimitive(FCoordinates2D(scaledLength, polyHeight), FMColor::Gray128));
		filledRect->SetLocalPosition(xOffset, yOffset);

		TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(FText::AsNumber(lengthTextValue), FFontSize::FloorplanInches(FontSize), FMColor::Gray128));
		text->SetLocalPosition(xOffset, (polyHeight * 2.0f) + FYCoord::FloorplanInches(Margin));
		text->HorizontalAlignment = DraftingAlignment::Center;
		// TODO: assuming feet
		lengthTextValue += (int)(length.AsFloorplanInches() / InchesPerFoot);

		Children.Add(text);
		Children.Add(filledRect);

		xOffset += scaledLength;
	}

	// last text representing the sum of all the lengths
	TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(FText::AsNumber(lengthTextValue), FFontSize::FloorplanInches(FontSize), FMColor::Gray128));
	text->SetLocalPosition(xOffset, (polyHeight * 2.0f) + FYCoord::FloorplanInches(Margin));
	text->HorizontalAlignment = DraftingAlignment::Center;
	Children.Add(text);

	Dimensions.X = xOffset;
	// two levels of filled rects and the text
	Dimensions.Y = (polyHeight * 2.0f) + FYCoord::FloorplanInches(Margin) + FFontSize::FloorplanInches(FontSize);

	return EDrawError::ErrorNone;
}

}
