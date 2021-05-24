#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateFunctionLibrary.h"

FDraftingElement::~FDraftingElement() { ClearChildren(); }

void FDraftingElement::SetLocalTransform(FModumateUnitCoord2D position, ModumateUnitParams::FAngle orientation, float scale) {
	SetLocalPosition(position);
	SetLocalOrientation(orientation);
	SetLocalScale(scale);
}

void FDraftingElement::ApplyTransform(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D& position,
	ModumateUnitParams::FAngle& orientation,
	float& scale)
{

	// calculate composite position
	FModumateUnitCoord2D parentPosition = position;

	float x = Position.X.AsWorldCentimeters(drawingInterface->DrawingScale);
	float y = Position.Y.AsWorldCentimeters(drawingInterface->DrawingScale);

	// accommodate for element alignment
	// Default implementation of GetDimensions returns the Dimension member variable -
	// the member variable has default values of zero so that this code has no effect without intention
	FModumateUnitCoord2D dimensions = GetDimensions(drawingInterface);
	float w = dimensions.X.AsWorldCentimeters(drawingInterface->DrawingScale);
	float h = dimensions.Y.AsWorldCentimeters(drawingInterface->DrawingScale);

	// default object rendering and alignment is bottom-left aligned,
	// so center alignment requires subtracting half of the dimension while top/right requires the whole dimension
	switch (VerticalAlignment)
	{
	case DraftingAlignment::Bottom: { h *= 0.0f; break; };
	case DraftingAlignment::Center: { h *= -0.5f; break; };
	case DraftingAlignment::Top: { h *= -1.0f; break; };
	};

	switch (HorizontalAlignment)
	{
	case DraftingAlignment::Left: { w *= 0.0f; break; };
	case DraftingAlignment::Center: { w *= -0.5f; break; };
	case DraftingAlignment::Right: { w *= -1.0f; break; };
	};

	x += w;
	y += h;

	// apply parent's 2D rotation matrix to the position
	float parentRadians = orientation.AsRadians();
	float rx = x * FMath::Cos(parentRadians) - y * FMath::Sin(parentRadians);
	float ry = x * FMath::Sin(parentRadians) + y * FMath::Cos(parentRadians);

	position = parentPosition + FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldCentimeters(rx), ModumateUnitParams::FYCoord::WorldCentimeters(ry));

	// calculate composite orientation
	float parentDegrees = orientation.AsDegrees();
	float compositeDegrees = Orientation.AsDegrees();

	float degrees = FRotator::ClampAxis(parentDegrees + compositeDegrees);
	orientation = ModumateUnitParams::FAngle::Degrees(degrees);

	// calculate composite scale
	scale = scale * Scale;

}

FBox2D FDraftingElement::MakeAABB(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	return BoundingBox;
}

void FDraftingElement::ClearChildren()
{
	Children.Empty();
}

void FDraftingElement::SetLayerTypeRecursive(FModumateLayerType layerType)
{
	LayerType = layerType;
	for (auto& child: Children)
	{
		child->SetLayerTypeRecursive(layerType);
	}
}

EDrawError FDraftingComposite::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	// draw all children with the calculated basis
	for (auto& child : Children)
	{
		// skip drawing the child if the bounding box is disjoint from the viewport
		if (child->BoundingBox.bIsValid && drawingInterface->Viewport.bIsValid && !drawingInterface->Viewport.Intersect(child->BoundingBox))
		{
			continue;
		}

		error = child->Draw(drawingInterface,
			position,
			orientation,
			scale);

		// Keep going for unimplemented. MOD-290
		if (error != EDrawError::ErrorNone && error != EDrawError::ErrorUnimplemented)
		{
			return error;
		}
	}

	return error;
}

FModumateUnitCoord2D FDraftingComposite::GetDimensions(IModumateDraftingDraw *drawingInterface)
{
	return Dimensions;
}

EDrawError FDraftingComposite::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	for (auto& child : Children)
	{
		error = child->InitializeBounds(drawingInterface);

		if (error != EDrawError::ErrorNone)
		{
			return error;
		}
	}

	return error;
}

