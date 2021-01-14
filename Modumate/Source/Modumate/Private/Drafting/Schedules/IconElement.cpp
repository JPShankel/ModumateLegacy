#include "Drafting/Schedules/IconElement.h"
#include "Drafting/ModumateDraftingElements.h"

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
		ModumateUnitParams::FYCoord yOffset = ModumateUnitParams::FYCoord::FloorplanInches(0);
		float width = 0.0f;

		for (int i = 0; i < Information->Children.Num(); i++)
		{
			Information->Children[i]->InitializeBounds(drawingInterface);

			yOffset -= Information->Children[i]->Dimensions.Y;
			yOffset -= ModumateUnitParams::FYCoord::FloorplanInches(Margin*2.0f);
			Information->Children[i]->MoveYTo(yOffset);

			width = FMath::Max(width, Information->Children[i]->Dimensions.X.AsFloorplanInches());
		}

		Dimensions.X = ModumateUnitParams::FXCoord::FloorplanInches(width);
		Dimensions.Y = yOffset * -1.0f;

		Dimensions.X += Icon->Dimensions.X + ModumateUnitParams::FXCoord::FloorplanInches(Margin) * 2.0f;
		Dimensions.X += ModumateUnitParams::FXCoord::FloorplanInches(IconMargin) * 2.0f;

		if (Icon->Dimensions.Y > Dimensions.Y)
		{
			Dimensions.Y = Icon->Dimensions.Y;
		}
		Dimensions.Y += ModumateUnitParams::FYCoord::FloorplanInches(IconMargin) * 2.0f;

		Information->SetLocalPosition(Icon->Dimensions.X + ModumateUnitParams::FXCoord::FloorplanInches(Margin) * 2.0f + ModumateUnitParams::FXCoord::FloorplanInches(IconMargin),
			ModumateUnitParams::FYCoord::FloorplanInches(IconMargin) * -1.0f);

		Icon->SetLocalPosition(ModumateUnitParams::FXCoord::FloorplanInches(IconMargin), Dimensions.Y * -1.0f + ModumateUnitParams::FYCoord::FloorplanInches(IconMargin));

		TSharedPtr<FDraftingRectangle> elementRect = MakeShareable(new FDraftingRectangle(Dimensions, ModumateUnitParams::FThickness::Points(0.1f)));
		elementRect->MoveYTo(Dimensions.Y * -1.0f);
		Children.Add(elementRect);

		return EDrawError::ErrorNone;
	}
}
