// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/DraftingManager.h"

class UWorld;
class MODUMATE_API UModumateDocument;

namespace Modumate {

	class FDrawingArea;

	class FDraftingDrawing : public FDraftingComposite
	{
	public:
		FDraftingDrawing(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID);

		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

		virtual bool InitializeDimensions(FModumateUnitCoord2D drawingSize, FModumateUnitCoord2D drawingMargin);

		virtual bool MakeTitleTag(IModumateDraftingDraw *drawingInterface);

		virtual bool MakeScaleTag(IModumateDraftingDraw *drawingInterface);

		virtual void OnPageCompleted() {};

		int32 ID;

		void GetForegroundLines(TSharedPtr<FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting);
		virtual bool MakeWorldObjects();

	// Object references
	public:
		// entire area of the drawing that is clipped by a bounding box
		// DrawingContent should contain everything except for drawing-level annotations,
		// like title tags or scale tags
		TWeakPtr<FDrawingArea> DrawingContent = nullptr;

		// all of the objects that are in world coordinates.
		// WorldObjects should be a child of DrawingContent, and may be translated within it.  
		// For example, in floorplans WorldObjects is translated to the bottom right corner of the available area
		TWeakPtr<FDraftingComposite> WorldObjects = nullptr;

	protected:
		FModumateUnitCoord2D DrawingSize;
		FModumateUnitCoord2D DrawingMargin;

	// Rooms - this information is likely only relevant to plans (cut plane normal = down vector)
	protected:
		void GetFacesConnectedToRooms(TMap<int32, TArray<int32>> &OutFacesToRooms) const;
		void GetVisibleRoomsAndLocations(TMap<int32, FVector2D> &OutRoomsAndLocations) const;

	// Drawing info
	public:
		// Title info
		FText Name;
		int32 DrawingNumber = 1;

		float DrawingScale;

		// TODO: use this to either calculate the available size (subtracting necessary annotations outside the drawing, like legends)
		bool bScaleToFit = false;

	protected:
		const UModumateDocument *Doc;
		UWorld *World;
	public:
		TPair<int32, int32> CaptureObjID;
	};
}