FBox2D FDraftingComposite::MakeAABB(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	FBox2D aabb = FBox2D();
	aabb.bIsValid = false;

	ApplyTransform(drawingInterface, position, orientation, scale);

	for (TSharedPtr<FDraftingElement> c : Children)
	{
		// TODO: validity flags in children to avoid duplicate work
		FBox2D childBox = FBox2D(EForceInit::ForceInitToZero);
		//if (c->BoundingBox.bIsValid)
		//{
		//	childBox = c->BoundingBox;
		//}
		//else
		//{
			childBox = c->MakeAABB(drawingInterface, position, orientation, scale);
		//}

		if (childBox.bIsValid)
		{
			if (!aabb.bIsValid)
			{
				aabb = childBox;
			}
			else
			{
				aabb.Min = FVector2D::Min(aabb.Min, childBox.Min);
				aabb.Max = FVector2D::Max(aabb.Max, childBox.Max);
			}
		}
	}

	BoundingBox = aabb;

	return aabb;
}

EDrawError FTextPrimitive::Draw(IModumateDraftingDraw *drawingInterface,
		FModumateUnitCoord2D position,
		ModumateUnitParams::FAngle orientation,
		float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	// sanitize orientation value so that text always faces up,
	// then adjust alignment values to keep position consistent
	float expectedOrientation = FRotator::ClampAxis(orientation.AsDegrees());
	if (expectedOrientation > 90.0f && expectedOrientation < 270.0f)
	{
		orientation = ModumateUnitParams::FAngle::Degrees(FRotator::ClampAxis(orientation.AsDegrees() + 180.0f));

		switch (TextAlignment)
		{
		case DraftingAlignment::Left: { TextAlignment = DraftingAlignment::Right; break; };
		case DraftingAlignment::Right: { TextAlignment = DraftingAlignment::Left; break; };
		};
	}

	// don't draw object if it is outside of the drawable region
	if (drawingInterface->Viewport.bIsValid && BoundingBox.bIsValid && !drawingInterface->Viewport.IsInside(BoundingBox))
	{
		return error;
	}

	// TODO: if AddText can take a vertical alignment value, then FDraftingText::Draw can be removed
	//		 if AddText rendering is centered by default, the extra code in this function above can be removed as well
	error = drawingInterface->AddText(*Text, FontSize, position.X, position.Y, orientation, Color, TextAlignment,
		ContainerWidth, TextFontName, LayerType);

	return error;
}

