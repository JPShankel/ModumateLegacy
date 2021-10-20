// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerView.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"

FDrawingDesignerView::FDrawingDesignerView()
{
}

bool FDrawingDesignerView::operator==(const FDrawingDesignerView& RHS) const
{
	if (id != RHS.id || image != RHS.image)
	{
		return false;
	}

	if (snaps.Num() != RHS.snaps.Num())
	{
		return false;
	}

	for (auto& kvp : snaps)
	{
		auto* other = RHS.snaps.Find(kvp.Key);
		if (!other || *other != kvp.Value)
		{
			return false;
		}
	}

	return true;
}

bool FDrawingDesignerView::operator!=(const FDrawingDesignerView& RHS) const
{
	return !(*this == RHS);
}

bool FDrawingDesignerViewSnap::operator==(const FDrawingDesignerViewSnap& RHS) const
{
	return id == RHS.id && FMath::IsNearlyEqual(x, RHS.x) && FMath::IsNearlyEqual(y, RHS.y);
}

bool FDrawingDesignerViewSnap::operator!=(const FDrawingDesignerViewSnap& RHS) const
{
	return !(*this == RHS);
}
