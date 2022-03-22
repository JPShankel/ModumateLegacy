// Copyright 2022 Modumate, Inc. All Rights Reserved.


#include "Drafting/ModumateDDDraw.h"
#include "DocumentManagement/ModumateDocument.h"

FModumateDDDraw::FModumateDDDraw(UModumateDocument* Document, UWorld* InWorld, const FDrawingDesignerGenericRequest& Req)
	: FModumateDwgDraw(InWorld),
	DDRequest(Req)
{
	Doc = Document;
}

bool FModumateDDDraw::SaveDocument(const FString& filename)
{
	if (ensure(GetNumPages() == 1) && Doc.IsValid())
	{
		FString jsonStringDiagram = GetJsonAsString(0);
		jsonStringDiagram.TrimEndInline();
		FDrawingDesignerGenericStringResponse rsp;
		rsp.request = DDRequest;
		rsp.answer = jsonStringDiagram;
		FString jsonResponse;
		rsp.WriteJson(jsonResponse);
		Doc->DrawingSendResponse(TEXT("onGenericResponse"), jsonResponse);

		return true;
	}
	return false;
}
