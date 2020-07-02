// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <regex>

/*
A simple script processor that reads files one line at a time and attempts to apply rules
*/

class MODUMATE_API FModumateScriptProcessor
{
public:

	typedef std::wregex FRegularExpression;
	typedef std::wsmatch FRegularExpressionMatch;

	typedef TFunction<void(const FString &Err)> FErrorReporter;
	typedef TFunction<void(const TCHAR *OriginalLine, const FRegularExpressionMatch &Match, int32 LineNum, const FErrorReporter &ErrorReporter)> FRuleHandler;

	//Some rules allow suffixes (additional text after the match) and some require exact matches
	bool AddRule(const FString &Rule, const FRuleHandler &RuleHandler, bool AllowSuffix = true);
	bool ParseFile(const FString &Filepath, const FErrorReporter &ErrorReporter);
	bool ParseLines(const TArray<FString> &Lines, const FErrorReporter &ErrorReporter);

	~FModumateScriptProcessor() {};
	FModumateScriptProcessor() {};

private:

	// Script processor manages dynamic memory, so forbid assignment and copying
	FModumateScriptProcessor(const FModumateScriptProcessor &c) {};
	const FModumateScriptProcessor &operator=(const FModumateScriptProcessor &c) { return *this; }

	struct FRuleMapping
	{
		bool AllowSuffix;
		FString RuleString;
		FRegularExpression RulePattern;
		FRuleHandler Action;
	};

	// Rules stored in array so they will be tried in their order they are declared
	TArray<FRuleMapping> Rules;

	bool TryRule(const FRuleMapping *RuleMapping, const FString &Line, int32 LineNum, const FErrorReporter &ErrorReporter) const;
	bool TryRules(const FString &Rule, int32 LineNum, const FErrorReporter &ErrorReporter) const;
};

/*
A processor for CSV scripts where the first item in a row is the command
*/

class MODUMATE_API FModumateCSVScriptProcessor
{
public:

	FModumateCSVScriptProcessor() {}
	~FModumateCSVScriptProcessor() {};

	typedef TFunction<void(const TArray<const TCHAR*> &Row, int32 RowNumber)> FRuleHandler;

	bool AddRule(const FName &Rule, const FRuleHandler &RuleHandler);
	bool ParseFile(const FString &Filepath) const;

private:
	TMap<FName, FRuleHandler> RuleMap;
};