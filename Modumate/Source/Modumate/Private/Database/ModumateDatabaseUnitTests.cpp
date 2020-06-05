#include "CoreMinimal.h"
#include "BIMKernel/BIMProperties.h"

namespace Modumate
{
	namespace BIM
	{
		IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDatabaseBIMTest, "Modumate.Database.BIM.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
			bool FModumateDatabaseBIMTest::RunTest(const FString& Parameters)
		{
			bool ret = true;
			UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Started"));

			FValueSpec vs;
			ret = vs.Scope == EScope::None && ret;
			ret = vs.Type == EValueType::None && ret;
			ret = vs.Name == TEXT("");

			vs = FValueSpec(EScope::Assembly, EValueType::FixedText, TEXT("Name"));

			BIM::FNameType fqn = vs.QN();
			ret = (fqn == TEXT("Assembly.Name")) && ret;

			BIM::EValueType vt = ValueTypeFromName(TEXT("Bad Value"));
			ret = (vt == EValueType::Error) && ret;

			UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Completed %s"), ret ? TEXT("PASSED") : TEXT("FAILED"));
			return ret;
		}
	}
}
