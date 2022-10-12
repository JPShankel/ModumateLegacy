// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "BIMPresetInstance.h"

#include "VDPTable.generated.h"


/**
 * Create a bidirectional map association between a canonical
 * and derived preset.
 *
 * A = VDP GUID
 * B = Canonical GUID
 */
USTRUCT()
struct MODUMATE_API FVDPTable
{
	GENERATED_BODY();
	TSet<FGuid> GetCanonicalGUIDs() const;
	TSet<FGuid> GetDerivedGUIDs() const;
	FGuid TranslateToDerived(const FGuid& guid) const;
	FGuid TranslateToCanonical(const FGuid& guid) const;
	bool HasDerived(const FGuid& guid) const;
	bool HasCanonical(const FGuid& guid) const;
	bool Remove(const FGuid& Value);
	bool Add(const FGuid& Canonical, const FGuid& Derived);
	bool Contains(const FGuid& Value) const;
	bool GetAssociated(const FGuid& Input, FGuid& Output) const;

protected:
	UPROPERTY()
	TMap<FGuid,FGuid> AtoB;
	UPROPERTY()
	TMap<FGuid,FGuid> BtoA;
};
