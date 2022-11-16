// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "BIMPresetInstance.h"

#include "VDPTable.generated.h"

typedef FGuid CanonicalPresetGuid;
typedef FGuid DocumentPresetGuid;

USTRUCT()
struct MODUMATE_API FVDPEntry
{
	GENERATED_BODY();

	UPROPERTY();
	TArray<FGuid> DerivedGuids;
};

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
	TSet<CanonicalPresetGuid> GetCanonicalGUIDs() const;
	TSet<DocumentPresetGuid> GetDerivedGUIDs() const;
	DocumentPresetGuid TranslateToDerived(const FGuid& guid) const;
	CanonicalPresetGuid TranslateToCanonical(const FGuid& guid) const;
	bool HasDerived(const FGuid& guid) const;
	bool HasCanonical(const FGuid& guid) const;
	bool Remove(const FGuid& Value);
	bool Add(const CanonicalPresetGuid& Canonical, const DocumentPresetGuid& Derived);
	bool Contains(const FGuid& Value) const;
	void UpgradeDocRecord(int32 DocRecordVersion);

protected:
	UPROPERTY()
	TMap<FGuid, FGuid> AtoB;
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Migrated to CanonicalToDerives"))
	TMap<FGuid, FGuid> BtoA_DEPRECATED;
	UPROPERTY()
	TMap<FGuid, FVDPEntry> CanonicalToDerives;
};
