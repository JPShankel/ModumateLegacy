// Copyright 2022 Modumate, Inc. All Rights Reserved.
#include "BIMKernel/BIMKernelUnitTests.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include  "BIMKernel/Presets/BIMPresetInstance.h"

#define LOCTEXT_NAMESPACE "BIMKernelUnitTests"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateBIMPropertyTest, "Modumate.BIM.Properties", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
	bool FModumateBIMPropertyTest::RunTest(const FString& Parameters)
{
	FBIMPresetInstance testPreset;
	FBIMUnitTestProperties testData;
	testData.intProp = 8675309;
	testData.stringProp = TEXT("Rikki Don't Lose That Number");
	testPreset.SetCustomData(testData);

	FString dataClass = FBIMUnitTestProperties::StaticStruct()->GetName();

	FString stringProp;
	bool bSuccess = testPreset.TryGetCustomDataString(dataClass, TEXT("stringProp"), stringProp);
	bSuccess = (stringProp == testData.stringProp) && bSuccess;

	float numProp = 0;
	bSuccess = testPreset.TryGetCustomDataNumber(dataClass, TEXT("intProp"), numProp) && bSuccess;
	bSuccess = (static_cast<int32>(numProp) == 8675309) && bSuccess;

	static FString testString = TEXT("My Set String");
	bSuccess = testPreset.TrySetCustomDataString(dataClass, TEXT("stringProp"), testString) && bSuccess;
	bSuccess = testPreset.TrySetCustomDataNumber(dataClass, TEXT("intProp"), 5551212) && bSuccess;

	FBIMUnitTestProperties getDataTest;
	testPreset.TryGetCustomData(getDataTest) && bSuccess;
	bSuccess = (getDataTest.stringProp == testString) && bSuccess;
	bSuccess = (getDataTest.intProp == 5551212) && bSuccess;

	FBIMUnitTestProperties2 templateTestData;
	templateTestData.templateTest = TEXT("template test");
	testPreset.SetCustomData(templateTestData);

	bSuccess = testPreset.TryGetCustomDataString<FBIMUnitTestProperties2>(TEXT("templateTest"), stringProp) && bSuccess;
	bSuccess = (stringProp == templateTestData.templateTest) && bSuccess;

	bSuccess = testPreset.TrySetCustomDataString<FBIMUnitTestProperties2>(TEXT("templateTest"), TEXT("set test")) && bSuccess;
	bSuccess = testPreset.TryGetCustomData(templateTestData) && bSuccess;
	bSuccess = (templateTestData.templateTest == TEXT("set test")) && bSuccess;

	return bSuccess;
}



#undef LOCTEXT_NAMESPACE 