FBox2D FTextPrimitive::MakeAABB(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	ApplyTransform(drawingInterface, position, orientation, scale);

	// TODO: this is probably the only object where this makes a difference
	// also has code allowing this code to safely be run more than once
	float width = GetWidth(drawingInterface);
	float height = FontSize.AsWorldCentimeters(drawingInterface->DrawingScale);

	FVector2D vPosition = FVector2D(position.X.AsWorldCentimeters(drawingInterface->DrawingScale), position.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FVector2D vDimX = FVector2D(width, 0.0f).GetRotated(orientation.AsDegrees());
	FVector2D vDimY = FVector2D(0.0f, height).GetRotated(orientation.AsDegrees());

	FBox2D textAABB = FBox2D({
		vPosition,
		vPosition + vDimX,
		vPosition + vDimY,
		vPosition + vDimX + vDimY
	});
	textAABB.bIsValid = true;

	BoundingBox = textAABB;

	return textAABB;
}

float FTextPrimitive::GetWidth(IModumateDraftingDraw *drawingInterface)
{
	FModumateUnitValue len = FModumateUnitValue::Points(0);
	drawingInterface->GetTextLength(*Text, FontSize, len, TextFontName);
	return len.AsFloorplanInches(drawingInterface->DrawingScale);
}

FDraftingText::FDraftingText(
	FText text,
	ModumateUnitParams::FFontSize fontSize,
	FMColor color,
	FontType type,
	DraftingAlignment justify,
	ModumateUnitParams::FAngle angle,
	ModumateUnitParams::FWidth width)
{
	// save child for use with getting dimensions later on
	Child = MakeShareable(new FTextPrimitive(text.ToString(), fontSize, color, type, justify, angle, width));

	Children.Add(Child);
}

EDrawError FDraftingText::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	// sanitize orientation value so that text always faces up,
	// then adjust alignment values to keep position consistent
	float expectedOrientation = FRotator::ClampAxis(orientation.AsDegrees() + Orientation.AsDegrees());
	if (expectedOrientation > 90.0f && expectedOrientation < 270.0f)
	{
		Position.Y = ModumateUnitParams::FYCoord::WorldCentimeters(Position.Y.AsWorldCentimeters(drawingInterface->DrawingScale) + GetDimensions(drawingInterface).Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	}
	return FDraftingComposite::Draw(drawingInterface, position, orientation, scale);
}

EDrawError FDraftingText::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	if (!FMath::IsNearlyZero(Dimensions.X.AsFloorplanInches()) || !FMath::IsNearlyZero(Dimensions.Y.AsFloorplanInches()))
	{
		// Already initialized
		return EDrawError::ErrorException;
	}

	ensure(Child->FontSize.GetUnitType() == EModumateUnitType::FontHeight || Child->FontSize.GetUnitType() == EModumateUnitType::FloorplanInches);
	ensure(Child->ContainerWidth.GetUnitType() == EModumateUnitType::FloorplanInches);

	float width = Child->GetWidth(drawingInterface);
	float height = Child->FontSize.AsFloorplanInches();

	float maxWidth = Child->ContainerWidth.AsFloorplanInches();

	// TODO: only handles spaces right now, may need to handle other types of whitespace eventually

	// attempt to wrap only if ContainerWidth has been set and there would be more than one row
	if (maxWidth > 0.0f && maxWidth < width)
	{
		// remove single child and replace it with split up text objects representing each line
		Children.Remove(Child);

		TArray<FString> words;

		FString splitString = Child->Text;
		FString word;
		FString remainder;

		// TODO: when it is necessary to have multiple types of whitespace, use FChar::IsWhiteSpace
		// and then measure the size of each different whitespace below
		// One day ICULineBreakIterator could be useful here as well
		while (splitString.Split(Delimiter, &word, &remainder))
		{
			splitString = remainder;
			words.Add(word);
			word.Empty();
		}
		words.Add(splitString);

		FModumateUnitValue len;
		drawingInterface->GetTextLength(*Delimiter,Child->FontSize,len,Child->TextFontName);
		float delimiterSize = len.AsFloorplanInches();

		FString currentLine;
		float xOffset = 0.0f;
		Dimensions.X = ModumateUnitParams::FXCoord::FloorplanInches(0.0f);

		for (auto w : words)
		{
			drawingInterface->GetTextLength(*w,Child->FontSize,len,Child->TextFontName);
			float current = len.AsFloorplanInches();

			if (xOffset + delimiterSize + current > maxWidth)
			{
				if (xOffset > Dimensions.X.AsFloorplanInches())
				{
					Dimensions.X = ModumateUnitParams::FXCoord::FloorplanInches(xOffset);
				}

				Children.Add(MakeShareable(new FTextPrimitive(currentLine, Child->FontSize, Child->Color, Child->TextFontName, Child->TextAlignment, Child->Angle, Child->ContainerWidth)));
				xOffset = 0.0f;
				currentLine.Empty();
			}

			xOffset += delimiterSize + current;
			if (!currentLine.IsEmpty())
			{
				currentLine += Delimiter;
			}
			currentLine += w;
		}
		Children.Add(MakeShareable(new FTextPrimitive(currentLine, Child->FontSize, Child->Color, Child->TextFontName, Child->TextAlignment, Child->Angle, Child->ContainerWidth)));

		float yOffset = RowMargin;
		for (int32 i = Children.Num()-1; i >= 0; i--)
		{
			Children[i]->MoveYTo(ModumateUnitParams::FYCoord::FloorplanInches(yOffset));

			yOffset += Child->FontSize.AsFloorplanInches() + RowMargin;
		}
		Dimensions.Y = ModumateUnitParams::FYCoord::FloorplanInches(yOffset);

	}
	else
	{
		Dimensions = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(width), ModumateUnitParams::FYCoord::FloorplanInches(height));
	}


	return EDrawError::ErrorNone;
}

FModumateUnitCoord2D FDraftingText::GetDimensions(IModumateDraftingDraw *drawingInterface)
{
	// TODO: this is inconsistent with the rest of the objects; potentially GetDimensions can be deprecated entirely,
	// since Dimensions has been promoted to FDraftingElement and should be populated through InitializeBounds
	if (FMath::IsNearlyZero(Dimensions.X.AsFloorplanInches()) && FMath::IsNearlyZero(Dimensions.Y.AsFloorplanInches()))
	{
		InitializeBounds(drawingInterface);
	}

	return Dimensions;
}

FDraftingTick::FDraftingTick(
	ModumateUnitParams::FLength length,
	ModumateUnitParams::FThickness thickness,
	FMColor color,
	LinePattern linePattern,
	ModumateUnitParams::FPhase phase)
{
	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(length,thickness,color,linePattern,phase));
	line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(45));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(length,thickness,color,linePattern,phase));
	line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(225));
	Children.Add(line);
}

