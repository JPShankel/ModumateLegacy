#include "BIMKernel/Presets/VDPTable.h"

TSet<FGuid> FVDPTable::GetCanonicalGUIDs() const
{
	TSet<FGuid> rtn;
	CanonicalToDerives.GetKeys(rtn);
	return rtn;
}

TSet<FGuid> FVDPTable::GetDerivedGUIDs() const
{
	TSet<FGuid> rtn;
	AtoB.GetKeys(rtn);
	return rtn;
}

FGuid FVDPTable::TranslateToDerived(const FGuid& guid) const
{
	if(HasCanonical(guid))
	{
		return CanonicalToDerives[guid].DerivedGuids[0];
	}
	return guid;
}

FGuid FVDPTable::TranslateToCanonical(const FGuid& guid) const
{
	if(HasDerived(guid))
	{
		return AtoB[guid];
	}
	return guid;
}

bool FVDPTable::HasDerived(const DocumentPresetGuid& guid) const
{
	return AtoB.Contains(guid);
}

bool FVDPTable::HasCanonical(const CanonicalPresetGuid& guid) const
{
	return CanonicalToDerives.Contains(guid);
}

bool FVDPTable::Remove(const FGuid& Value)
{
	if(AtoB.Contains(Value))
	{
		//Value is a Derived guid
		DocumentPresetGuid docGuid = Value;
		const FGuid Canonical = AtoB[docGuid];

		//Remove it from the entries list and remove the back
		// association.
		auto& entries = CanonicalToDerives[Canonical].DerivedGuids;
		entries.Remove(docGuid);
		AtoB.Remove(docGuid);
		
		return true;
	}

	if (CanonicalToDerives.Contains(Value))
	{
		//Value is Canonical, This clears the entries list
		CanonicalPresetGuid Canonical = Value;
		auto& entries = CanonicalToDerives[Canonical].DerivedGuids;
		for(DocumentPresetGuid& entry : entries)
		{
			AtoB.Remove(entry);	
		}
		CanonicalToDerives.Remove(Canonical);
		return true;
	}

	return false;
}
bool FVDPTable::Add(const FGuid& Canonical, const FGuid& Derived)
{

	if(!AtoB.Contains(Derived))
	{
		AtoB.Add(Derived, Canonical);
	}
	
	if(!CanonicalToDerives.Contains(Canonical))
	{
		CanonicalToDerives.Add(Canonical, {});
	}
	auto& entry = CanonicalToDerives[Canonical];
	entry.DerivedGuids.Add(Derived);
	
	return true;
}

bool FVDPTable::Contains(const FGuid& Value) const
{
	return CanonicalToDerives.Contains(Value) || AtoB.Contains(Value);
}

void FVDPTable::UpgradeDocRecord(int32 DocRecordVersion)
{
	//28: BtoA --> CanonicalToDerives. This allows 1:* mapping.
	if(DocRecordVersion < 28)
	{
		for(auto& kvp : BtoA_DEPRECATED)
		{
			CanonicalToDerives.Add(kvp.Key, {{kvp.Value}});
		}
		BtoA_DEPRECATED.Empty(); //Empty so it's not replicated anymore
	}
}
