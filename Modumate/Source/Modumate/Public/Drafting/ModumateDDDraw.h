// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Drafting/ModumateDwgDraw.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"

class UModumateDocument;

class FModumateDDDraw : public FModumateDwgDraw
{
public:
	FModumateDDDraw(UModumateDocument* Document, UWorld* InWorld, const FDrawingDesignerGenericRequest& Req);
	FDrawingDesignerGenericRequest DDRequest;
	virtual bool SaveDocument(const FString& filename) override;

private:
	TWeakObjectPtr<UModumateDocument> Doc;
};