FDraftingRectangle::FDraftingRectangle(
	FModumateUnitCoord2D dimensions,
	ModumateUnitParams::FThickness thickness,
	FMColor color,
	LinePattern linePattern,
	ModumateUnitParams::FPhase phase)
{
	Dimensions = dimensions;

	TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(ModumateUnitParams::FLength(dimensions.X), thickness, color));
	line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(0));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(ModumateUnitParams::FLength(dimensions.X), thickness, color));
	line->SetLocalPosition(FModumateUnitCoord2D(dimensions.X*0.0f, dimensions.Y));
	line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(0));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(ModumateUnitParams::FLength(dimensions.Y), thickness, color));
	line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90));
	Children.Add(line);

	line = MakeShareable(new FDraftingLine(ModumateUnitParams::FLength(dimensions.Y), thickness, color));
	line->SetLocalPosition(FModumateUnitCoord2D(dimensions.X, dimensions.Y*0.0f));
	line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90));
	Children.Add(line);
}

FDraftingFraction::FDraftingFraction(FText numerator, FText denominator, ModumateUnitParams::FFontSize fontSize, ModumateUnitParams::FThickness lineWidth, FMColor color)
	: Numerator(numerator), Denominator(denominator), FontSize(fontSize), LineWidth(lineWidth), Color(color)
{

}

EDrawError FDraftingFraction::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	TSharedPtr<FDraftingText> numeratorText = MakeShareable(new FDraftingText(Numerator, FontSize, Color));
	TSharedPtr<FDraftingText> denominatorText = MakeShareable(new FDraftingText(Denominator, FontSize, Color));

	// derive tick length (line at 45 degrees) matching half of the text height
	ModumateUnitParams::FLength tickLength = ModumateUnitParams::FLength::WorldCentimeters(FontSize.AsWorldCentimeters(drawingInterface->DrawingScale) / 2.0f) * FMath::Sqrt(2);
	TSharedPtr<FDraftingTick> divider = MakeShareable(new FDraftingTick(tickLength, LineWidth, Color));

	FModumateUnitCoord2D numeratorDimensions = numeratorText->GetDimensions(drawingInterface);
	FModumateUnitCoord2D denominatorDimensions = denominatorText->GetDimensions(drawingInterface);

	numeratorText->SetLocalPosition(numeratorDimensions.X, denominatorDimensions.Y);
	numeratorText->HorizontalAlignment = DraftingAlignment::Right;

	divider->SetLocalPosition(numeratorDimensions.X, denominatorDimensions.Y);

	denominatorText->MoveXTo(numeratorDimensions.X);

	Children.Add(numeratorText);
	Children.Add(denominatorText);
	Children.Add(divider);

	Dimensions.X = numeratorDimensions.X + denominatorDimensions.X;
	Dimensions.Y = numeratorDimensions.Y + denominatorDimensions.Y;

	return EDrawError::ErrorNone;
}

FDraftingSwingDoor::FDraftingSwingDoor(ModumateUnitParams::FLength doorLength, ModumateUnitParams::FLength doorDepth) : DoorLength(doorLength), DoorDepth(doorDepth)
{
	// two frame lines along the side of the door
	// TODO: delete this entirely 
	TSharedPtr<FDraftingLine> frameLine = MakeShareable(new FDraftingLine(doorDepth, DoorFrameThickness));
	frameLine->MoveXTo(ModumateUnitParams::FXCoord(DoorFrameThickness) / 2.0f * -1.0f);
	frameLine->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90.0f));
	Children.Add(frameLine);

	frameLine = MakeShareable(new FDraftingLine(doorDepth, DoorFrameThickness));
	frameLine->MoveXTo(ModumateUnitParams::FXCoord(doorLength + DoorFrameThickness / 2.0f));
	frameLine->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90.0f));
	Children.Add(frameLine);

	// door rect next to right side
	FModumateUnitCoord2D doorDimensions = FModumateUnitCoord2D(ModumateUnitParams::FXCoord(DoorThickness), ModumateUnitParams::FYCoord(doorLength));
	TSharedPtr<FDraftingRectangle> doorRect = MakeShareable(new FDraftingRectangle(doorDimensions, DoorFrameThickness/2.0f));
	doorRect->SetLocalPosition(ModumateUnitParams::FXCoord(DoorLength - DoorThickness), ModumateUnitParams::FYCoord(DoorDepth));
	//doorRect->Orientation = FAngle::Degrees(90.0f);
	Children.Add(doorRect);

	// arc connecting opposite frame to door
	ModumateUnitParams::FRadius arcRadius = ModumateUnitParams::FRadius::WorldCentimeters(doorLength.AsWorldCentimeters());

	// TODO: see line pattern definition, this should be a TArray
	std::vector<double> pattern = { 8.0, 4.0 };
	LinePattern arcLinePattern;
	arcLinePattern.LineStyle = DraftingLineStyle::Dashed;
	arcLinePattern.DashPattern = pattern;

	TSharedPtr<FDraftingArc> doorArc = MakeShareable(new FDraftingArc(arcRadius, ModumateUnitParams::FAngle::Degrees(90), ModumateUnitParams::FThickness::Points(0.25f), FMColor::Gray144, arcLinePattern));
	doorArc->SetLocalPosition(ModumateUnitParams::FXCoord::WorldCentimeters(DoorLength.AsWorldCentimeters()), doorRect->GetLocalPosition().Y);
	doorArc->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90.0f));
	Children.Add(doorArc);

	// TODO: not sure whether this primitive should contain the thicknesses of the frameline, which extend outside of this dimensioning
	Dimensions.X = ModumateUnitParams::FXCoord(doorLength);
	Dimensions.Y = ModumateUnitParams::FYCoord(doorLength) + ModumateUnitParams::FYCoord(doorDepth);
}

