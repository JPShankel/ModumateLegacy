#include "IconElement.h"
#include "ModumateDraftingElements.h"

using namespace Modumate::Units;

namespace Modumate {

	FIconElement::FIconElement()
	{
		Information = MakeShareable(new FDraftingComposite());
		Icon = MakeShareable(new FDraftingComposite());
		Children.Add(Information);
		Children.Add(Icon);
	}

	EDrawError FIconElement::InitializeBounds(IModumateDraftingDraw *drawingInterface)
	{
		FYCoord yOffset = FYCoord::FloorplanInches(0);
		float width = 0.0f;

		for (int i = 0; i < Information->Children.Num(); i++)
		{
			Information->Children[i]->InitializeBounds(drawingInterface);

			yOffset -= Information->Children[i]->Dimensions.Y;
			yOffset -= FYCoord::FloorplanInches(Margin*2.0f);
			Information->Children[i]->MoveYTo(yOffset);

			width = FMath::Max(width, Information->Children[i]->Dimensions.X.AsFloorplanInches());
		}

		Dimensions.X = FXCoord::FloorplanInches(width);
		Dimensions.Y = yOffset * -1.0f;

		Dimensions.X += Icon->Dimensions.X + FXCoord::FloorplanInches(Margin) * 2.0f;
		Dimensions.X += FXCoord::FloorplanInches(IconMargin) * 2.0f;

		if (Icon->Dimensions.Y > Dimensions.Y)
		{
			Dimensions.Y = Icon->Dimensions.Y;
		}
		Dimensions.Y += FYCoord::FloorplanInches(IconMargin) * 2.0f;

		Information->SetLocalPosition(Icon->Dimensions.X + FXCoord::FloorplanInches(Margin) * 2.0f + FXCoord::FloorplanInches(IconMargin),
			FYCoord::FloorplanInches(IconMargin) * -1.0f);

		Icon->SetLocalPosition(FXCoord::FloorplanInches(IconMargin), Dimensions.Y * -1.0f + FYCoord::FloorplanInches(IconMargin));

		TSharedPtr<FDraftingRectangle> elementRect = MakeShareable(new FDraftingRectangle(Dimensions, FThickness::Points(0.1f)));
		elementRect->MoveYTo(Dimensions.Y * -1.0f);
		Children.Add(elementRect);

		return EDrawError::ErrorNone;
	}
}
