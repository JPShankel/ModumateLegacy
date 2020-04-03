#include "SummaryList.h"

#include "IconElement.h"

using namespace Modumate::Units;

namespace Modumate {
	EDrawError FSummaryList::InitializeBounds(IModumateDraftingDraw *drawingInterface)
	{
		EDrawError error = EDrawError::ErrorNone;

		if (Title != nullptr)
		{
			Title->InitializeBounds(drawingInterface);
			Title->MoveYTo(Title->Dimensions.Y * -1.0f);

			Dimensions.Y = Title->Dimensions.Y + FYCoord::FloorplanInches(TitleMargin);
		}


		FXCoord xOffset = FXCoord::FloorplanInches(0.0f);
		FYCoord yOffset = (Dimensions.Y) * -1.0f;
		FYCoord maxIconY = FYCoord::FloorplanInches(0.0f);
		bool bHasMultipleRows = false;

		for (int32 i = 0; i < ffeIconElements.Num(); i++)
		{
			auto iconElement = ffeIconElements[i];
			iconElement->InitializeBounds(drawingInterface);

			FXCoord iconDimX = iconElement->Dimensions.X;
			// row is filled with data when the width limit is exceeded, reset the variables to start a new row
			if (xOffset + iconDimX > maxDimensionX)
			{
				Dimensions.X = xOffset > Dimensions.X ? xOffset : Dimensions.X;
				xOffset = FXCoord::FloorplanInches(0.0f);
				bHasMultipleRows = true;

				Dimensions.Y += maxIconY;
				yOffset += (maxIconY) * -1.0f;
				maxIconY = FYCoord::FloorplanInches(0.0f);
			}

			iconElement->SetLocalPosition(xOffset, yOffset);
			Children.Add(iconElement);

			xOffset += iconDimX;

			// when there is only one row, the dimension should match the combined width of the elements
			// when there are multiple rows, the dimension should match the width of the widest row
			if (!bHasMultipleRows)
			{
				Dimensions.X += iconDimX;
			}

			maxIconY = iconElement->Dimensions.Y > maxIconY ? iconElement->Dimensions.Y : maxIconY;

		}
		FXCoord TitleX = Title->GetDimensions(drawingInterface).X;
		Dimensions.X = Dimensions.X > TitleX ? Dimensions.X : TitleX;
		Dimensions.Y += maxIconY;

		return error;
	}
}
