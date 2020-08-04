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

			FBIMPropertyValue vs;
			ret = vs.Scope == EBIMValueScope::None && ret;
			ret = vs.Type == EBIMValueType::None && ret;
			ret = vs.Name == TEXT("");

			vs = FBIMPropertyValue(EBIMValueScope::Assembly, EBIMValueType::FixedText, TEXT("Name"));

			FBIMNameType fqn = vs.QN();
			ret = (fqn == TEXT("Assembly.Name")) && ret;

			UE_LOG(LogUnitTest, Display, TEXT("Modumate BIM Schema - Unit Test Completed %s"), ret ? TEXT("PASSED") : TEXT("FAILED"));
			return ret;
		}
	}
}
