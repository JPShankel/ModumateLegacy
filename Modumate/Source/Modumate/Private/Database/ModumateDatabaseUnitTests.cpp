#include "CoreMinimal.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/Core/BIMProperties.h"

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

	if (!outVector.Y == 0.0f)
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDatabaseBIMTest, "Modumate.Database.BIM.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateDatabaseBIMTest::RunTest(const FString& Parameters)
{
	bool ret = true;
	UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Started"));

	FBIMPropertyKey vs;
	ret = vs.Scope == EBIMValueScope::None && ret;
	ret = vs.Name == TEXT("");

	vs = FBIMPropertyKey(EBIMValueScope::Assembly, TEXT("Name"));

	FBIMNameType fqn = vs.QN();
	ret = (fqn == TEXT("Assembly.Name")) && ret;

	ret = testVectorFormula() && ret;

	FModumateDatabase bimDatabase;
	bimDatabase.Init();
	bimDatabase.ReadPresetData();
	ret = bimDatabase.UnitTest() && ret;

	UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Completed %s"), ret ? TEXT("PASSED") : TEXT("FAILED"));
	return ret;
}
