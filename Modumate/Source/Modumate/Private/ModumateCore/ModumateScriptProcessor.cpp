// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateScriptProcessor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Serialization/Csv/CsvParser.h"

/*
A rule binds a regular expression to a lambda callback
*/
bool FModumateScriptProcessor::AddRule(const FString &Rule, const FRuleHandler &OnRule, bool AllowSuffix)
{
	FRuleMapping mapping;
	mapping.RuleString = Rule;
	mapping.RulePattern = std::wregex(TCHAR_TO_WCHAR(*Rule));
	// Lambdas don't pass well by value, so make a dynamic copy
	mapping.Action = OnRule;
	mapping.AllowSuffix = AllowSuffix;
	Rules.Add(mapping);
	return true;
}

/*
If a given rulemapping matches the input line, call the action
*/
bool FModumateScriptProcessor::TryRule(const FRuleMapping *RuleMapping, const FString &Line, int32 LineNum, const FErrorReporter &ErrorReporter) const
{
	std::wsmatch m;
	std::wstring wsLine(TCHAR_TO_WCHAR(*Line));
	if (RuleMapping != nullptr)
	{
		if (RuleMapping->AllowSuffix ? std::regex_search(wsLine, m, RuleMapping->RulePattern) : std::regex_match(wsLine, m, RuleMapping->RulePattern))
		{
			RuleMapping->Action(*Line, m, LineNum, ErrorReporter);
			return true;
		}
	}
	return false;
}

/*
For a given line, try all the rules...if none apply, this was a syntax error
*/
bool FModumateScriptProcessor::TryRules(const FString &Line, int32 LineNum, const FErrorReporter &ErrorReporter) const
{
	for (const auto &rule : Rules)
	{
		if (TryRule(&rule, Line, LineNum, ErrorReporter))
		{
			return true;
		}
	}
	ErrorReporter(FString::Printf(TEXT("Syntax error at line %d"), LineNum));
	return false;
}

bool FModumateScriptProcessor::ParseLines(const TArray<FString> &Lines, const FErrorReporter &ErrorReporter)
{
	bool ret = true;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		// TODO: allow clients to define their own comment pattern
		FString trimLine = Lines[i].TrimStartAndEnd();

		if (trimLine.IsEmpty() || trimLine.Left(2) == TEXT("--"))
		{
			continue;
		}

		ret = TryRules(*trimLine, i + 1, ErrorReporter) && ret;
	}
	return ret;
}

bool FModumateScriptProcessor::ParseFile(const FString &Filepath, const FErrorReporter &ErrorReporter)
{
	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *Filepath))
	{
		ErrorReporter(FString::Printf(TEXT("Could not find file %s in FModumateScriptProcessor::Parse"), *Filepath));
		return false;
	}
	return ParseLines(lines, ErrorReporter);
}

bool FModumateCSVScriptProcessor::AddRule(const FName &Rule, const FRuleHandler &RuleHandler)
{
	RuleMap.Add(Rule, RuleHandler);
	return true;
}

bool FModumateCSVScriptProcessor::ParseFile(const FString &Filepath) const
{
	FString CSVString;
	if (!FFileHelper::LoadFileToString(CSVString, *Filepath))
	{
		return false;
	}

	FCsvParser CSVParsed(CSVString);
	const FCsvParser::FRows &rows = CSVParsed.GetRows();

	if (rows.Num() == 0)
	{
		return false;
	}

	if (rows[0].Num() == 0)
	{
		return false;
	}

	for (int32 i = 0; i < rows.Num(); ++i)
	{
		FString command = rows[i][0];
		if (command.IsEmpty())
		{
			continue;
		}
		const FRuleHandler * handler = RuleMap.Find(*command);
		if (handler != nullptr)
		{
			(*handler)(rows[i],i);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateScriptProcessorUnitTest, "Modumate.Core.ScriptProcessor.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
bool FModumateScriptProcessorUnitTest::RunTest(const FString &Parameters)
{
	bool ret = true;

	// List and character match
	{
		TArray<FString> errors;
		FModumateScriptProcessor compiler;
		const FString kAllCapsWord = TEXT("[A-Z0-9]+");
		const FString kAllCapsWordList = TEXT("(") + kAllCapsWord + TEXT(")") + TEXT("(\\s+") + kAllCapsWord + TEXT(")*");

		compiler.AddRule(kAllCapsWordList, [](
			const TCHAR *originalLine,
			const FModumateScriptProcessor::FRegularExpressionMatch &match,
			int32 lineNum,
			FModumateScriptProcessor::FErrorReporter errorReporter)
			{

			},
			false);

		TArray<FString> lines;
		lines.Add(TEXT("WORD ONE TWO THREE"));
		lines.Add(TEXT("Not L^gals"));
		lines.Add(TEXT("AWORD"));

		compiler.ParseLines(lines, [&errors](const FString &err) {errors.Add(err); });

		ret = ret && (errors.Num() == 1);
	}

	//Group match
	{
		TArray<FString> errors;
		FModumateScriptProcessor compiler;
		const FString kManBearPig = TEXT("(MAN|BEAR|PIG)");

		compiler.AddRule(kManBearPig, [](
			const TCHAR *originalLine,
			const FModumateScriptProcessor::FRegularExpressionMatch &match,
			int32 lineNum,
			FModumateScriptProcessor::FErrorReporter errorReporter)
		{

		},
			false);

		TArray<FString> lines;
		lines.Add(TEXT("MANBEARPIG"));
		lines.Add(TEXT("MAN"));
		lines.Add(TEXT("BEAR"));
		lines.Add(TEXT("MANBEAR"));
		lines.Add(TEXT("BEARPIG"));

		compiler.ParseLines(lines, [&errors](const FString &err) {errors.Add(err); });

		ret = ret && (errors.Num() == 3);
	}

	// Suffix match
	{
		TArray<FString> errors;
		TArray<FString> suffices;
		FModumateScriptProcessor compiler;
		const FString kWord = TEXT("\\w+\\s*");

		compiler.AddRule(kWord, [&suffices](
			const TCHAR *originalLine,
			const FModumateScriptProcessor::FRegularExpressionMatch &match,
			int32 lineNum,
			FModumateScriptProcessor::FErrorReporter errorReporter)
			{
				if (match.suffix().str().length() > 0)
				{
					suffices.Add(FString(WCHAR_TO_TCHAR(match.suffix().str().c_str())));
				}
			},
			true);

		TArray<FString> lines;
		lines.Add(TEXT("SINGLE"));
		lines.Add(TEXT("SINGLE WITH WORD SUFFICES"));
		lines.Add(TEXT("SINGLE W&^th *rt&r 9ffices"));

		compiler.ParseLines(lines, [&errors](const FString &err) {errors.Add(err); });

		ret = ret && (errors.Num() == 0);
		ret = ret && (suffices.Num() == 2);
		if (suffices.Num() == 2)
		{
			ret = ret && suffices[0] == TEXT("WITH WORD SUFFICES");
			ret = ret && suffices[1] == TEXT("W&^th *rt&r 9ffices");
		}
	}

	return ret;
}
