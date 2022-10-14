#include "BIMKernel/Presets/VDPTable.h"

TSet<FGuid> FVDPTable::GetCanonicalGUIDs() const
{
	TSet<FGuid> rtn;
	BtoA.GetKeys(rtn);
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
		return BtoA[guid];
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


bool FVDPTable::HasDerived(const FGuid& guid) const
{
	return AtoB.Contains(guid);
}

bool FVDPTable::HasCanonical(const FGuid& guid) const
{
	return BtoA.Contains(guid);
}

bool FVDPTable::Remove(const FGuid& Value)
{
	if(AtoB.Contains(Value))
	{
		const FGuid Canonical = AtoB[Value];
		AtoB.Remove(Value);
		BtoA.Remove(Canonical);
		return true;
	}

	if (BtoA.Contains(Value))
	{
		const FGuid Derived = BtoA[Value];
		AtoB.Remove(Derived);
		BtoA.Remove(Value);
		return true;
	}

	return false;
}
bool FVDPTable::Add(const FGuid& Canonical, const FGuid& Derived)
{
	if(ensureMsgf(!BtoA.Contains(Canonical), TEXT("Added canonical preset that already existed")) &&
	   ensureMsgf(!AtoB.Contains(Derived), TEXT("Added Derived preset that already existed")))
	{
		AtoB.Add(Derived, Canonical);
		BtoA.Add(Canonical, Derived);
		return true;
	}

	return false;
}

bool FVDPTable::Contains(const FGuid& Value) const
{
	return BtoA.Contains(Value) || AtoB.Contains(Value);
}

bool FVDPTable::GetAssociated(const FGuid& Input, FGuid& Output) const
{
	if(BtoA.Contains(Input))
	{
		Output = BtoA[Input];
		return true;
	}

	if(AtoB.Contains(Input))
	{
		Output = AtoB[Input];
		return true;
	}
	
	return false;
}
