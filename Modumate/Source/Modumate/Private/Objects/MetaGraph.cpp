// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaGraph.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"

AMOIMetaGraph::AMOIMetaGraph()
{ }

void AMOIMetaGraph::PostCreateObject(bool bNewObject)
{
	if (Document)
	{
		Super::PostCreateObject(bNewObject);
	}
}
