#include "DrawingArea.h"

namespace Modumate {
	EDrawError FDrawingArea::Draw(IModumateDraftingDraw *drawingInterface,
		Units::FCoordinates2D position,
		Units::FAngle orientation,
		float scale)
	{
		EDrawError error = EDrawError::ErrorNone;
		if (drawingInterface == nullptr)
		{
			error = EDrawError::ErrorBadParam;
			return error;
		}

		ApplyTransform(drawingInterface, position, orientation, scale);

		// Assumes that the dimensions and position of this object specify the proper bounding rectangle of the drawing
		// TODO: Does not account for any orientation or scale effects, potentially could copy location values and call ApplyTransform
		if (bClipped)
		{
			FVector2D epsilon = FVector2D(DEFAULT_OBJECT_EPSILON, DEFAULT_OBJECT_EPSILON);
			FVector2D min = FVector2D(position.X.AsWorldCentimeters(drawingInterface->DrawingScale), position.Y.AsWorldCentimeters(drawingInterface->DrawingScale)) - epsilon;
			FVector2D max = FVector2D((position + Dimensions).X.AsWorldCentimeters(drawingInterface->DrawingScale), (position + Dimensions).Y.AsWorldCentimeters(drawingInterface->DrawingScale)) + epsilon;
			drawingInterface->Viewport = FBox2D(min, max);
		}

		// draw all children with the calculated basis
		for (auto child : Children)
		{
			error = child->Draw(drawingInterface,
				position,
				orientation,
				scale);

			if (error != EDrawError::ErrorNone)
			{
				return error;
			}
		}

		drawingInterface->Viewport.Init();

		return error;
	}
}
