// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "DesignOption.generated.h"


USTRUCT()
struct MODUMATE_API FMOIDesignOptionData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> groups;

	UPROPERTY()
	TArray<int32> subOptions;

	UPROPERTY()
	FString hexColor;

	UPROPERTY()
	bool isShowing = true;
};

UCLASS()
class MODUMATE_API AMOIDesignOption : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIDesignOption();
	virtual ~AMOIDesignOption() {};

	UPROPERTY()
	FMOIDesignOptionData InstanceData;

	// Design options don't have an associated tool, so provide delta functions here
	static TSharedPtr<FMOIDelta> MakeCreateDelta(UModumateDocument* Doc,const FString& DisplayName, int32 ParentID = 0);
	static TSharedPtr<FMOIDelta> MakeAddRemoveGroupDelta(UModumateDocument* Doc, int32 OptionID, int32 GroupID, bool bAdd);
	bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas);
	void PostCreateObject(bool bNewObject);
};