FDraftingLine::FDraftingLine(
	FModumateUnitCoord2D P1,
	FModumateUnitCoord2D P2,
	ModumateUnitParams::FThickness thickness,
	FMColor color,
	LinePattern linePattern,
	ModumateUnitParams::FPhase phase)
{
	// potentially, Position and Orientation could be assigned to the FLinePrimitive child
	Position = P1;

	FModumateUnitCoord2D difference = P2 - P1;
	Orientation = ModumateUnitParams::FAngle::Radians(FMath::Atan2(difference.Y.AsWorldCentimeters(), difference.X.AsWorldCentimeters()));

	Children.Add(MakeShareable(new FLinePrimitive(difference.Size(), thickness, color, linePattern, phase)));
}

EDrawError FLinePrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	FModumateUnitCoord2D TopRight;
	float length = Length.AsWorldCentimeters(drawingInterface->DrawingScale);
	float radians = orientation.AsRadians();

	TopRight.X = ModumateUnitParams::FXCoord::WorldCentimeters(position.X.AsWorldCentimeters(drawingInterface->DrawingScale) + length * FMath::Cos(radians));
	TopRight.Y = ModumateUnitParams::FYCoord::WorldCentimeters(position.Y.AsWorldCentimeters(drawingInterface->DrawingScale) + length * FMath::Sin(radians));

	// values for drawingInterface view rectangle should be set at the top-level, first draw call
	if (drawingInterface->Viewport.bIsValid)
	{
		FVector2D clippedStart;
		FVector2D clippedEnd;

		if (UModumateFunctionLibrary::ClipLine2DToRectangle(
			FVector2D(position.X.AsWorldCentimeters(drawingInterface->DrawingScale), position.Y.AsWorldCentimeters(drawingInterface->DrawingScale)),
			FVector2D(TopRight.X.AsWorldCentimeters(drawingInterface->DrawingScale), TopRight.Y.AsWorldCentimeters(drawingInterface->DrawingScale)),
			drawingInterface->Viewport,
			clippedStart,
			clippedEnd))
		{
			position = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldCentimeters(clippedStart.X), ModumateUnitParams::FYCoord::WorldCentimeters(clippedStart.Y));
			TopRight = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldCentimeters(clippedEnd.X), ModumateUnitParams::FYCoord::WorldCentimeters(clippedEnd.Y));
			error = drawingInterface->DrawLine(position.X, position.Y, TopRight.X, TopRight.Y, LineWidth, Color,
				Pattern, Phase, LayerType);
		}

	}
	else
	{
		error = drawingInterface->DrawLine(position.X, position.Y, TopRight.X, TopRight.Y, LineWidth, Color,
			Pattern, Phase, LayerType);
	}

	return error;
}

