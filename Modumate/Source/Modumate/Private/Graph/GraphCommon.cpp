// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/GraphCommon.h"

#include "ModumateCore/ModumateTypes.h"

FGraphVertexPair::FGraphVertexPair()
{
	Key = MOD_ID_NONE;
	Value = MOD_ID_NONE;
}

FGraphVertexPair::FGraphVertexPair(int32 VertexIDA, int32 VertexIDB)
{
	ensure(VertexIDA != VertexIDB);
	Key = VertexIDA;
	Value = VertexIDB;
}

FGraphVertexPair FGraphVertexPair::MakeEdgeKey(int32 VertexIDA, int32 VertexIDB)
{
	return FGraphVertexPair(FMath::Min(VertexIDA, VertexIDB), FMath::Max(VertexIDA, VertexIDB));
}
