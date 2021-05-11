// Copyright 2020 Modumate, Inc. All Rights Reserved.
#include "ModumateCore/ModumateCoreUnitTests.h"

#include "CoreMinimal.h"

#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "CompGeom/PolygonTriangulation.h"
#include "JsonObjectConverter.h"
#include "MathUtil.h"
#include "ModumateCore/EdgeDetailData.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Polygon2.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"

#define LOCTEXT_NAMESPACE "CoreUnitTests"

bool UModumateTestObjectBase::GetInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = nullptr;
	OutStructPtr = nullptr;

	if (auto structProp = GetInstanceField())
	{
		OutStructDef = structProp->Struct;
		OutStructPtr = structProp->ContainerPtrToValuePtr<void>(this);
	}

	return (OutStructDef != nullptr) && (OutStructPtr != nullptr);
}

bool UModumateTestObjectBase::GetInstanceData(UScriptStruct*& OutStructDef, const void*& OutStructPtr) const
{
	void* mutableStructPtr;
	bool bSuccess = const_cast<UModumateTestObjectBase*>(this)->GetInstanceData(OutStructDef, mutableStructPtr);
	OutStructPtr = const_cast<const void*>(mutableStructPtr);
	return bSuccess;
}

FStructProperty* UModumateTestObjectBase::GetInstanceField() const
{
	static const FName instanceDataPropName(TEXT("InstanceData"));
	return CastField<FStructProperty>(GetClass()->FindPropertyByName(instanceDataPropName));
}

bool UModumateTestObject3::GetInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = CustomData.StaticStruct();
	OutStructPtr = &CustomData;
	return true;
}

namespace Modumate
{
	// Pointer tests


	FPointerTestMember::FPointerTestMember(int32 InID, TSharedPtr<FPointerTestContainer> InContainer)
		: ID(InID)
		, Container(InContainer)
	{
		if (Container.IsValid())
		{
			ContainerID = Container.Pin()->ID;
		}
	}

	FPointerTestMember::~FPointerTestMember()
	{
		int32 containerID = -1;

		auto container = Container.Pin();
		if (container.IsValid())
		{
			ensure(ContainerID == container->ID);
		}
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumatePointerTests, "Modumate.Core.Pointers", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::LowPriority)
		bool FModumatePointerTests::RunTest(const FString& Parameters)
	{
		static bool bLogResults = UE_BUILD_DEBUG;
		for (int32 i = 0; i < 16; ++i)
		{
			auto container = MakeShared<FPointerTestContainer>(i);

			for (int32 j = 0; j < 16; ++j)
			{
				auto member = container->AddMember(j);
				if (ensure(member) && bLogResults)
				{
					UE_LOG(LogTemp, Log, TEXT("Added member ID %d to container ID %d"), member->ID, container->ID);
				}
			}

			if ((container->ID % 2) == 0)
			{
				container->Reset();
			}
		}

		return true;
	}

	// Serialization tests

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateUStructSerializationTest, "Modumate.Core.Serialization.UStructSerialization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::LowPriority)
		bool FModumateUStructSerializationTest::RunTest(const FString& Parameters)
	{
		// Struct 1 property comparison

		FModumateTestStruct1 testStruct1A{ false, 1.0 / 2.54, FVec2d(2.54, 1.6), {1, 2, 3, 4} };
		FModumateTestStruct1 testStruct1B{ true, 1.0 / 2.54, FVec2d(2.54, 1.6), {1, 0, 3, 4} };
		FModumateTestStruct1 testStruct1C{ true, 1.0 / 2.54, FVec2d(2.54, 6.1), {1, 2, 3, 4} };

		for (TFieldIterator<FProperty> propIter(FModumateTestStruct1::StaticStruct()); propIter; ++propIter)
		{
			FProperty* propDef = *propIter;

			FString propName;
			propDef->GetName(propName);

			const void* structPropValueA = propDef->ContainerPtrToValuePtr<void>(&testStruct1A);
			const void* structPropValueB = propDef->ContainerPtrToValuePtr<void>(&testStruct1B);
			const void* structPropValueC = propDef->ContainerPtrToValuePtr<void>(&testStruct1C);
			bool bPropsEqualAB = propDef->Identical(structPropValueA, structPropValueB);
			bool bPropsEqualBC = propDef->Identical(structPropValueB, structPropValueC);
			bool bPropsEqualAC = propDef->Identical(structPropValueA, structPropValueC);

			if (propName.Equals(TEXT("bValue")))
			{
				TestFalse(TEXT("A.bValue != B.bValue"), bPropsEqualAB);
				TestTrue(TEXT("B.bValue == C.bValue"), bPropsEqualBC);
				TestFalse(TEXT("A.bValue != C.bValue"), bPropsEqualAC);
			}
			else if (propName.Equals(TEXT("Integers")))
			{
				TestFalse(TEXT("A.Integers != B.Integers"), bPropsEqualAB);
				TestFalse(TEXT("B.Integers != C.Integers"), bPropsEqualBC);
				TestTrue(TEXT("A.Integers == C.Integers"), bPropsEqualAC);
			}
			else if (propName.Equals(TEXT("Vector")))
			{
				TestTrue(TEXT("A.Vector == B.Vector"), bPropsEqualAB);
				TestFalse(TEXT("B.Vector != C.Vector"), bPropsEqualBC);
				TestFalse(TEXT("A.Vector != C.Vector"), bPropsEqualAC);
			}
		}

		// Struct 2 property comparison

		FModumateTestStruct2 testStruct2A;
		testStruct2A.VectorMap.Add(FName(TEXT("Handle")), FVector(6.0f, 36.0f, 0.0f));
		testStruct2A.VectorMap.Add(FName(TEXT("Frame")), FVector(0.0f, 0.0f, 0.0f));
		testStruct2A.VectorMap.Add(FName(TEXT("Panel")), FVector(0.0f, 0.0f, 1.0f));

		FModumateTestStruct2 testStruct2B;
		testStruct2B.VectorMap.Add(FName(TEXT("Frame")), FVector(0.0f, 0.0f, 0.0f));
		testStruct2B.VectorMap.Add(FName(TEXT("Panel")), FVector(0.0f, 0.0f, 1.0f));
		testStruct2B.VectorMap.Add(FName(TEXT("Handle")), FVector(6.0f, 36.0f, 0.0f));

		for (TFieldIterator<FProperty> propIter(FModumateTestStruct2::StaticStruct()); propIter; ++propIter)
		{
			FProperty* propDef = *propIter;

			const void* structPropValueA = propDef->ContainerPtrToValuePtr<void>(&testStruct2A);
			const void* structPropValueB = propDef->ContainerPtrToValuePtr<void>(&testStruct2B);
			bool bPropsEqualAB = propDef->Identical(structPropValueA, structPropValueB);
			TestTrue(TEXT("A.VectorMap == B.VectorMap"), bPropsEqualAB);
		}

		// JSON serialization

		FString testStructWithObjString;
		TestTrue(TEXT("JSON write success"), FJsonObjectConverter::UStructToJsonObjectString(testStruct1A, testStructWithObjString));

		FModumateTestStruct1 testStruct1AJSONDeserialized;
		TestTrue(TEXT("JSON read success"), FJsonObjectConverter::JsonObjectStringToUStruct(testStructWithObjString, &testStruct1AJSONDeserialized, 0, 0));

		TestEqual(TEXT("JSON read == original"), testStruct1AJSONDeserialized, testStruct1A);

		// Cbor serialization

		TArray<uint8> buffer;
		FMemoryWriter writer(buffer);
		FCborStructSerializerBackend serializerBackend(writer, EStructSerializerBackendFlags::Default);

		FStructSerializerPolicies policies;
		policies.NullValues = EStructSerializerNullValuePolicies::Ignore;

		FStructSerializer::Serialize(testStruct1A, serializerBackend, policies);
		TestTrue(TEXT("CBOR write success"), buffer.Num() > 0);

		FMemoryReader reader(buffer);
		FCborStructDeserializerBackend deserializerBackend(reader);

		FModumateTestStruct1 testStruct1ACBorDeserialized;
		FModumateTestStruct2 testStruct2ACBorDeserialized;
		TestTrue(TEXT("CBOR struct1 read success"), FStructDeserializer::Deserialize(testStruct1ACBorDeserialized, deserializerBackend));
		TestEqual(TEXT("CBOR read == original"), testStruct1ACBorDeserialized, testStruct1A);

		return true;
	}