FBox2D FLinePrimitive::MakeAABB(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	ApplyTransform(drawingInterface, position, orientation, scale);

	FVector2D vPosition;
	vPosition.X = position.X.AsWorldCentimeters(drawingInterface->DrawingScale);
	vPosition.Y = position.Y.AsWorldCentimeters(drawingInterface->DrawingScale);

	FVector2D TopRight;
	float length = Length.AsWorldCentimeters(drawingInterface->DrawingScale);
	float radians = orientation.AsRadians();

	TopRight.X = position.X.AsWorldCentimeters(drawingInterface->DrawingScale) + length * FMath::Cos(radians);
	TopRight.Y = position.Y.AsWorldCentimeters(drawingInterface->DrawingScale) + length * FMath::Sin(radians);

	FBox2D lineAABB;
	lineAABB.Min = FVector2D::Min(vPosition, TopRight);
	lineAABB.Max = FVector2D::Max(vPosition, TopRight);
	lineAABB.bIsValid = true;

	BoundingBox = lineAABB;

	return lineAABB;
}

/*
FModumateUnitCoord2D FDraftingLineComposite::GetDimensions(IModumateDraftingDraw *drawingInterface)
{
	return FModumateUnitCoord2D(FXCoord::WorldCentimeters(Length.AsWorldCentimeters()), FYCoord::WorldCentimeters(LineWidth.AsWorldCentimeters()));
}
//*/

EDrawError FArcPrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	// don't draw object if it is outside of the drawable region
	if (drawingInterface->Viewport.bIsValid && BoundingBox.bIsValid && !drawingInterface->Viewport.IsInside(BoundingBox))
	{
		return error;
	}

	float end = FRotator::ClampAxis(orientation.AsDegrees() + Angle.AsDegrees());
	int slices = (int)(Angle.AsDegrees() * 0.2f);

	error = drawingInterface->DrawArc(position.X, position.Y, orientation, ModumateUnitParams::FAngle::Degrees(end), Radius,
		LineWidth, Color, Pattern, slices, LayerType);

	return error;
}

FBox2D FArcPrimitive::MakeAABB(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	ApplyTransform(drawingInterface, position, orientation, scale);

	TArray<FVector2D> boundingPoints;
	FVector2D vPosition = FVector2D(position.X.AsWorldCentimeters(drawingInterface->DrawingScale), position.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	boundingPoints.Add(vPosition);

	float radius = Radius.AsWorldCentimeters(drawingInterface->DrawingScale);

	float startDeg = orientation.AsDegrees();
	float endDeg = orientation.AsDegrees() + Angle.AsDegrees();

	boundingPoints.Add(vPosition + FVector2D(radius, 0.0f).GetRotated(startDeg));
	boundingPoints.Add(vPosition + FVector2D(radius, 0.0f).GetRotated(endDeg));

	for (int32 i = 0; i <= 270; i += 90)
	{
		if (FRotator::ClampAxis(startDeg-i) > FRotator::ClampAxis(endDeg-i))
		{
			boundingPoints.Add(vPosition + FVector2D(radius, 0.0f).GetRotated(i));
		}
	}

	FBox2D arcAABB = FBox2D(boundingPoints);

	BoundingBox = arcAABB;

	return arcAABB;
}

FFilledCirclePrimitive::FFilledCirclePrimitive(ModumateUnitParams::FRadius radius, FMColor color) : Radius(radius), Color(color)
{
	Dimensions.X = ModumateUnitParams::FXCoord(Radius * 2.0f);
	Dimensions.Y = ModumateUnitParams::FYCoord(Radius * 2.0f);
}

EDrawError FFilledCirclePrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	// don't draw object if it is outside of the drawable region
	if (drawingInterface->Viewport.bIsValid && BoundingBox.bIsValid && !drawingInterface->Viewport.IsInside(BoundingBox))
	{
		return error;
	}

	drawingInterface->FillCircle(position.X, position.Y, Radius, Color, LayerType);

	return error;
}

FCirclePrimitive::FCirclePrimitive(ModumateUnitParams::FRadius radius, ModumateUnitParams::FThickness thickness, FMColor color, LinePattern linePattern) : Radius(radius), LineWidth(thickness), Color(color), Pattern(linePattern)
{
	// empty
}

EDrawError FCirclePrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	if (drawingInterface == nullptr)
	{
		return EDrawError::ErrorBadParam;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	// don't draw object if it is outside of the drawable region
	if (drawingInterface->Viewport.bIsValid && BoundingBox.bIsValid && !drawingInterface->Viewport.IsInside(BoundingBox))
	{
		return EDrawError::ErrorNone;
	}

	drawingInterface->DrawCircle(position.X, position.Y, Radius, LineWidth, Pattern, Color, LayerType);

	return EDrawError::ErrorNone;
}

