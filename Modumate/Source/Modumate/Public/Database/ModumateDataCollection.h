// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMKey.h"
#include "Algo/Transform.h"

template<class T>
class TModumateDataCollection
{
public:
	typedef TMap<FBIMKey, T> FDataMap;		
	FDataMap DataMap;

	void AddData(const T& Data)
	{
		RemoveData(Data);
		DataMap.Add(Data.UniqueKey(), Data);
	}

	void AddDataIfNotPresent(const T& Data)
	{
		if (GetData(Data.UniqueKey()) == nullptr)
		{
			DataMap.Add(Data.UniqueKey(), Data);
		}
	}

	void RemoveData(const T& Data)
	{
		DataMap.Remove(Data.UniqueKey());
	}

	const T *GetData(const FBIMKey& Str) const
	{
		return DataMap.Find(Str);
	}

	void Reset()
	{
		DataMap.Reset();
	}

	template<class R>
	TArray<R> GetDataRecords() const
	{
		TArray<R> records;
		Algo::Transform(DataMap, records, [](const FDataMap::ElementType &kvp) {return kvp.Value.ToDataRecord(); });
		return records;
	}
};