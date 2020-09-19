#include "CoreMinimal.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "BIMKernel/BIMProperties.h"

static bool testVectorFormula()
{
	Modumate::Expression::FVectorExpression vf1(TEXT("Parent.NativeSizeX-2.5"), TEXT("(1/2)*Parent.NativeSizeY"), TEXT("(1/2)*(12+Parent.NativeSizeZ-6)"));

	TMap<FString, float> vars;
	vars.Add(TEXT("Parent.NativeSizeX"), 3.0f);
	vars.Add(TEXT("Parent.NativeSizeY"), 10.0f);
	vars.Add(TEXT("Parent.NativeSizeZ"), 8.0f);

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
	
	Modumate::Expression::FVectorExpression vf2(TEXT("Parent.NativeSizeX-2.5"), TEXT(""), TEXT("(1/2)*(12+Parent.NativeSizeZ-6)"));
	
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

	FBIMPropertyValue vs;
	ret = vs.Scope == EBIMValueScope::None && ret;
	ret = vs.Type == EBIMValueType::None && ret;
	ret = vs.Name == TEXT("");

	vs = FBIMPropertyValue(EBIMValueScope::Assembly, EBIMValueType::String, TEXT("Name"));

	FBIMNameType fqn = vs.QN();
	ret = (fqn == TEXT("Assembly.Name")) && ret;

	ret = testVectorFormula() && ret;

	UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Completed %s"), ret ? TEXT("PASSED") : TEXT("FAILED"));
	return ret;
}
