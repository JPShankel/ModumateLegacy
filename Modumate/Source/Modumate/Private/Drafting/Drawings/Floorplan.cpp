#include "Floorplan.h"

#include "DraftingManager.h"
#include "DrawingArea.h"
#include "ModumateDocument.h"
#include "ModumateObjectInstance.h"

using namespace Modumate::Units;

namespace Modumate {

	FFloorplan::FFloorplan(const FModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID)
		: FDraftingDrawing(doc,world,captureObjID)
	{
		DrawingScale = 48.0f;
	}

	void FFloorplan::OnPageCompleted()
	{
		MakeWorldObjects();

		TSharedPtr<FDraftingComposite> worldObjects = WorldObjects.Pin();
		ensureAlways(worldObjects.IsValid());

		auto viewArea = DrawingContent.Pin();
		ensureAlways(viewArea.IsValid());

		worldObjects->SetLocalPosition(FCoordinates2D::WorldCentimeters(worldObjects->BoundingBox.Min) * -1.0f);
		viewArea->Dimensions = FCoordinates2D::WorldCentimeters(viewArea->Dimensions.AsWorldCentimeters(DrawingScale));
		worldObjects->TranslateBy(viewArea->Dimensions.X);

		viewArea->Children.Add(worldObjects);
		worldObjects->Dimensions = FCoordinates2D::WorldCentimeters(worldObjects->BoundingBox.GetSize());
		worldObjects->TranslateBy(worldObjects->Dimensions.X * -1.0f);

		UDraftingManager::OnPageCompleted(CaptureObjID, World);
	}
}