FFilledRectPrimitive::FFilledRectPrimitive(FModumateUnitCoord2D dimensions, FMColor color) : Color(color)
{
	Dimensions = dimensions;
}

EDrawError FFilledRectPrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	FModumateUnitCoord2D offsetDimensions = FModumateUnitCoord2D::WorldCentimeters(Dimensions.AsWorldCentimeters(drawingInterface->DrawingScale));
	// TODO: based on this code, FFilledRectPrimitives may not be able to respond to orientation
	TArray<FModumateUnitCoord2D> corners;
	corners.Add(position);
	corners.Add(position + FModumateUnitCoord2D(offsetDimensions.X, ModumateUnitParams::FYCoord::WorldCentimeters(0.0f)));
	corners.Add(position + offsetDimensions);
	corners.Add(position + FModumateUnitCoord2D(ModumateUnitParams::FXCoord::WorldCentimeters(0.0f), offsetDimensions.Y));

	// don't draw object if it is outside of the drawable region
	if (drawingInterface->Viewport.bIsValid && BoundingBox.bIsValid && !drawingInterface->Viewport.IsInside(BoundingBox))
	{
		return error;
	}

	TArray<float> points;
	for (int i = 0; i < corners.Num(); i++)
	{
		auto& coord = corners[i];

		points.Add(coord.X.AsPoints(drawingInterface->DrawingScale));
		points.Add(coord.Y.AsPoints(drawingInterface->DrawingScale));
	}

	drawingInterface->FillPoly(points.GetData(), points.Num()/2, Color, LayerType);

	return error;
}

EDrawError FFilledPolyPrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	ApplyTransform(drawingInterface, position, orientation, scale);

	// unzip member into X and Y components
	TArray<float> points;
	TArray<FVector2D> transformedPoints;
	for (int32 i = 0; i < PolyPoints.Num(); i++)
	{
		auto coord = PolyPoints[i];
		coord = coord.GetRotated(orientation.AsDegrees());
		coord.X += position.X.AsWorldCentimeters(drawingInterface->DrawingScale) * scale;
		coord.Y += position.Y.AsWorldCentimeters(drawingInterface->DrawingScale) * scale;

		transformedPoints.Add(coord);

	}

	for (int32 i = 0; i < transformedPoints.Num(); i++)
	{
		auto coord = transformedPoints[i];
		auto nextCoord = transformedPoints[(i + 1) % transformedPoints.Num()];

		if (drawingInterface->Viewport.bIsValid)
		{
			// TODO: correct for axis-aligned rectangle only
			// can be used for the axis-aligned elevation views instead of the code below
			//coord = drawingInterface->Viewport.GetClosestPointTo(coord);
			//points.Add(coord.X);
			//points.Add(coord.Y);

			FVector2D outStart, outEnd;
			if (UModumateFunctionLibrary::ClipLine2DToRectangle(coord, nextCoord, drawingInterface->Viewport, outStart, outEnd))
			{
				// determine which points are significant to the polygon loop when clipped to the viewport
				for (auto& point : { outStart, outEnd })
				{
					FVector2D prevPoint = FVector2D(EForceInit::ForceInitToZero);
					if (points.Num() > 1)
					{
						prevPoint.X = points[points.Num() - 2];
						prevPoint.Y = points[points.Num() - 1];

						if (!(prevPoint - point).IsNearlyZero())
						{
							points.Add(point.X);
							points.Add(point.Y);
						}
					}
					else
					{
						points.Add(point.X);
						points.Add(point.Y);
					}
				}
			}
		}

		else
		{
			points.Add(coord.X);
			points.Add(coord.Y);
		}
	}

	if (points.Num() / 2 >= 3)
	{
		drawingInterface->FillPoly(points.GetData(), points.Num()/2, Color, LayerType);
	}

	return EDrawError::ErrorNone;
}

FImagePrimitive::FImagePrimitive(FString imagePath, FModumateUnitCoord2D dimensions) : Path(imagePath)
{
	Dimensions = dimensions;
}

EDrawError FImagePrimitive::Draw(IModumateDraftingDraw *drawingInterface,
	FModumateUnitCoord2D position,
	ModumateUnitParams::FAngle orientation,
	float scale)
{
	ApplyTransform(drawingInterface, position, orientation, scale);

	// don't draw object if it is outside of the drawable region
	if (drawingInterface->Viewport.bIsValid && BoundingBox.bIsValid && !drawingInterface->Viewport.IsInside(BoundingBox))
	{
		return EDrawError::ErrorNone;
	}

	drawingInterface->AddImage(*Path, position.X, position.Y, Dimensions.X, Dimensions.Y, LayerType);

	return EDrawError::ErrorNone;
}

