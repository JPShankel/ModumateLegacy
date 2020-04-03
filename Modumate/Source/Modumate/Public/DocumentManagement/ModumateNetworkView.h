// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once
#include "APDFLLib.h"
#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Debug/ReporterGraph.h"

namespace Modumate
{
	class FModumateObjectInstance;

	namespace FModumateNetworkView
	{
#define POINTS_CLOSE_DIST_SQ 0.25f
		struct FSegment
		{
			FVector Points[2] = { FVector::ZeroVector, FVector::ZeroVector };
			bool Valid = false;
			int32 ObjID = 0;
		};

		struct FNetworkNode
		{
			TArray<FSegment *> Segments;
			TArray<FNetworkNode *> Neighbors;
			FVector Point = FVector::ZeroVector;
		};

		bool TryMakePolygon(const TArray< const Modumate::FModumateObjectInstance *> &obs, TArray<FVector> &polyPoints, const FModumateObjectInstance *startObj = nullptr, TArray<int32> *connectedSegmentIDs = nullptr);
		void MakeSegmentGroups(const TArray<const Modumate::FModumateObjectInstance *> &obs, TArray<TArray<const Modumate::FModumateObjectInstance *>> &groups);
	};
}
