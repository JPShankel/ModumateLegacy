#include "Drafting/Drawings/Floorplan.h"

#include "Drafting/DraftingManager.h"
#include "Drafting/Drawings/DrawingArea.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"

FFloorplan::FFloorplan(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID)
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

#if 0  // Legacy movement of drawing within sheet:
	worldObjects->SetLocalPosition(FModumateUnitCoord2D::WorldCentimeters(worldObjects->BoundingBox.Min) * -1.0f);
	viewArea->Dimensions = FModumateUnitCoord2D::WorldCentimeters(viewArea->Dimensions.AsWorldCentimeters(DrawingScale));
	worldObjects->TranslateBy(viewArea->Dimensions.X);

	worldObjects->Dimensions = FModumateUnitCoord2D::WorldCentimeters(worldObjects->BoundingBox.GetSize());
	worldObjects->TranslateBy(worldObjects->Dimensions.X * -1.0f);
#endif

	UDraftingManager::OnPageCompleted(CaptureObjID, World);
}