FDimensionPrimitive::FDimensionPrimitive(FModumateUnitCoord2D start,
	FModumateUnitCoord2D end,
	FModumateUnitCoord2D position,
	FMColor color /*= FMColor::Black*/)
	: Start(start), End(end), StringPosition(position), Color(color)
{ }

EDrawError FDimensionPrimitive::Draw(IModumateDraftingDraw* drawingInterface,
	FModumateUnitCoord2D position /*= FModumateUnitCoord2D()*/,
	ModumateUnitParams::FAngle orientation /*= ModumateUnitParams::FAngle::Radians(0)*/,
	float scale /*= 1.0f*/)
{
	EDrawError error = EDrawError::ErrorNone;
	if (drawingInterface == nullptr)
	{
		error = EDrawError::ErrorBadParam;
		return error;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	FVector2D pos(position.X.AsWorldCentimeters(drawingInterface->DrawingScale), position.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FTransform2D xform(FQuat2D(orientation.AsRadians()) );
	xform.Concatenate(FTransform2D(scale));
	xform.SetTranslation(pos);

	FVector2D P1(Start.X.AsWorldCentimeters(drawingInterface->DrawingScale), Start.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FVector2D P2(End.X.AsWorldCentimeters(drawingInterface->DrawingScale), End.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FVector2D P3(StringPosition.X.AsWorldCentimeters(drawingInterface->DrawingScale), StringPosition.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	P1 = xform.TransformPoint(P1);
	P2 = xform.TransformPoint(P2);
	P3 = xform.TransformPoint(P3);

	return drawingInterface->AddDimension(
		ModumateUnitParams::FXCoord::WorldCentimeters(P1.X),
		ModumateUnitParams::FXCoord::WorldCentimeters(P1.Y),
		ModumateUnitParams::FXCoord::WorldCentimeters(P2.X),
		ModumateUnitParams::FXCoord::WorldCentimeters(P2.Y),
		ModumateUnitParams::FXCoord::WorldCentimeters(P3.X),
		ModumateUnitParams::FXCoord::WorldCentimeters(P3.Y),
		Color, LayerType);
}

FAngularDimensionPrimitive::FAngularDimensionPrimitive(FModumateUnitCoord2D start,
	FModumateUnitCoord2D end,
	FModumateUnitCoord2D center,
	FMColor color /*= FMColor::Black*/)
	: Start(start), End(end), Center(center), Color(color)
{ }

EDrawError FAngularDimensionPrimitive::Draw(IModumateDraftingDraw* drawingInterface,
	FModumateUnitCoord2D position /*= FModumateUnitCoord2D()*/,
	ModumateUnitParams::FAngle orientation /*= ModumateUnitParams::FAngle::Radians(0)*/,
	float scale /*= 1.0f*/)
{
	if (drawingInterface == nullptr)
	{
		return EDrawError::ErrorBadParam;
	}

	ApplyTransform(drawingInterface, position, orientation, scale);

	FVector2D pos(position.X.AsWorldCentimeters(drawingInterface->DrawingScale), position.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FTransform2D xform(FQuat2D(orientation.AsRadians()));
	xform.Concatenate(FTransform2D(scale));
	xform.SetTranslation(pos);

	FVector2D P1(Start.X.AsWorldCentimeters(drawingInterface->DrawingScale), Start.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FVector2D P2(End.X.AsWorldCentimeters(drawingInterface->DrawingScale), End.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	FVector2D P3(Center.X.AsWorldCentimeters(drawingInterface->DrawingScale), Center.Y.AsWorldCentimeters(drawingInterface->DrawingScale));
	P1 = xform.TransformPoint(P1);
	P2 = xform.TransformPoint(P2);
	P3 = xform.TransformPoint(P3);

	return drawingInterface->AddAngularDimension(
		ModumateUnitParams::FXCoord::WorldCentimeters(P1.X),
		ModumateUnitParams::FXCoord::WorldCentimeters(P1.Y),
		ModumateUnitParams::FXCoord::WorldCentimeters(P2.X),
		ModumateUnitParams::FXCoord::WorldCentimeters(P2.Y),
		ModumateUnitParams::FXCoord::WorldCentimeters(P3.X),
		ModumateUnitParams::FXCoord::WorldCentimeters(P3.Y),
		Color, LayerType);
}