	// Test for (and interactive debuggable example of) custom USTRUCT comparison utilities.
	// Expectation: USTRUCTs that can be compared by their UPROPERTYs should be able to be compared by the global utility UScriptStruct::CompareScriptStruct,
	// which will in turn compare properties based on their own CPPStructOps.
	// Struct properties that either shouldn't use operator== for logical comparison, or do not implement it,
	// will recurse through their properties, unless they have the WithIdenticalViaEquality trait.
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateUStructComparisonTest, "Modumate.Core.Serialization.UStructComparison", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::LowPriority)
		bool FModumateUStructComparisonTest::RunTest(const FString& Parameters)
	{
		FModumateTestStruct1 testStruct1A{ false, 1.0 / 2.54, FVec2d(1.1, 2.2), {1, 2, 3, 4} };
		FModumateTestStruct1 testStruct1B{ true, 1.0 / 2.54, FVec2d(1.3, 2.2), {1, 0, 3, 4} };

		FModumateTestStruct2 testStruct2A;
		testStruct2A.VectorMap.Add(FName(TEXT("Handle")), FVector(6.0f, 36.0f, 0.0f));
		testStruct2A.VectorMap.Add(FName(TEXT("Frame")), FVector(0.0f, 0.0f, 0.0f));
		testStruct2A.VectorMap.Add(FName(TEXT("Panel")), FVector(0.0f, 0.0f, 1.0f));

		FModumateTestStruct2 testStruct2B;
		testStruct2B.VectorMap.Add(FName(TEXT("Frame")), FVector(1.0f, 0.0f, 0.0f));
		testStruct2B.VectorMap.Add(FName(TEXT("Handle")), FVector(6.0f, 36.0f, 0.0f));

		FModumateTestStructContainer testStructContainerA{ testStruct1A, testStruct2A };
		FModumateTestStructContainer testStructContainerACopy = testStructContainerA;
		FModumateTestStructContainer testStructContainerB{ testStruct1B, testStruct2B };

		bool bAEqualsCopy = FModumateTestStructContainer::StaticStruct()->CompareScriptStruct(&testStructContainerA, &testStructContainerACopy, 0x0);
		TestTrue(TEXT("testStructContainerA == testStructContainerACopy"), bAEqualsCopy);

		bool bAEqualsB = FModumateTestStructContainer::StaticStruct()->CompareScriptStruct(&testStructContainerA, &testStructContainerB, 0x0);
		TestTrue(TEXT("testStructContainerA != testStructContainerB"), !bAEqualsB);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateUObjectSerializationTest, "Modumate.Core.Serialization.UObjectSerialization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::LowPriority)
		bool FModumateUObjectSerializationTest::RunTest(const FString& Parameters)
	{
		// UObject duplication

		auto testObject1Original = NewObject<UModumateTestObject1>();
		testObject1Original->InstanceData.bValue = true;
		testObject1Original->InstanceData.Number = 1.0 / 2.54;
		testObject1Original->InstanceData.Vector = FVec2d::One();
		testObject1Original->InstanceData.Integers = { 1, 2, 3, 4 };
		UModumateTestObjectBase* testObject1OriginalBase = testObject1Original;

		auto testObject1CloneBase = StaticDuplicateObject(testObject1OriginalBase, (UObject*)GetTransientPackage());
		auto testObject1Clone = Cast<UModumateTestObject1>(testObject1CloneBase);

		UTEST_TRUE(TEXT("Cloned correct UClass"), testObject1Clone != nullptr);
		UTEST_EQUAL(TEXT("Cloned correct properties"), testObject1Original->InstanceData, testObject1Clone->InstanceData);

		for (TFieldIterator<FProperty> propIter(UModumateTestObject1::StaticClass()); propIter; ++propIter)
		{
			FProperty* propDef = *propIter;

			FString propName;
			propDef->GetName(propName);

			const void* objPropValueA = propDef->ContainerPtrToValuePtr<void>(testObject1Original);
			const void* objPropValueB = propDef->ContainerPtrToValuePtr<void>(testObject1Clone);
			bool bPropsEqualAB = propDef->Identical(objPropValueA, objPropValueB);
			TestTrue(FString::Printf(TEXT("Original.%s == Clone.%s"), *propName, *propName), bPropsEqualAB);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateEmbeddedStructSerializationTest, "Modumate.Core.Serialization.EmbeddedStructSerialization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateEmbeddedStructSerializationTest::RunTest(const FString& Parameters)
	{
		FString objData1JsonString;

		// Make a "MOI" and edit some of its instance data
		auto testObject1Original = NewObject<UModumateTestObject1>();
		testObject1Original->InstanceData = FModumateTestStruct1{ false, 1.0 / 2.54, FVec2d::One(), {1, 2, 3, 4} };

		// Save
		{
			// Get ready to save it, needing only a pointer to the base class
			FModumateTestInstanceData objData1Orig;
			objData1Orig.InstanceID = 7;
			objData1Orig.InstanceName = FString(TEXT("Door1"));

			UScriptStruct* structDef;
			const void* structPtr;
			UTEST_TRUE(TEXT("Get original instance data struct"), testObject1Original->GetInstanceData(structDef, structPtr));
			objData1Orig.CustomInstanceData.SaveStructData(structDef, structPtr, true);

			// Save it out to a "document"
			TestTrue(TEXT("JSON write success"), FJsonObjectConverter::UStructToJsonObjectString(objData1Orig, objData1JsonString));
		}

		// Load
		{
			// Load just the struct data from the "document"
			FModumateTestInstanceData objData1Loaded;
			TestTrue(TEXT("JSON read success"), FJsonObjectConverter::JsonObjectStringToUStruct(objData1JsonString, &objData1Loaded, 0, 0));
			objData1Loaded.CustomInstanceData.SaveCborFromJson();
			TestTrue(TEXT("Struct data load success"), objData1Loaded.CustomInstanceData.IsValid());

			// Create the loaded "MOI" from the instance data
			auto testObject1Loaded = NewObject<UModumateTestObject1>();
			UScriptStruct* outStructDef1;
			void* outStructPtr1;
			TestTrue(TEXT("Get loaded instance data struct"), testObject1Loaded->GetInstanceData(outStructDef1, outStructPtr1));
			TestTrue(TEXT("Instance data load success"), objData1Loaded.CustomInstanceData.LoadStructData(outStructDef1, outStructPtr1));

			TestEqual(TEXT("Instance data loaded correctly"), testObject1Original->InstanceData, testObject1Loaded->InstanceData);

			// Try to load the "MOI" into the incorrect type
			auto testObject2Loaded = NewObject<UModumateTestObject2>();
			UScriptStruct* outStructDef2;
			void* outStructPtr2;
			TestTrue(TEXT("Get loaded instance data struct"), testObject2Loaded->GetInstanceData(outStructDef2, outStructPtr2));
			TestFalse(TEXT("Wrong instance data load failure"), objData1Loaded.CustomInstanceData.LoadStructData(outStructDef2, outStructPtr2));

			// Copy and modify the typed instance data of the loaded "MOI"
			auto modifiedInstanceData1 = testObject1Loaded->InstanceData;
			modifiedInstanceData1.Number = PI;

			// Save the modified instance data, creating a snapshot of the edit
			FModumateTestInstanceData objData1Modified = objData1Loaded;
			TestTrue(TEXT("Save out modified instance data struct"), objData1Modified.CustomInstanceData.SaveStructData(modifiedInstanceData1));

			// Load the modified snapshot back into the original "MOI"
			TestTrue(TEXT("Load in modified instance data struct"), objData1Modified.CustomInstanceData.LoadStructData(testObject1Loaded->InstanceData));
			TestEqual(TEXT("Loaded in correct instance data"), testObject1Loaded->InstanceData.Number, (double)PI);

			// Test CBOR serialization of contained custom struct data
			// This is not very helpful because CBOR auto-USTRUCT-serialization will end up only serialization the wrapper's JSON string, rather than the CBOR buffer.
			// TODO: customize serialization behavior so that CBOR can pass through the buffer directly, like how UStructToJsonObject will pass through the wrapper JSON object directly.
			TArray<uint8> buffer;
			FMemoryWriter writer(buffer);
			FCborStructSerializerBackend serializerBackend(writer, EStructSerializerBackendFlags::Default);

			FStructSerializerPolicies policies;
			policies.NullValues = EStructSerializerNullValuePolicies::Ignore;

			objData1Modified.CustomInstanceData.SaveJsonFromCbor();
			FStructSerializer::Serialize(objData1Modified, serializerBackend, policies);
			TestTrue(TEXT("CBOR instance data write success"), buffer.Num() > 0);

			// Test CBOR deserialization of contained custom struct data
			FMemoryReader reader(buffer);
			FCborStructDeserializerBackend deserializerBackend(reader);

			FModumateTestInstanceData objData1CborDeserialized;
			TestTrue(TEXT("CBOR instance data read success"), FStructDeserializer::Deserialize(objData1CborDeserialized, deserializerBackend));
			TestTrue(TEXT("CBOR data post-load success"), objData1CborDeserialized.CustomInstanceData.SaveFromJsonString());
			TestEqual(TEXT("CBOR instance data read == original"), objData1CborDeserialized.CustomInstanceData, objData1Modified.CustomInstanceData);
		}

		return true;
	}

	// Modumate Geometry Statics

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryRayPrecision, "Modumate.Core.Geometry.RayPrecision", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryRayPrecision::RunTest(const FString& Parameters)
	{
		static constexpr int32 numTests = 64;
		static constexpr int32 numRepetitions = 16;
		static constexpr double testSize = 1000.0;
		static constexpr float goalFloatError = (1.e-2f);
		static constexpr double goalDoubleError = (1.e-10);
		FRandomStream rand;
		int32 maxErrorTestIdx = INDEX_NONE;
		float maxFloatError = 0.0f;
		double maxDoubleError = 0.0;

		// First, test precision of ray projection of standard float-based FVector types
		for (int32 testIdx = 0; testIdx < numTests; ++testIdx)
		{
			FVector startPoint1 = testSize * rand.VRand();
			FVector startPoint2 = testSize * rand.VRand();
			FVector dir = (startPoint2 - startPoint1).GetSafeNormal();
			FVector testPoint2 = startPoint2;
			float error = 0.0f;

			for (int32 iteration = 0; iteration < numRepetitions; ++iteration)
			{
				FVector projectedDelta = (testPoint2 - startPoint1).ProjectOnToNormal(dir);
				testPoint2 = startPoint1 + projectedDelta;
				error = FVector::Dist(testPoint2, startPoint2);
			}

			if ((error > maxFloatError) || (maxErrorTestIdx == INDEX_NONE))
			{
				maxFloatError = error;
				maxErrorTestIdx = testIdx;
			}
		}

		TestTrue(FString::Printf(TEXT("Maximum FVector projection error under %f"), goalFloatError), maxFloatError < goalFloatError);

		rand.Reset();
		maxErrorTestIdx = INDEX_NONE;
		for (int32 testIdx = 0; testIdx < numTests; ++testIdx)
		{
			FVector startPoint1f = testSize * rand.VRand();
			FVector startPoint2f = testSize * rand.VRand();

			FVector3d startPoint1(startPoint1f.X, startPoint1f.Y, startPoint1f.Z);
			FVector3d startPoint2(startPoint2f.X, startPoint2f.Y, startPoint2f.Z);

			FVector3d dir = (startPoint2 - startPoint1).Normalized(SMALL_NUMBER);
			FVector3d testPoint2 = startPoint2;
			double error = 0.0;

			for (int32 iteration = 0; iteration < numRepetitions; ++iteration)
			{
				FVector3d projectedDelta = dir * (testPoint2 - startPoint1).Dot(dir);
				testPoint2 = startPoint1 + projectedDelta;
				error = testPoint2.Distance(startPoint2);
			}

			maxDoubleError = FMath::Max(error, maxDoubleError);
		}

		TestTrue(FString::Printf(TEXT("Maximum FVector3d projection error under %f"), goalDoubleError), maxDoubleError < goalDoubleError);

		return true;
	}

	// Ray Intersection Tests
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryRayIntersection, "Modumate.Core.Geometry.RayIntersection", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateGeometryRayIntersection::RunTest(const FString& Parameters)
	{
		FVector2D intersectionPoint;
		float rayDistA, rayDistB;
		bool bIntersection;
		bool bSuccess = true;

		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(1.0f, 1.0f), FVector2D(2.0f, -1.0f).GetSafeNormal(),
			FVector2D(-1.0f, -10.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, true);

		bSuccess = bSuccess && bIntersection && (rayDistA > 0.0f) && (rayDistB > 0.0f) &&
			intersectionPoint.Equals(FVector2D(23.0f, -10.f));

		// coincident, non-colinear rays
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(4.0f, 1.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, false);
		bSuccess = bSuccess && !bIntersection;

		// coincident, colinear rays
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(4.0f, 0.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, false);
		bSuccess = bSuccess && bIntersection &&
			FMath::IsNearlyEqual(rayDistA, 4.0f) &&
			FMath::IsNearlyEqual(rayDistB, 0.0f) &&
			intersectionPoint.Equals(FVector2D(4.0f, 0.0f));

		// anti-parallel, colinear rays pointing at each other
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(4.0f, 0.0f), FVector2D(-1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, false);
		bSuccess = bSuccess && bIntersection &&
			FMath::IsNearlyEqual(rayDistA, 2.0f) &&
			FMath::IsNearlyEqual(rayDistB, 2.0f) &&
			intersectionPoint.Equals(FVector2D(2.0f, 0.0f));

		// anti-parallel, colinear rays pointing away from each other
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(-1.0f, 0.0f),
			FVector2D(4.0f, 0.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, false);
		bSuccess = bSuccess && bIntersection &&
			FMath::IsNearlyEqual(rayDistA, -2.0f) &&
			FMath::IsNearlyEqual(rayDistB, -2.0f) &&
			intersectionPoint.Equals(FVector2D(2.0f, 0.0f));

		// 3D anti-parallel, colinear rays pointing at each other
		FVector intersectionPoint3D(ForceInitToZero);
		bIntersection = UModumateGeometryStatics::RayIntersection3D(
			FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f).GetSafeNormal(),
			FVector(10.0f, 10.0f, 10.0f), FVector(-1.0f, -1.0f, -1.0f).GetSafeNormal(),
			intersectionPoint3D, rayDistA, rayDistB);
		TestTrue(TEXT("3D anti-parallel rays intersect"), bIntersection);
		TestTrue(TEXT("3D anti-parallel ray intersection correct"), intersectionPoint3D.Equals(FVector(5.0f, 5.0f, 5.0f), RAY_INTERSECT_TOLERANCE));

		return bSuccess;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometrySegmentIntersection, "Modumate.Core.Geometry.SegmentIntersection", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometrySegmentIntersection::RunTest(const FString& Parameters)
	{
		FVector2D intersectionPoint;
		bool bIntersection, bOverlapping;

		// Simple intersection
		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(10.0f, 10.0f),
			FVector2D(10.0f, 0.0f), FVector2D(0.0f, 10.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("X segments intersect"), bIntersection && !bOverlapping && intersectionPoint.Equals(FVector2D(5.0f, 5.0f)));

		// Point intersections
		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f),
			FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Point A intersects Point B"), bIntersection && !bOverlapping && intersectionPoint.Equals(FVector2D(1.0f, 1.0f)));

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f),
			FVector2D(-10.0f, 1.0f), FVector2D(10.0f, 1.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Point A intersects Segment B"), bIntersection && !bOverlapping && intersectionPoint.Equals(FVector2D(0.0f, 1.0f)));

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(-10.0f, 2.0f), FVector2D(10.0f, 2.0f),
			FVector2D(0.0f, 2.0f), FVector2D(0.0f, 2.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Segment A intersects Point B"), bIntersection && !bOverlapping && intersectionPoint.Equals(FVector2D(0.0f, 2.0f)));

		// Parallel non-intersections
		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(10.0f, 0.0f),
			FVector2D(4.0f, 1.0f), FVector2D(14.0f, 1.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Parallel, non-intersecting segments pointing the same direction"), !bIntersection && !bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(4.0f, 0.0f),
			FVector2D(10.0f, 0.0f), FVector2D(14.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Colinear, non-intersecting segments both pointing right"), !bIntersection && !bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(4.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(14.0f, 0.0f), FVector2D(10.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Colinear, non-intersecting segments both pointing left"), !bIntersection && !bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(4.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(10.0f, 0.0f), FVector2D(14.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Colinear, non-intersecting segments pointing away from each other"), !bIntersection && !bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(4.0f, 0.0f),
			FVector2D(14.0f, 0.0f), FVector2D(10.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Colinear, non-intersecting segments pointing towards each other"), !bIntersection && !bOverlapping);

		// Parallel intersections
		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(10.0f, 0.0f),
			FVector2D(5.0f, 0.0f), FVector2D(15.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Overlapping segments both pointing right"), bIntersection && bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(10.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(15.0f, 0.0f), FVector2D(5.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Overlapping segments both pointing left"), bIntersection && bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(10.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(5.0f, 0.0f), FVector2D(15.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Overlapping segments pointing away from each other"), bIntersection && bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(10.0f, 0.0f),
			FVector2D(15.0f, 0.0f), FVector2D(5.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Overlapping segments pointing towards each other"), bIntersection && bOverlapping);

		// Contained intersections
		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(10.0f, 0.0f),
			FVector2D(7.0f, 0.0f), FVector2D(3.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Segment A -> contains Segment B <-, "), bIntersection && bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(10.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(7.0f, 0.0f), FVector2D(3.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Segment A <- contains Segment B <-, "), bIntersection && bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(10.0f, 0.0f),
			FVector2D(3.0f, 0.0f), FVector2D(7.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Segment A -> contains Segment B ->, "), bIntersection && bOverlapping);

		bIntersection = UModumateGeometryStatics::SegmentIntersection2D(
			FVector2D(10.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(3.0f, 0.0f), FVector2D(7.0f, 0.0f),
			intersectionPoint, bOverlapping);
		TestTrue(TEXT("Segment A <- contains Segment B ->, "), bIntersection && bOverlapping);

		return true;
	}

	// Point in poly tests
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPointInPoly, "Modumate.Core.Geometry.PointInPoly", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateGeometryPointInPoly::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> triangle({
			FVector2D(0.0f, 0.0f),
			FVector2D(10.0f, 0.0f),
			FVector2D(5.0f, 8.0f),
		});

		FPointInPolyResult pointInPolyResult;
		bool bSuccess;

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 4.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("bInsideTriangle1"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-5.0f, 4.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle2"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(15.0f, 4.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle3"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 14.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle4"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(15.0f, 0.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle5"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		TArray<FVector2D> concaveUShape({
			FVector2D(0.0f, 0.0f),     // 0
			FVector2D(10.0f, 0.0f),    // 1
			FVector2D(10.0f, 1.0f),    // 2        9--8    4--3
			FVector2D(10.0f, 10.0f),   // 3        |  |    |  |
			FVector2D(8.0f, 10.0f),    // 4        |  |    |  |
			FVector2D(8.0f, 2.0f),     // 5        |  7-6-5   |
			FVector2D(5.0f, 2.0f),     // 6       10          2
			FVector2D(2.0f, 2.0f),     // 7        |          |
			FVector2D(2.0f, 10.0f),    // 8        0----------1
			FVector2D(0.0f, 10.0f),    // 9
			FVector2D(0.0f, 1.0f),     // 10
		});

		for (int32 i = 0; i < 2; ++i)
		{
			// For the second iteration, reverse the shape to make sure none of the results are winding-dependent
			if (i == 1)
			{
				Algo::Reverse(concaveUShape);
			}

			// Sanity concave intersection tests
			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("bInsideU1"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 1.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("bInsideU2"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 1.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("bInsideU3"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.0f, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("bInsideU4"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU5"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-5.0f, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU6"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(15.0f, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU7"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			// Inclusive overlap epsilon tests
			float halfEpsilon = 0.5f * RAY_INTERSECT_TOLERANCE;
			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU8"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU9"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(10 - halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU10"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(10 + halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU11"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(halfEpsilon, 10.0f - halfEpsilon), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU12"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-halfEpsilon, 10.0f + halfEpsilon), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("!bInsideU13"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

			// Test for ray intersections that hit close to points on the containing polygon
			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 2.0f + halfEpsilon), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("Inside lower left, half-epsilon above corner"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 2.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("Inside lower left, even with corner"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 2.0f - halfEpsilon), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("Inside lower left, half-epsilon below corner"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.0f, 1.0f + halfEpsilon), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("Inside lower right, half-epsilon above corner"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.0f, 1.0f), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("Inside lower right, even with corner"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

			bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.0f, 1.0f - halfEpsilon), concaveUShape, pointInPolyResult);
			TestTrue(TEXT("Inside lower right, half-epsilon below corner"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);
		}

		return true;
	}

	// Poly intersection tests
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryConvexPolyIntersection, "Modumate.Core.Geometry.ConvexPolyIntersection", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateGeometryConvexPolyIntersection::RunTest(const FString& Parameters)
	{
		// Poly intersection test group 1 - convex, non-overlapping

		TArray<FVector2D> smallPoly({
			FVector2D(1.0f, 1.0f),
			FVector2D(8.0f, 1.0f),
			FVector2D(8.0f, 7.0f),
			FVector2D(2.0f, 8.0f),
		});

		TArray<FVector2D> bigPoly({
			FVector2D(-1.0f, -1.0f),
			FVector2D(0.0f, 10.0f),
			FVector2D(12.0f, 11.0f),
			FVector2D(11.0f, 0.0f),
		});

		bool bSuccess, bOverlapping, bFullyContained, bPartiallyContained;

		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(bigPoly, smallPoly, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Small poly in big poly"), bSuccess && !bOverlapping && bFullyContained && !bPartiallyContained);

		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(smallPoly, bigPoly, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Big poly not in small poly"), bSuccess && !bOverlapping && !bFullyContained && !bPartiallyContained);


		// Poly intersection test group 2 - convex, "partially contained" (single shared vertex)

		smallPoly = {
			FVector2D(8.0f, 5.0f),
			FVector2D(-1.0f, -1.0f),
			FVector2D(5.0f, 8.0f),
		};

		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(bigPoly, smallPoly, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Touching poly partially contained by big poly"), bSuccess && !bOverlapping && !bFullyContained && bPartiallyContained);

		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(smallPoly, bigPoly, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Big poly not in partially contained poly"), bSuccess && !bOverlapping && !bFullyContained && !bPartiallyContained);


		// Poly intersection test group 3 - convex, overlapping edge

		TArray<FVector2D> planePoly({
			FVector2D(0.0f, 0.0f),
			FVector2D(20.0f, 0.0f),
			FVector2D(20.0f, 10.0f),
			FVector2D(0.0f, 10.0f),
		});

		TArray<FVector2D> portalPoly({
			FVector2D(5.0f, 0.0f),
			FVector2D(5.0f, 7.0f),
			FVector2D(8.0f, 7.0f),
			FVector2D(8.0f, 0.0f),
		});

		// Simulate placing a door on a wall without graph vertex-edge intersections
		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(planePoly, portalPoly, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Door poly overlapping wall poly"), bSuccess && bOverlapping && !bFullyContained && !bPartiallyContained);

		// Ensure it still detects touching and not overlapping within the tolerance

		TArray<FVector2D> portalPolyLower;
		Algo::Transform(portalPoly, portalPolyLower, [](const FVector2D &p) { return p - FVector2D(0.0f, 0.5f * RAY_INTERSECT_TOLERANCE); });
		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(planePoly, portalPolyLower, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Slightly lower door poly overlapping wall poly"), bSuccess && bOverlapping && !bFullyContained && !bPartiallyContained);

		TArray<FVector2D> portalPolyHigher;
		Algo::Transform(portalPoly, portalPolyHigher, [](const FVector2D &p) { return p + FVector2D(0.0f, 0.5f * RAY_INTERSECT_TOLERANCE); });
		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(planePoly, portalPolyHigher, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Slightly higher door poly overlapping wall poly"), bSuccess && bOverlapping && !bFullyContained && !bPartiallyContained);

		// Ensure it doesn't detect touching outside of the tolerance

		portalPolyLower.Reset();
		Algo::Transform(portalPoly, portalPolyLower, [](const FVector2D &p) { return p - FVector2D(0.0f, 1.5f * RAY_INTERSECT_TOLERANCE); });
		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(planePoly, portalPolyLower, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Lower door poly overlapping wall poly"), bSuccess && bOverlapping && !bFullyContained && !bPartiallyContained);

		portalPolyHigher.Reset();
		Algo::Transform(portalPoly, portalPolyHigher, [](const FVector2D &p) { return p + FVector2D(0.0f, 1.5f * RAY_INTERSECT_TOLERANCE); });
		bSuccess = UModumateGeometryStatics::GetPolygonIntersection(planePoly, portalPolyHigher, bOverlapping, bFullyContained, bPartiallyContained);
		TestTrue(TEXT("Higher door poly contained by wall poly"), bSuccess && !bOverlapping && bFullyContained && !bPartiallyContained);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryRayBounding, "Modumate.Core.Geometry.RayBounding", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryRayBounding::RunTest(const FString& Parameters)
	{
		bool bBounded, bOverlaps;

		// Convex test
		FVector2D boundingRay1A(1.0f, 0.0f);
		FVector2D boundingNormal1A(0.0f, 1.0f);
		FVector2D boundingRay1B(0.0f, 1.0f);
		FVector2D boundingNormal1B(1.0f, 0.0f);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(-1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test1 (-1, 0) not bounded"), !bBounded && !bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test1 (1, 0) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(0.0f, 1.0f), bOverlaps);
		TestTrue(TEXT("Test1 (0, 1) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(1.0f, 1.0f).GetSafeNormal(), bOverlaps);
		TestTrue(TEXT("Test1 (0.7, 0.7) bounded"), bBounded && !bOverlaps);

		// Concave test
		FVector2D boundingRay2A(1.0f, 0.0f);
		FVector2D boundingNormal2A(0.0f, -1.0f);
		FVector2D boundingRay2B(0.0f, 1.0f);
		FVector2D boundingNormal2B(-1.0f, 0.0f);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(-1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test2 (-1, 0) bounded"), bBounded && !bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test2 (1, 0) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(0.0f, 1.0f), bOverlaps);
		TestTrue(TEXT("Test2 (0, 1) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(1.0f, 1.0f).GetSafeNormal(), bOverlaps);
		TestTrue(TEXT("Test2 (0.7, 0.7) not bounded"), !bBounded && !bOverlaps);

		return true;
	}

	// Layer definition and triangulation via structured data
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryLayerTriangulation, "Modumate.Core.Geometry.LayerTriangulation", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryLayerTriangulation::RunTest(const FString& Parameters)
	{
		bool bSuccess = true;

		// Simple case: same vertices on both sides, no holes
		FLayerGeomDef simpleLayer(TArray<FVector>({
			FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f),
			FVector(10.0f, 0.0f, 5.0f), FVector(0.0f, 0.0f, 5.0f) }),
			1.0f, FVector(0.0f, 1.0f, 0.0f));
		TArray<FVector> simpleVerts, simpleNormals;
		TArray<int32> simpleTris;
		TArray<FVector2D> simpleUVs;
		TArray<FProcMeshTangent> simpleTangents;
		bool bSimpleTriSuccess = simpleLayer.TriangulateMesh(simpleVerts, simpleTris, simpleNormals, simpleUVs, simpleTangents);
		bSuccess = bSuccess && bSimpleTriSuccess;

		// Harder case: different vertices on both sides, hole(s)
		TArray<FVector> complexLayerVertsA({
			FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f),
			FVector(10.0f, 0.0f, 5.0f), FVector(0.0f, 0.0f, 5.0f)
		});
		TArray<FVector> complexLayerVertsB({
			FVector(0.0f, 1.0f, 0.0f), FVector(12.0f, 1.0f, 0.0f),
			FVector(12.0f, 1.0f, 6.0f), FVector(0.0f, 1.0f, 6.0f)
		});
		TArray<FPolyHole3D> holes({
			FPolyHole3D(TArray<FVector>({
				FVector(2.0f, 0.0f, 1.0f), FVector(5.0f, 0.0f, 1.0f),
				FVector(5.0f, 0.0f, 3.0f), FVector(2.0f, 0.0f, 3.0f)
			}))
		});

		FLayerGeomDef complexLayer(complexLayerVertsA, complexLayerVertsB, FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), &holes);
		TArray<FVector> complexVerts, complexNormals;
		TArray<int32> complexTris;
		TArray<FVector2D> complexUVs;
		TArray<FProcMeshTangent> complexTangents;
		bool bComplexTriSuccess = complexLayer.TriangulateMesh(complexVerts, complexTris, complexNormals, complexUVs, complexTangents);
		bSuccess = bSuccess && bComplexTriSuccess;

		return bSuccess;
	}

	FPolygon2f VerticesToTPoly(const TArray<FVector2D>& Vertices)
	{
		TArray<FVector2f> convertedVertices;
		for (const FVector2D& vertex : Vertices)
		{
			convertedVertices.Add(vertex);
		}

		return FPolygon2f(convertedVertices);
	}

	bool TestTriangulation(FAutomationTestBase* Test, const FString& Description, const TArray<FVector2D>& Vertices, const TArray<FPolyHole2D>& Holes,
		TArray<int32>& OutTriangles, TArray<FVector2D>* OutCombinedVertices, bool bCheckValid,
		bool bExpectedSuccess, int32 ExpectedNumTriIndices, int32 ExpectedNumVertices)
	{
		bool bSuccess = false;

		float expectedArea = 0.0f, computedArea = 0.0f;
		constexpr float areaEpsilon = 0.5f;

		bSuccess = UModumateGeometryStatics::TriangulateVerticesGTE(Vertices, Holes, OutTriangles, OutCombinedVertices, bCheckValid);
		if (bSuccess)
		{
			auto perimeterPoly = VerticesToTPoly(Vertices);
			float perimeterArea = perimeterPoly.Area();

			TArray<FPolygon2f> holePolys;
			Algo::Transform(Holes, holePolys, [](const FPolyHole2D& hole) { return VerticesToTPoly(hole.Points); });
			TArray<float> holeAreas;
			Algo::Transform(holePolys, holeAreas, [](const FPolygon2f& holePoly) { return holePoly.Area(); });
			float holesArea = Algo::Accumulate(holeAreas, 0.0f);
			expectedArea = perimeterArea - holesArea;

			TArray<FVector2D>& combinedVertices = *OutCombinedVertices;
			for (int32 triIndexA = 0; triIndexA < OutTriangles.Num(); triIndexA += 3)
			{
				int32 triIndexB = triIndexA + 1;
				int32 triIndexC = triIndexA + 2;
				TArray<FVector2D> triangleVertices({ combinedVertices[OutTriangles[triIndexA]], combinedVertices[OutTriangles[triIndexB]], combinedVertices[OutTriangles[triIndexC]] });
				auto triPoly = VerticesToTPoly(triangleVertices);
				computedArea += triPoly.Area();
			}
		}

		return Test->TestTrue(Description,
			(bSuccess == bExpectedSuccess) &&
			(!bExpectedSuccess || (ExpectedNumTriIndices == OutTriangles.Num())) &&
			(!bExpectedSuccess || (OutCombinedVertices == nullptr) || (ExpectedNumVertices == OutCombinedVertices->Num())) &&
			(!bExpectedSuccess || ((computedArea > 0.0f) && FMath::IsNearlyEqual(expectedArea, computedArea, areaEpsilon)))
		);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryTriangulateVertices, "Modumate.Core.Geometry.TriangulateVertices", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryTriangulateVertices::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> OutVertices;
		TArray<int32> OutTriangles;

		TArray<FVector2D> InVertices;
		TArray<FPolyHole2D> InHoles;

		InVertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		TestTriangulation(this, TEXT("simple square"), InVertices, InHoles, OutTriangles, &OutVertices, true, true, 6, 4);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryTriangulateHoles, "Modumate.Core.Geometry.TriangulateHoles", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryTriangulateHoles::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> perimeter, outVertices;
		TArray<FPolyHole2D> holes;
		TArray<int32> outTriangleIndices;

		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(25.0f, 25.0f),
			FVector2D(75.0f, 25.0f),
			FVector2D(75.0f, 75.0f),
			FVector2D(25.0f, 75.0f)
		}) };

		TestTriangulation(this, TEXT("CW square with a CCW hole in the middle"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 24, 8);

		Algo::Reverse(perimeter);
		TestTriangulation(this, TEXT("CCW square with a CCW hole in the middle"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 24, 8);
		Algo::Reverse(holes[0].Points);
		TestTriangulation(this, TEXT("CCW square with a CW hole in the middle"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 24, 8);
		Algo::Reverse(perimeter);
		TestTriangulation(this, TEXT("CW square with a CW hole in the middle"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 24, 8);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(1158.06592f, 0.0f),
			FVector2D(1158.06592f, 1337.09644f),
			FVector2D(0.0f, 1337.09644f)
		};

		holes = { FPolyHole2D({
			FVector2D(320.486328f, 276.400391f),
			FVector2D(148.749756f, 639.932617f),
			FVector2D(0.0f, 0.0f)
		})};

		TestTriangulation(this, TEXT("Square with a separate triangle touching at one corner, sharing a vertex"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 15, 7);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(148.749756f, 639.932617f),
			FVector2D(320.486328f, 276.400391f),
			FVector2D(0.0f, 0.0f),
			FVector2D(1158.06592f, 0.0f),
			FVector2D(1158.06592f, 1337.09644f),
			FVector2D(0.0f, 1337.09644f)
		};

		holes.Reset();

		TestTriangulation(this, TEXT("Square with an implicit triangular hole touching at one corner, repeating a vertex"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 15, 7);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(50.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(50.0f, 0.0f),
			FVector2D(75.0f, 50.0f),
			FVector2D(50.0f, 75.0f),
			FVector2D(25.0f, 50.0f)
		}) };

		TestTriangulation(this, TEXT("Pentagon with a diamond hole touching at one point"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 21, 9);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(30.0f, 40.0f),
			FVector2D(50.0f, 40.0f),
			FVector2D(50.0f, 60.0f),
			FVector2D(30.0f, 60.0f)
		}), FPolyHole2D({
			FVector2D(70.0f, 40.0f),
			FVector2D(50.0f, 40.0f),
			FVector2D(50.0f, 60.0f),
			FVector2D(70.0f, 60.0f),
		}) };

		TestTriangulation(this, TEXT("Square with two square holes that share an edge"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 30, 12);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(30.0f, 50.0f),
			FVector2D(40.0f, 60.0f),
			FVector2D(50.0f, 50.0f),
			FVector2D(60.0f, 60.0f),
			FVector2D(70.0f, 50.0f),
			FVector2D(60.0f, 40.0f),
			FVector2D(50.0f, 50.0f),
			FVector2D(40.0f, 40.0f)
		}) };

		TestTriangulation(this, TEXT("Square with a hole made of two diamonds that share a vertex"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 36, 12);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(30.0f, 50.0f),
			FVector2D(40.0f, 60.0f),
			FVector2D(50.0f, 50.0f),
			FVector2D(40.0f, 40.0f)
		}), FPolyHole2D({
			FVector2D(70.0f, 50.0f),
			FVector2D(60.0f, 60.0f),
			FVector2D(50.0f, 50.0f),
			FVector2D(60.0f, 40.0f),
		}) };

		TestTriangulation(this, TEXT("Square with two diamond holes that share a vertex"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 36, 12);


		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(0.0f, 25.0f),
			FVector2D(0.0f, 75.0f),
			FVector2D(50.0f, 75.0f),
			FVector2D(50.0f, 25.0f)
		})};

		TestTriangulation(this, TEXT("CW square with CW hole whose edge lies within one of the outer polygon's edges"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 18, 8);

		Algo::Reverse(holes[0].Points);
		TestTriangulation(this, TEXT("CW square with CCW hole whose edge lies within one of the outer polygon's edges"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 18, 8);
		Algo::Reverse(perimeter);
		TestTriangulation(this, TEXT("CCW square with CCW hole whose edge lies within one of the outer polygon's edges"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 18, 8);
		Algo::Reverse(holes[0].Points);
		TestTriangulation(this, TEXT("CCW square with CW hole whose edge lies within one of the outer polygon's edges"), perimeter, holes, outTriangleIndices, &outVertices, true, true, 18, 8);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryBasisVectors, "Modumate.Core.Geometry.BasisVectors", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryBasisVectors::RunTest(const FString& Parameters)
	{
		bool bSuccess = true;
		FRandomStream rand;

		for (int32 i = 0; i < 1000; ++i)
		{
			FVector basisZ = rand.VRand();
			FVector basisX, basisY;
			UModumateGeometryStatics::FindBasisVectors(basisX, basisY, basisZ);

			FVector crossZ = basisX ^ basisY;

			if (!basisZ.Equals(crossZ) || !basisY.IsNormalized() ||
				(!FMath::IsNearlyEqual(FMath::Abs(basisZ.Z), 1.0f) && (basisY.Z > 0.0f)))
			{
				bSuccess = false;
			}
		}

		bool bShortestDistanceSuccess = true;
		for (int32 i = 0; i < 1000; ++i)
		{
			FVector o1 = rand.GetUnitVector() * static_cast<float>(rand.RandRange(-500, 500));
			FVector d1 = rand.GetUnitVector();

			FVector o2 = rand.GetUnitVector() * static_cast<float>(rand.RandRange(-500, 500));
			FVector d2 = rand.GetUnitVector();

			FVector intercept1, intercept2;
			float distance;

			if (!UModumateGeometryStatics::FindShortestDistanceBetweenRays(o1, d1, o2, d2, intercept1, intercept2, distance))
			{
				bShortestDistanceSuccess = FMath::IsNearlyEqual(FMath::PointDistToLine(o1, d2, o2), distance) && bShortestDistanceSuccess;
				bShortestDistanceSuccess = FMath::IsNearlyEqual(FMath::PointDistToLine(o2, d1, o1), distance) && bShortestDistanceSuccess;
			}
			else
			{
				bShortestDistanceSuccess = FVector::Orthogonal(intercept1 - intercept2, d1) && bShortestDistanceSuccess;
				bShortestDistanceSuccess = FVector::Orthogonal(intercept1 - intercept2, d2) && bShortestDistanceSuccess;
				bShortestDistanceSuccess = FMath::IsNearlyEqual((intercept1 - intercept2).Size(), distance);
			}
		}

		return bSuccess && bShortestDistanceSuccess;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPlanarity, "Modumate.Core.Geometry.Planarity", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryPlanarity::RunTest(const FString& Parameters)
	{
		static const float planarityTestEpsilon = PLANAR_DOT_EPSILON;
		FRandomStream rand;

		uint32 firstRandInt = rand.GetUnsignedInt();
		TestTrue(TEXT("Consistent first random uint"), (firstRandInt == 907633515));

		// Keep track of which test and polygon contributes the most planarity error,
		// for both our own plane generation method and the one from GeometricObjects.
		float maxModError = 0.0f, maxGeoError = 0.0f;
		int32 maxModErrorTestIdx = INDEX_NONE, maxModErrorPointIdx = INDEX_NONE;
		int32 maxGeoErrorTestIdx = INDEX_NONE, maxGeoErrorPointIdx = INDEX_NONE;

		for (int32 testIndex = 0; testIndex < 256; ++testIndex)
		{
			FVector planeN = rand.VRand();
			float planeW = rand.FRandRange(-128.0f, 128.0f);
			FVector planeBase = planeW * planeN;
			FPlane randPlane(planeN, planeW);
			int32 numPlanePoints = rand.RandRange(4, 8);
			TArray<FVector> planePoints;

			FVector planeBasisX, planeBasisY;
			UModumateGeometryStatics::FindBasisVectors(planeBasisX, planeBasisY, planeN);

			// Generate random points on the plane, that form a circle
			for (int32 i = 0; i < numPlanePoints; ++i)
			{
				float radius = 100.0f;
				float angle = (i * 2 * PI) / numPlanePoints;
				FVector randPoint = planeBase +
					(radius * FMath::Cos(angle) * planeBasisX) +
					(radius * FMath::Sin(angle) * planeBasisY);

				float randPlaneDot = randPlane.PlaneDot(randPoint);
				bool bPointCloseToPlane = FMath::IsNearlyZero(randPlaneDot, 4.0f * planarityTestEpsilon);
				TestTrue(TEXT("Random point is close to plane"), bPointCloseToPlane);

				randPoint -= randPlaneDot * randPlane;
				randPlaneDot = randPlane.PlaneDot(randPoint);
				bool bPointOnPlane = FMath::IsNearlyZero(randPlaneDot, planarityTestEpsilon);
				TestTrue(TEXT("Fixed random point is on plane"), bPointOnPlane);
				if (!bPointOnPlane)
				{
					return false;
				}

				planePoints.Add(randPoint);
			}

			// Test the GeometricObjects method (requires a valid polygon, so we can't have out-of-order points)
			TArray<FVector3f> planePointsGTE;
			Algo::Transform(planePoints, planePointsGTE, [](const FVector& p) { return FVector3f(p.X, p.Y, p.Z); });
			FVector3f geoPlaneNormal, geoPlaneBase;
			PolygonTriangulation::ComputePolygonPlane(planePointsGTE, geoPlaneNormal, geoPlaneBase);
			TestTrue(TEXT("ComputePolygonPlane found plane from generated planar points"), geoPlaneNormal.IsNormalized());
			FPlane geoCalculatedPlane(FVector(geoPlaneBase.X, geoPlaneBase.Y, geoPlaneBase.Z), FVector(geoPlaneNormal.X, geoPlaneNormal.Y, geoPlaneNormal.Z));

			// Randomly shuffle the plane points, for our own method that doesn't assuming a polygonal shape
			TArray<FVector> shuffledPoints(planePoints);
			for (int32 i = 0; i < numPlanePoints; ++i)
			{
				shuffledPoints.Swap(i, rand.RandRange(0, numPlanePoints - 1));
			}

			// Test our own method for deriving a plane from an arbitrary set of points
			FPlane calculatedPlane;
			bool bPointsPlanar = UModumateGeometryStatics::GetPlaneFromPoints(shuffledPoints, calculatedPlane, planarityTestEpsilon);
			TestTrue(TEXT("GetPlaneFromPoints found plane from generated planar points"), bPointsPlanar);

			// Make sure that the calculated plane's normal agrees with the actual normal
			bool bCalculatedPlaneParallel = FVector::Parallel(planeN, FVector(calculatedPlane));
			TestTrue(TEXT("GetPlaneFromPoints's normal agrees with generated plane normal"), bCalculatedPlaneParallel);

			bool bGeoCalculatedPlaneParallel = FVector::Parallel(planeN, FVector(geoCalculatedPlane));
			TestTrue(TEXT("ComputePolygonPlane's normal agrees with generated plane normal"), bGeoCalculatedPlaneParallel);

			// Now, find the maximum planarity error of the points, compared to the computed plane
			for (int32 pointIdx = 0; pointIdx < numPlanePoints; ++pointIdx)
			{
				const FVector& planePoint = planePoints[pointIdx];
				float planeDist = FMath::Abs(calculatedPlane.PlaneDot(planePoint));
				if ((planeDist > maxModError) || (maxModErrorTestIdx == INDEX_NONE) || (maxModErrorPointIdx == INDEX_NONE))
				{
					maxModError = planeDist;
					maxModErrorTestIdx = testIndex;
					maxModErrorPointIdx = pointIdx;
				}

				float geoPlaneDist = FMath::Abs(geoCalculatedPlane.PlaneDot(planePoint));
				if ((geoPlaneDist > maxGeoError) || (maxGeoErrorTestIdx == INDEX_NONE) || (maxGeoErrorPointIdx == INDEX_NONE))
				{
					maxGeoError = geoPlaneDist;
					maxGeoErrorTestIdx = testIndex;
					maxGeoErrorPointIdx = pointIdx;
				}
			}
		}

		TestTrue(FString::Printf(TEXT("Maximum GetPlaneFromPoints planarity error %f (for test #%d, point #%d) are below tolerance: %f"),
			maxModError, maxModErrorTestIdx, maxModErrorPointIdx), maxModError < planarityTestEpsilon);

		TestTrue(FString::Printf(TEXT("Maximum ComputePolygonPlane planarity error %f (for test #%d, point #%d) are below tolerance: %f"),
			maxGeoError, maxGeoErrorTestIdx, maxModErrorPointIdx), maxGeoError < planarityTestEpsilon);

		uint32 finalRandInt = rand.GetUnsignedInt();
		TestTrue(TEXT("Consistent final random uint"), (finalRandInt == 3930641878));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPlaneIntersects, "Modumate.Core.Geometry.PlaneIntersects", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryPlaneIntersects::RunTest(const FString& Parameters)
	{
		FPlane plane(FVector::ZeroVector, FVector(0.0f, 0.0f, 1.0f));
		FVector tangent = FVector(1.0f, 0.0f, 0.0f);

		FVector intersection;
		TestTrue(TEXT("Segment on plane should not have intersection"), !FMath::SegmentPlaneIntersection(FVector::ZeroVector, tangent, plane, intersection));

		// would look like a block-lettered S
		TArray<FVector> S = {
			FVector(1.0f, 0.0f, 1.0f),
			FVector(3.0f, 0.0f, 1.0f),
			FVector(3.0f, 0.0f, 4.0f),
			FVector(2.0f, 0.0f, 4.0f),
			FVector(2.0f, 0.0f, 5.0f),
			FVector(3.0f, 0.0f, 5.0f),
			FVector(3.0f, 0.0f, 6.0f),
			FVector(1.0f, 0.0f, 6.0f),
			FVector(1.0f, 0.0f, 3.0f),
			FVector(2.0f, 0.0f, 3.0f),
			FVector(2.0f, 0.0f, 2.0f),
			FVector(1.0f, 0.0f, 2.0f)
		};

		FVector UnitXZ = FVector(1.0f, 0.0f, 1.0f).GetSafeNormal();

		TArray<FPlane> testPlanes = {
			FPlane(FVector::UpVector, FVector::UpVector),
			FPlane(2 * FVector::UpVector, FVector::UpVector),
			FPlane(2.5*FVector::UpVector, FVector::UpVector),
			FPlane(3 * FVector::UpVector, FVector::UpVector),
			FPlane(4 * FVector::UpVector, FVector::UpVector),
			FPlane(6 * FVector::UpVector, FVector::UpVector),
			FPlane(7 * FVector::UpVector, FVector::UpVector),
			FPlane(FVector::ForwardVector, FVector::ForwardVector),
			FPlane(2 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(2.5 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(3 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(4 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(UnitXZ, UnitXZ),
			FPlane(2 * UnitXZ, UnitXZ),
			FPlane(3 * UnitXZ, UnitXZ),
			FPlane(4 * UnitXZ, UnitXZ),
			FPlane(5 * UnitXZ, UnitXZ)
		};

		TArray<int32> expectedResults = { 2, 3, 2, 3, 3, 2, 0, 4, 6, 4, 4, 0, 0, 2, 4, 2, 2 };

		for (int32 idx = 0; idx < testPlanes.Num(); idx++)
		{
			auto& testPlane = testPlanes[idx];

			int32 intersections = 0;

			// loop through edges to calculate the amount of times there is an intersection
			for (int32 polyIdx = 0; polyIdx < S.Num(); polyIdx++)
			{
				FVector segmentStart = S[polyIdx];
				FVector segmentEnd = S[(polyIdx+1) % S.Num()];

				FVector outPoint;
				if (FMath::SegmentPlaneIntersection(segmentStart, segmentEnd, testPlane, outPoint))
				{
					intersections++;
				}
			}

			TestTrue(TEXT("Even intersections"), intersections == expectedResults[idx]);
		}

		return true;
	}

	// Modumate command parameter types test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCommandParameterTest, "Modumate.Core.Command.ParameterTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateCommandParameterTest::RunTest(const FString& Parameters)
	{
		bool bSuccess = true;

		FModumateCommandParameter floatVal("floatVal", 1.0f);
		FModumateCommandParameter intVal("intVal", 2);

		float fv = floatVal.AsFloat();
		bSuccess = bSuccess && ensureAlways(fv == 1.0f);

		int32 iv = intVal.AsInt();
		bSuccess = bSuccess && ensureAlways(iv == 2);

		fv = intVal.AsFloat();
		bSuccess = bSuccess && ensureAlways(fv == 2.0f);

		iv = floatVal.AsInt();
		bSuccess = bSuccess && ensureAlways(iv == 1.0f);

		FModumateCommand testCom = FModumateCommand(TEXT("testCommand"))
			.Param(TEXT("intVal"), 3)
			.Param(TEXT("floatVal"), 4.0f)
			.Param(TEXT("boolFalse"), false)
			.Param(TEXT("boolTrue"), true);

		FModumateFunctionParameterSet testComSet = testCom.GetParameterSet();

		iv = FModumateCommandParameter(testComSet.GetValue(TEXT("intVal")));
		bSuccess = bSuccess && ensureAlways(iv == 3);

		fv = FModumateCommandParameter(testComSet.GetValue(TEXT("floatVal")));
		bSuccess = bSuccess && ensureAlways(fv == 4.0f);

		fv = FModumateCommandParameter(testComSet.GetValue(TEXT("intVal")));
		bSuccess = bSuccess && ensureAlways(fv == 3.0f);

		iv = FModumateCommandParameter(testComSet.GetValue(TEXT("floatVal")));
		bSuccess = bSuccess && ensureAlways(iv == 4);

		iv = testComSet.GetValue(TEXT("intVal"), 6);
		bSuccess = bSuccess && ensureAlways(iv == 3);

		iv = testComSet.GetValue(TEXT("missingVal"), 6);
		bSuccess = bSuccess && ensureAlways(iv == 6);

		fv = testComSet.GetValue(TEXT("intVal"), 6);
		bSuccess = bSuccess && 	ensureAlways(fv == 3.0f);

		fv = testComSet.GetValue(TEXT("missingVal"), 6);
		bSuccess = bSuccess && ensureAlways(fv == 6.0f);

		bool bv = testComSet.GetValue(TEXT("boolTrue"));
		bSuccess = bSuccess && ensureAlways(bv);

		bv = testComSet.GetValue(TEXT("boolFalse"));
		bSuccess = bSuccess && ensureAlways(!bv);

		return bSuccess;
	}

	static bool testVariableExtraction()
	{
		TArray<FString> outVars;
		Modumate::Expression::ExtractVariables(TEXT("1.75*Globe.Radius + (5 1/2) * TotalArea + 15/(Self.Part_Frame.NativeSizeX)"), outVars);
		if (outVars.Num() != 3)
		{
			return false;
		}

		FString vars[] = { TEXT("Globe.Radius"),TEXT("TotalArea"),TEXT("Self.Part_Frame.NativeSizeX") };

		for (int32 i = 0; i < 3; ++i)
		{
			if (!outVars[i].Equals(vars[i]))
			{
				return false;
			}
		}

		return true;
	}

	static bool testVectorFormula()
	{
		FVectorExpression vf1(TEXT("Parent.SizeX-2.5"), TEXT("(1/2)*Parent.SizeY"), TEXT("(1/2)*(12+Parent.SizeZ-6)"));

		TMap<FString, float> vars;
		vars.Add(TEXT("Parent.SizeX"), 3.0f);
		vars.Add(TEXT("Parent.SizeY"), 10.0f);
		vars.Add(TEXT("Parent.SizeZ"), 8.0f);

		FVector outVector;
		if (!vf1.Evaluate(vars, outVector))
		{
			return false;
		}
		if (!FMath::IsNearlyEqual(outVector.X, 0.5f))
		{
			return false;
		}
		if (!FMath::IsNearlyEqual(outVector.Y, 5.0f))
		{
			return false;
		}
		if (!FMath::IsNearlyEqual(outVector.Z, 7.0f))
		{
			return false;
		}

		FVectorExpression vf2(TEXT("Parent.SizeX-2.5"), TEXT(""), TEXT("(1/2)*(12+Parent.SizeZ-6)"));

		outVector = FVector::ZeroVector;
		if (!vf2.Evaluate(vars, outVector))
		{
			return false;
		}

		if (outVector.Y != 0.0f)
		{
			return false;
		}

		return true;
	}

	// Modumate expressions test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateExpressionUnitTest, "Modumate.Core.Expression.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateExpressionUnitTest::RunTest(const FString& Parameters)
	{
		UE_LOG(LogUnitTest, Display, TEXT("Modumate Evaluator - Unit Test Started"));

		TMap<FString, float> vars1;
		vars1.Add("A", 1.0f);
		vars1.Add("ZED", 2.0f);
		vars1.Add("AAB", 4.5f);
		vars1.Add("W2", 6.0f);

		auto tryCase = [vars1](const TCHAR *expression, float expectedValue, bool bExpectSuccess = true) -> bool
		{
			float value = 0.0f;
			bool bEvaluationSuccess = Expression::Evaluate(vars1, expression, value);

			if (bExpectSuccess)
			{
				if (FMath::IsNearlyEqual(value, expectedValue, KINDA_SMALL_NUMBER))
				{
					UE_LOG(LogUnitTest, Display, TEXT("Formula: %s, expected: %f, got: %f - SUCCESS"), expression, expectedValue, value);
				}
				else
				{
					UE_LOG(LogUnitTest, Error, TEXT("Formula: %s, expected: %f, got: %f - FAILURE"), expression, expectedValue, value);
				}
			}
			else
			{
				if (bEvaluationSuccess)
				{
					UE_LOG(LogUnitTest, Error, TEXT("Formula: %s, expected error, got: %f - FAILURE"), expression, value);
				}
				else
				{
					UE_LOG(LogUnitTest, Display, TEXT("Formula: %s, expected error, got error - SUCCESS"), expression);
				}
			}

			return bEvaluationSuccess == bExpectSuccess;
		};

		tryCase(TEXT("(1 + (2 + 3))"), 6);
		tryCase(TEXT("(4.5 * 2)/(1 + 2)"), 3.0f);
		tryCase(TEXT("((2AAB))/(A + ZED)"), 3.0f);
		tryCase(TEXT("((ZED + 2/3))"), 2.6666f);
		tryCase(TEXT("((ZED + 2/3) * (AAB - (A+ZED)))"), 4.0f);
		tryCase(TEXT("3A"), 3.0f);
		tryCase(TEXT("2W2"), 12.0f);
		tryCase(TEXT("(1+W2)ZED"), 14.0f);

		bool ret = testVectorFormula();

		ret = testVariableExtraction() && ret;

		UE_LOG(LogUnitTest, Display, TEXT("Modumate Evaluator - Unit Test Ended"));

		return ret;
	}

	// expression expected failures test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateExpressionUnitTestFails, "Modumate.Core.Expression.UnitTestFails", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NegativeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateExpressionUnitTestFails::RunTest(const FString& Parameters)
	{
		TMap<FString, float> vars1;
		vars1.Add("A", 1.0f);
		vars1.Add("ZED", 2.0f);
		vars1.Add("AAB", 4.5f);
		vars1.Add("W2", 6.0f);

		float value = 0.0f;
		TestTrue(TEXT("Expected expression failure 1"), Expression::Evaluate(vars1, TEXT("W2W2"), value));

		TestTrue(TEXT("Expected expression failure 2"), Expression::Evaluate(vars1, TEXT("W2ZED"), value));

		return true;
	}

	// Modumate dimension format test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDimensionFormatUnitTest, "Modumate.Core.DimensionFormat.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateDimensionFormatUnitTest::RunTest(const FString& Parameters)
	{
		struct TestCase
		{
			FString dimStr;
			EDimensionFormat expectedFormat;
			double expectedCentimeters;

			TestCase(const TCHAR *ds, EDimensionFormat ef, double ec) : dimStr(ds), expectedFormat(ef), expectedCentimeters(ec) {};
		};

		static constexpr double parseTolerance = 1.0e-6;

		static TArray<TestCase> testCases = { 
			{TEXT("5 6 7"),EDimensionFormat::Error,0},
			{TEXT("3 FT 7IN"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (3.0 * 12.0 + 7)},
			{TEXT("6ft 8'"),EDimensionFormat::Error,0},
			{TEXT("5-in"),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 5.0},
			{TEXT("5-8"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0 )},
			{TEXT("5 8 7/8"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0 + 7.0 / 8.0)},
			{TEXT("5ft 8 7/8in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0 + 7.0 / 8.0)},
			{TEXT("5-ft-8-in-7/8in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0 + 7.0 / 8.0)},
			{TEXT("5-ft-8-in-7/8in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0 + 7.0 / 8.0)},
			{TEXT("in-5ft8in7/8in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0 + 7.0 / 8.0)},
			{TEXT("6\"ft"),EDimensionFormat::Error,0},
			{TEXT("5"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0)},
			{TEXT("6'"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * (6.0 * 12.0)},
			{TEXT("6'in"),EDimensionFormat::Error,0},
			{TEXT("6\"ft"),EDimensionFormat::Error,0},
			{TEXT("7\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 7.0},
			{TEXT("-8"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * (-8.0 * 12.0)},
			{TEXT("- 9 \""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 9.0},
			{TEXT("5.75"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12.0 * 5 + 9.0)},
			{TEXT("5.75\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 5.75},
			{TEXT("5/8'"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 / 8.0)},
			{TEXT("5/8"),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 / 8.0)},
			{TEXT("ryg8dds"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 8 * 12},
			{TEXT("ryg8.5ddf"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 8.5 * 12},
			{TEXT("5 8"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0)},
			{TEXT("5lq 8kj"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 * 12.0 + 8.0)},
			{TEXT("5/8\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * (5.0 / 8.0)},
			{TEXT("5ft 9in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 69},
			{TEXT("5 ft 9 in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 69},
			{TEXT("1"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 1 * 12},
			{TEXT(" -1 "),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * -1 * 12},
			{TEXT(" +1 "),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 1 * 12},
			{TEXT("33"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 33 * 12},
			{TEXT("1.2"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 1.2 * 12},
			{TEXT("-31.2"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * -31.2 * 12},
			{TEXT("0.1"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 0.1 * 12},
			{TEXT("1."),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 1.0 * 12},
			{TEXT(".1"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 0.1 * 12},
			{TEXT("4'"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 4 * 12},
			{TEXT("15ft"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 15 * 12},
			{TEXT("2.3ft"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 2.3 * 12},
			{TEXT("2.3'"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 2.3 * 12},
			{TEXT("-2.3ft"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * -2.3 * 12},
			{TEXT("3\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 3},
			{TEXT(" .5\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 0.5},
			{TEXT("3.2\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 3.2},
			{TEXT("7in"),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 7},
			{TEXT("3 7/8\""),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * (3.0 + (7.0/8.0))},
			{TEXT("7 3/4in"),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * (7.0 + (3.0/4.0))},
			{TEXT("-7 3/4in"),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * -(7.0 + (3.0 / 4.0))},
			{TEXT("1' 5\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12+5)},
			{TEXT("1' - 5\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12 + 5)},
			{TEXT("1'-5\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12 + 5)},
			{TEXT("1'--5\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12 + 5)},
			{TEXT("5ft 8in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12 * 5 + 8)},
			{TEXT("5ft - 8in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12 * 5 + 8)},
			{TEXT("4' 3/8\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12.0 * 4.0 + (3.0/8.0))},
			{TEXT("4' - 3/8\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12.0 * 4.0 + (3.0 / 8.0))},
			{TEXT("11ft 5 1/2in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12.0 * 11.0 + 5.0 + (1.0 / 2.0))},
			{TEXT("-11ft 5 1/2in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * -(12.0 * 11.0 + 5.0 + (1.0 / 2.0))},
			{TEXT("-2,301.25ft"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * -2301.25 * 12},
			{TEXT("1,100ft 5 1/2in"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12.0 * 1100.0 + 5.0 + (1.0 / 2.0))},
			{TEXT("8' 2 5/8\""),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * (12.0 * 8.0 + 2.0 + (5.0 / 8.0))},
			{TEXT("2.3maynard"),EDimensionFormat::FeetAndInches,UModumateDimensionStatics::InchesToCentimeters * 2.3 * 12.0},
			{TEXT("zed7"),EDimensionFormat::JustFeet,UModumateDimensionStatics::InchesToCentimeters * 7 * 12.0},
			{TEXT("zed7in"),EDimensionFormat::JustInches,UModumateDimensionStatics::InchesToCentimeters * 7 },
			{TEXT("6m"),EDimensionFormat::JustMeters,600.0},
			{TEXT("23mm"),EDimensionFormat::JustMillimeters,2.3},
			{TEXT("1.6m"),EDimensionFormat::JustMeters,160.0},
			{TEXT("8cm"),EDimensionFormat::JustCentimeters,8.0},
			{TEXT(" 9.6cm"),EDimensionFormat::JustCentimeters,9.6},
			{TEXT("-9.6cm"),EDimensionFormat::JustCentimeters,-9.6},
			{TEXT("6m 3cm"),EDimensionFormat::MetersAndCentimeters,603.0},
			{TEXT("1m 4.5cm"),EDimensionFormat::MetersAndCentimeters,104.5},
			{TEXT("1m - 4.5cm"),EDimensionFormat::MetersAndCentimeters,104.5},
			{TEXT("1,000m - 4.5cm"),EDimensionFormat::MetersAndCentimeters,100004.5},
			{TEXT("zed"),EDimensionFormat::Error,0.0},
			{TEXT("7 -3/4in"),EDimensionFormat::Error,0.0},
			{TEXT(",ft"),EDimensionFormat::Error,0.0},
			{TEXT(".ft"),EDimensionFormat::Error,0.0},
			{TEXT("0..1ft"),EDimensionFormat::Error,0.0},
		};

		int32 testIdx = 0;
		for (auto &tc : testCases)
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(tc.dimStr);
			ensureAlways(TestTrue(FString::Printf(TEXT("test %d"), testIdx), dim.Format == tc.expectedFormat));
			ensureAlways(TestTrue(FString::Printf(TEXT("test %d %f %f"), testIdx, dim.Centimeters, tc.expectedCentimeters), FMath::IsNearlyEqual(dim.Centimeters, tc.expectedCentimeters, parseTolerance)));
			testIdx++;
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateIDListNormalization, "Modumate.Core.IDListNormalization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateIDListNormalization::RunTest(const FString& Parameters)
	{
		TArray<int32> inListA = { 3, 5, 8, 2, 7 };
		TArray<int32> outListA = { 2, 7, 3, 5, 8 };
		TestTrue(TEXT("ListA can be normalized"), UModumateFunctionLibrary::NormalizeIDs(inListA));
		TestEqual(TEXT("ListA normalized correctly"), inListA, outListA);

		TArray<int32> inListB = { 3 };
		TArray<int32> outListB = { 3 };
		TestTrue(TEXT("ListB can be normalized"), UModumateFunctionLibrary::NormalizeIDs(inListB));
		TestEqual(TEXT("ListB normalized correctly"), inListB, outListB);

		TArray<int32> inListC = { 5, 2, 3, 2 };
		TArray<int32> outListC = { 5, 2, 3, 2 };
		TestTrue(TEXT("ListC can not be normalized"), !UModumateFunctionLibrary::NormalizeIDs(inListC));
		TestEqual(TEXT("ListC didn't get normalized"), inListC, outListC);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateEdgeDetailHashTest, "Modumate.Details.EdgeHash", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::HighPriority)
		bool FModumateEdgeDetailHashTest::RunTest(const FString& Parameters)
	{
		TArray<int32> orientationIndices;
		auto conditionSorter = [](const FEdgeDetailCondition& A, const FEdgeDetailCondition& B) -> bool {
			return A.Angle < B.Angle;
		};

		// Make a simple test edge detail
		FEdgeDetailData edgeDetail1;
		edgeDetail1.Conditions = {
			FEdgeDetailCondition{30, 0, {1, 1}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{300, 0, {1, 2}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{345, -1, {1}, EDetailParticipantType::Layered}
		};
		edgeDetail1.Overrides = {
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides()
		};
		edgeDetail1.Conditions.Sort(conditionSorter);
		FEdgeDetailData::NormalizeConditionAngles(edgeDetail1.Conditions);
		edgeDetail1.UpdateConditionHash();

		// Rotate the original edge detail conditions by various amounts to test hash rotational invariance
		TArray<float> testRotations = { 60, 135, 180 };
		for (float testRotation : testRotations)
		{
			FEdgeDetailData edgeDetailRotated = edgeDetail1;
			for (auto& condition : edgeDetailRotated.Conditions)
			{
				condition.Angle = FRotator::ClampAxis(condition.Angle + testRotation);
			}
			edgeDetailRotated.Conditions.Sort(conditionSorter);
			FEdgeDetailData::NormalizeConditionAngles(edgeDetailRotated.Conditions);
			edgeDetailRotated.UpdateConditionHash();

			TestTrue(*FString::Printf(TEXT("Edge detail rotated %.0f degrees preserves hash"), testRotation), edgeDetailRotated.CompareConditions(edgeDetail1, orientationIndices));
			TestTrue(*FString::Printf(TEXT("Edge detail rotated %.0f degrees equal by 1 rotation"), testRotation),
				(orientationIndices.Num() == 1) && (orientationIndices[0] != 0) && (orientationIndices[0] < 3));
		}

		// Make the original edge detail, but with (manually) flipped conditions
		FEdgeDetailData edgeDetailFlipped;
		edgeDetailFlipped.Conditions = {
			FEdgeDetailCondition{360 - 345, 1, {1}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{360 - 300, 0, {2, 1}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{360 - 30, 0, {1, 1}, EDetailParticipantType::Layered}
		};
		edgeDetailFlipped.Overrides = {
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides()
		};
		edgeDetailFlipped.Conditions.Sort(conditionSorter);
		FEdgeDetailData::NormalizeConditionAngles(edgeDetailFlipped.Conditions);
		edgeDetailFlipped.UpdateConditionHash();

		TestTrue(TEXT("Flipped edge detail preserves hash"), edgeDetailFlipped.CompareConditions(edgeDetail1, orientationIndices));
		TestTrue(TEXT("Flipped edge detail equal by 1 flip"), (orientationIndices.Num() == 1) && (orientationIndices[0] >= 3));

		// Make a plus-sign edge detail with full symmetry
		FEdgeDetailData edgeDetailPlus;
		edgeDetailPlus.Conditions = {
			FEdgeDetailCondition{0, 0, {1, 1}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{90, 0, {1, 1}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{180, 0, {1, 1}, EDetailParticipantType::Layered},
			FEdgeDetailCondition{270, 0, {1, 1}, EDetailParticipantType::Layered}
		};
		edgeDetailPlus.Overrides = {
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides(),
			FEdgeDetailOverrides()
		};
		edgeDetailPlus.Conditions.Sort(conditionSorter);
		FEdgeDetailData::NormalizeConditionAngles(edgeDetailPlus.Conditions);
		edgeDetailPlus.UpdateConditionHash();

		// Rotate the plus-sign edge detail
		float plusRotation = 135;
		FEdgeDetailData edgeDetailPlusRotated = edgeDetailPlus;
		for (auto& condition : edgeDetailPlusRotated.Conditions)
		{
			condition.Angle = FRotator::ClampAxis(condition.Angle + plusRotation);
		}
		edgeDetailPlusRotated.Conditions.Sort(conditionSorter);
		FEdgeDetailData::NormalizeConditionAngles(edgeDetailPlusRotated.Conditions);
		edgeDetailPlusRotated.UpdateConditionHash();

		TestTrue(TEXT("Rotated plus edge detail preserves hash"), edgeDetailPlusRotated.CompareConditions(edgeDetailPlus, orientationIndices));
		TestEqual(TEXT("Rotated plus edge detail equal by all rotations and flips"), orientationIndices.Num(), 8);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE 
