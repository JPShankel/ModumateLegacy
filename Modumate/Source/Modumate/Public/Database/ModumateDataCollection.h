// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Algo/Transform.h"

namespace Modumate {
	template<class T>
	class DataCollection
	{
	public:
		typedef TMap<FName, T> FDataMap;		
		FDataMap DataMap;

		void AddData(const T &data)
		{
			RemoveData(data);
			DataMap.Add(data.UniqueKey(), data);
		}

		void AddDataIfNotPresent(const T &data)
		{
			if (GetData(data.UniqueKey()) == nullptr)
			{
				DataMap.Add(data.UniqueKey(), data);
			}
		}

		void RemoveData(const T &data)
		{
			DataMap.Remove(data.UniqueKey());
		}

		const T *GetData(const FName &str) const
		{
			return DataMap.Find(str);
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
}