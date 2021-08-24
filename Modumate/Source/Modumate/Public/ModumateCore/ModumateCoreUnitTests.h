// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateCore/StructDataWrapper.h"
#include "VectorTypes.h"

#include "ModumateCoreUnitTests.generated.h"


#define POINTER_MEMBERS 1

class FPointerTestContainer;

class FPointerTestMember
{
public:
	FPointerTestMember(int32 InID, TSharedPtr<FPointerTestContainer> InContainer);
	~FPointerTestMember();

	int32 ID = -1;
	int32 ContainerID = -1;

	TWeakPtr<FPointerTestContainer> Container;
};

class FPointerTestContainer : public TSharedFromThis<FPointerTestContainer>
{
public:
	FPointerTestContainer() { }

	FPointerTestContainer(int32 InID) : ID(InID) { }

#if POINTER_MEMBERS
	TSharedPtr<FPointerTestMember> AddMember(int32 MemberID)
	{
		auto sharedThis = this->AsShared();
		auto newMember = MakeShared<FPointerTestMember>(MemberID, sharedThis);
		auto addedMember = Members.Add(MemberID, newMember);
		ensure(newMember == addedMember);
		return newMember;
	}
#else
	FPointerTestMember* AddMember(int32 MemberID)
	{
		auto sharedThis = this->AsShared();
		FPointerTestMember& addedMember = Members.Add(MemberID, FPointerTestMember(MemberID, sharedThis));
		return &addedMember;
	}
#endif

	void Reset()
	{
		Members.Reset();
	}

	int32 ID = -1;
#if POINTER_MEMBERS
	TMap<int32, TSharedPtr<FPointerTestMember>> Members;
#else
	TMap<int32, FPointerTestMember> Members;
#endif
};


USTRUCT()
struct MODUMATE_API FModumateTestStruct1
{
	GENERATED_BODY()

	UPROPERTY()
	bool bValue = false;

	UPROPERTY()
	double Number = 0.0;

	UPROPERTY()
	FVec2d Vector;

	UPROPERTY()
	TArray<int32> Integers;

	bool operator==(const FModumateTestStruct1& RHS) const { return (bValue == RHS.bValue) && (Number == RHS.Number) && (Vector == RHS.Vector) && (Integers == RHS.Integers); }
	bool operator!=(const FModumateTestStruct1& RHS) const { return !(*this == RHS); }
};

template<>
struct TStructOpsTypeTraits<FModumateTestStruct1> : public TStructOpsTypeTraitsBase2<FModumateTestStruct1>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};
IMPLEMENT_STRUCT(ModumateTestStruct1);

USTRUCT()
struct MODUMATE_API FModumateTestStruct2
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FVector> VectorMap;
};

USTRUCT()
struct MODUMATE_API FModumateTestStructContainer
{
	GENERATED_BODY()

	UPROPERTY()
	FModumateTestStruct1 ContainedStruct1;

	UPROPERTY()
	FModumateTestStruct2 ContainedStruct2;
};

USTRUCT()
struct MODUMATE_API FModumateTestInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 InstanceID = 0;

	UPROPERTY()
	FString InstanceName;

	UPROPERTY()
	FStructDataWrapper CustomInstanceData;
};

UCLASS()
class MODUMATE_API UModumateTestObjectBase : public UObject
{
	GENERATED_BODY()

public:
	virtual bool GetInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr);
	bool GetInstanceData(UScriptStruct*& OutStructDef, const void*& OutStructPtr) const;

protected:
	virtual FStructProperty* GetInstanceField() const;
};

UCLASS()
class MODUMATE_API UModumateTestObject1 : public UModumateTestObjectBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FModumateTestStruct1 InstanceData;
};

UCLASS()
class MODUMATE_API UModumateTestObject2 : public UModumateTestObjectBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FModumateTestStruct2 InstanceData;
};

UCLASS()
class MODUMATE_API UModumateTestObject3 : public UModumateTestObjectBase
{
	GENERATED_BODY()

public:
	virtual bool GetInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) override;

	FModumateTestStruct1 CustomData;
};
