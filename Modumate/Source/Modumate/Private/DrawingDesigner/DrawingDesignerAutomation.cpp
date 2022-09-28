#include "DrawingDesigner/DrawingDesignerAutomation.h"

#include "DrawingDesigner/DrawingDesignerRenderControl.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/CutPlane.h"

FDrawingDesignerAutomation::FDrawingDesignerAutomation(UModumateDocument * Document) :
Document(Document)
{
}

TArray<FModumateDimension> FDrawingDesignerAutomation::GenerateDimensions(int32 CutPlaneID) const
{
	AMOICutPlane* cutPlane = Cast<AMOICutPlane>(Document->GetObjectById(CutPlaneID));
	if (!ensure(cutPlane))
	{
		return {};
	}
	TArray<FVector> corners;
	if (ensureAlways(cutPlane->GetNumCorners() == 4))
	{
		for (int32 c = 0; c < 4; ++c)
		{
			corners.Add(cutPlane->GetCorner(c));
		}

	}
	FVector axisX, axisY, center;
	TArray<FVector2D> cached2DPositions;
	FVector origin = corners[0];
	FPlane plane = FPlane(cutPlane->GetCorner(0), cutPlane->GetNormal());
	UModumateGeometryStatics::AnalyzeCachedPositions(corners, plane, axisX, axisY, cached2DPositions, center);
	FBox2D drawingBox = FBox2D(cached2DPositions);
	
	float orthoWidth = drawingBox.GetSize().X;
	float orthoHeight = drawingBox.GetSize().Y;
	FVector2d size {orthoWidth, orthoHeight};
	
	FModumateDimensions dimensionsCreator;
	auto dims = dimensionsCreator.GetDimensions(this->Document, plane, origin, axisX);
	TArray<FModumateDimension> rtn;
	
	for(auto dim : dims)
	{
		//Scale everything by size	
		dim.Points[0] = dim.Points[0] / size;
		dim.Points[1] = dim.Points[1] / size;
		dim.TextPosition = dim.TextPosition / size;
		
		if(!FDrawingDesignerRenderControl::IsFloorplan(*cutPlane))
		{
			//Rotate everything if it's a floorplan
			dim.Points[0] = FVector2d{1,1} - dim.Points[0];
			dim.Points[1] = FVector2d{1,1} - dim.Points[1];
			dim.TextPosition = FVector2d{1, 1} - dim.TextPosition;
		}
		rtn.Push(dim);
	}
	return rtn;
}
