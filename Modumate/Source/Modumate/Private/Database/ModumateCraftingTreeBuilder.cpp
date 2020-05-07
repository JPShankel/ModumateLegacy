// Copyright 2018 Modumate, Inc. All Rights Reserved.


#include "ModumateCraftingTreeBuilder.h"
#include "ModumateObjectDatabase.h"
#include "ModumateScriptProcessor.h"
#include "Algo/Transform.h"

using namespace Modumate;

namespace Modumate {

	/*
	Build a decision tree based on a root form in the DDL
	*/
	bool FCraftingTreeBuilder::BuildDecisionTree(
		const ModumateObjectDatabase &db,
		const FString &rootName,
		Modumate::TDecisionTree<FCraftingItem> &outTree,
		TArray<FString> &errors)
	{
		/*
		The tree consists of the root node and its children
		*/
		FForm *form = Forms.Find(rootName);
		if (form == nullptr)
		{
			errors.Add(FString::Printf(TEXT("Could not find root form %s in BuildDecisionTree"), *rootName));
			return false;
		}

		Modumate::TDecisionTreeNode<FCraftingItem> &node = outTree.RootNode;
		/*
		The root node is a form by definition, so call the recursive form building function
		*/
		if (RecurseBuildFormNode(db, *form, node, errors))
		{
			// Populate the tree with synthetic guids
			int32 guid = 666;

			outTree.RootNode.WalkDepthFirst([&guid,rootName](Modumate::FCraftingDecisionTreeNode &tr) {
				tr.Data.GUID = *FString::Printf(TEXT("%s%d"), *rootName,guid++);
				return true;
			});

			// Set parent/child GUID relationships and set default selection data
			outTree.RootNode.WalkDepthFirst([&guid](Modumate::FCraftingDecisionTreeNode &tr) {
				for (auto &sf : tr.Subfields)
				{
					sf.Data.ParentGUID = tr.Data.GUID;
				}
				if (tr.Data.Type == EDecisionType::Select && tr.Subfields.Num() > 0)
				{
					tr.Data.SelectedSubnodeGUID = tr.Subfields[0].Data.GUID;
					tr.Data.DefaultSelectedSubnodeGUID = tr.Data.SelectedSubnodeGUID.ToString();
					tr.Data.AssimilateSubfieldValues(tr.Subfields[0].Data);
				}
				return true;
			});

			// Separate dynamic lists from the main tree and store in the dynamic list template list
			outTree.ProcessDynamicLists();

			return true;
		}
		return false;
	}

	/*
	RecurseBuildFormNode iterates over the list of form elements and builds a child node for each
	*/
	bool FCraftingTreeBuilder::RecurseBuildFormNode(
		const ModumateObjectDatabase &db,
		const FForm &form,
		Modumate::TDecisionTreeNode<FCraftingItem> &node,
		TArray<FString> &errors)
	{
		node.Data.Type = EDecisionType::Form;
		node.Data.Label = form.VisibleLabel;
		node.Data.Collapsible = form.Collapsible;
		node.Data.PresetFormKey = FName(*form.InternalName);
		if (form.HasPreset)
		{
			node.Data.HasPreset = true;
			node.Data.PresetType = ECraftingPresetType::User;
			node.Data.BIMScope = EnumValueByString(EBIMValueScope, form.Scope);		
		}
		for (auto &el : form.Elements)
		{
			Modumate::TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
			newNode.Data.SubCategory = node.Data.SubCategory;
			switch (el.Type)
			{
			// TODO: all open input types are simple text fields for now, selectors and such coming
			case BIM::EValueType::Dimension:
			{
				newNode.Data.Type = EDecisionType::Dimension;
				newNode.Data.Label = el.Name;
				newNode.Data.PropertyValueBindings = el.Targets;
			}
			break;

			case BIM::EValueType::Material:
			case BIM::EValueType::Formula:
			case BIM::EValueType::Number:
			case BIM::EValueType::Integer:
			case BIM::EValueType::UserString:
			{
				newNode.Data.Type = EDecisionType::String;
				newNode.Data.Label = el.Name;
				newNode.Data.PropertyValueBindings = el.Targets;
			}
			break;

			case BIM::EValueType::Form:
			{
				newNode.Data.Type = EDecisionType::Form;
				newNode.Data.Label = el.Name;

				for (auto &t : el.Targets)
				{
					FForm *newForm = Forms.Find(t);
					if (newForm != nullptr)
					{
						TDecisionTreeNode<FCraftingItem> &subformNode = newNode.Subfields.AddDefaulted_GetRef();
						subformNode.Data.SubCategory = newNode.Data.SubCategory;
						RecurseBuildFormNode(db, *newForm, subformNode, errors);
					}
					else
					{
						errors.Add(FString::Printf(TEXT("Unidentified form %s"), *t));
					}
				}
			}
			break;

			case BIM::EValueType::FixedText:
			{
				newNode.Data.Type = EDecisionType::Terminal;
				newNode.Data.Label = el.Name;
				newNode.Data.PropertyValueBindings = el.Targets;
			}
			break;

			// FillTableSelect creates a list of selectable options from the metadata stored on the object database
			case BIM::EValueType::TableSelect:
			{
				newNode.Data.Type = EDecisionType::Select;
				newNode.Data.Label = el.Name;
				newNode.Data.PropertyValueBindings = el.Targets;
				FillTableSelect(db, newNode, errors);
				newNode.Data.SelectStyle = el.SelectionStyle;
				// If we only have one selection and it has no children, it's a private constant
				if (newNode.Subfields.Num() == 1 && newNode.Subfields[0].Subfields.Num()==0)
				{
					newNode.Data.Type = EDecisionType::Private;
				}
			}
			break;

			// TODO: support non-select-style DynamicLists (none specified in project yet)
			case BIM::EValueType::DynamicList:
			case BIM::EValueType::Select:
			{
				// Get the list we want to fill
				for (auto &t : el.Targets)
				{
					FList *selectList = Lists.Find(t);
					if (selectList == nullptr)
					{
						errors.Add(FString::Printf(TEXT("Unidentified list %s"), *t));
						continue;
					}

					// And get its type
					FForm *listType = Forms.Find(selectList->ElementType);
					if (listType == nullptr)
					{
						errors.Add(FString::Printf(TEXT("Unidentified list type %s"), *selectList->ElementType));
						continue;
					}

					if (el.Type == BIM::EValueType::Select)
					{
						// single-selection lists are just private constants
						if (selectList->Elements.Num() == 1)
						{
							newNode.Data.Type = EDecisionType::Form;
						}
						else
						{
							newNode.Data.Type = EDecisionType::Select;
						}
					}
					else
					{
						newNode.Data.Type = EDecisionType::DynamicList;
					}

					newNode.Data.Label = el.Name;
					newNode.Data.SelectStyle = el.SelectionStyle;

					// For each element, build a list item node
					for (auto &sel : selectList->Elements)
					{
						Modumate::TDecisionTreeNode<FCraftingItem> &selectNode = newNode.Subfields.AddDefaulted_GetRef();
						selectNode.Data.SubCategory = newNode.Data.SubCategory;
						RecurseBuildListItemNode(db, sel, *listType, selectNode, errors);
					}
				}
			}
			break;
			default:
				errors.Add(FString::Printf(TEXT("Unsupported form value type %s"), *BIM::NameFromValueType(el.Type).ToString()));
				break;
			};
		}
		return errors.Num() == 0;
	}

	bool FCraftingTreeBuilder::RecurseBuildListItemNode(
		const ModumateObjectDatabase &db,
		const FListElement &listElem,
		const FForm &elemType,
		Modumate::TDecisionTreeNode<FCraftingItem> &node,
		TArray<FString> &errors)
	{
		node.Data.Label = FText::FromString(elemType.InternalName);

		// If a list element binds more than one variable, collect them into a form of constant values
		if (listElem.Bindings.Num() > 1)
		{
			node.Data.Type = EDecisionType::Form;
		}

		// Get bindings as qualifiable value specs with scope name
		TArray<BIM::FValueSpec> valueSpecs;
		Algo::Transform(
			listElem.Bindings,
			valueSpecs,
			[elemType](const FValueBinding &b)
			{
				const FFormElement *formEl = elemType.Elements.FindByPredicate([b](const FFormElement &el) {return el.Targets.Contains(b.Variable); });
				return BIM::FValueSpec(formEl->Type, *b.Variable, b.Value);
			});

		// If we have a subcategory, make sure everyone gets it
		FName subCategory = node.Data.SubCategory;
		for (auto &v : valueSpecs)
		{
			if (v.Name == BIM::Parameters::Subcategory)
			{
				subCategory = *v.Value.AsString();
			}
		}

		// For each variable, either add a node to the form we created (for multiple values) or simply fill in the root node (for single values)
		for (auto &v : valueSpecs)
		{
			Modumate::TDecisionTreeNode<FCraftingItem> &newNode = listElem.Bindings.Num() > 1 ? node.Subfields.AddDefaulted_GetRef() : node;
			newNode.Data.SubCategory = subCategory;
			newNode.Data.SetProperty(v.QN().ToString(), v.Value);

			switch (v.Type)
			{
			case BIM::EValueType::Dimension:
			case BIM::EValueType::Material:
			case BIM::EValueType::Formula:
			case BIM::EValueType::Number:
			case BIM::EValueType::Integer:
			case BIM::EValueType::FixedText:
			{
				BindFixedValue(newNode, v);

				if (node.Data.Label.IsEmpty() || v.Name == BIM::Parameters::Title)
				{
					node.Data.Label = newNode.Data.Label;
				}
			}
			break;

			// If the value is a form, recurse back to the form builder, so we can have lists of forms with lists of forms embedded
			case BIM::EValueType::Form:
			{
				FForm *form = Forms.Find(v.Value);
				if (form != nullptr)
				{
					RecurseBuildFormNode(db, *form, newNode, errors);
				}
				else
				{
					errors.Add(FString::Printf(TEXT("Unidentified form reference %s"), *v.Value.AsString()));
				}
			}
			break;
			default:
				errors.Add(FString::Printf(TEXT("Unsupported list element value type %s"), *BIM::NameFromValueType(v.Type).ToString()));
			};
		}
		return errors.Num() == 0;
	}

	/*
	Given a target node's subcategory, a scope and a value, find the appropriate meta-data table and populate the node
	*/
	bool FCraftingTreeBuilder::FillTableSelect(
		const ModumateObjectDatabase &db,
		TDecisionTreeNode<FCraftingItem> &node,
		TArray<FString> &errors)
	{
		bool ret = true;

		node.Data.SelectStyle = EDecisionSelectStyle::Icons;

		// Most metadata is stored in the parameter list of a base option class
		// Custom metadata, as for portals (TBD) will be handled by their own functions
		auto bindNodeMetaData = [](
			TDecisionTreeNode<FCraftingItem> &node,
			TDecisionTreeNode<FCraftingItem> &parent,
			const FCraftingOptionBase &option)
		{
			node.Data.Type = EDecisionType::Terminal;
			node.Data.SubCategory = parent.Data.SubCategory;
			node.Data.Color = option.CustomColor;
			node.Data.Icon = option.Icon;
			node.Data.Label = option.DisplayName;
			node.Data.EngineMaterial = option.EngineMaterial;
			node.Data.ProfileMesh = option.ProfileMesh;
			node.Data.Properties = option.CraftingParameters;
			for (auto &t : node.Data.PropertyValueBindings)
			if (!t.IsEmpty())
			{
				node.Data.SetProperty(t, option.Key.ToString());
			}
		};

		// We only need to build controls for the first binding...extra bindings will carry the value selected for the first
		// Otherwise we get redundant controls
		if (node.Data.PropertyValueBindings.Num() > 0)
		{
			BIM::FValueSpec valueSpec(BIM::EValueType::Select, *node.Data.PropertyValueBindings[0]);

			// TODO: portal configurations to be refactored, but keeping old tables for now
			if (valueSpec.Scope == BIM::EScope::Portal)
			{
				// Part set data to be refactored, just store key for now and look up old data in database in ModumateObjectAssembly
				if (valueSpec.Name == BIM::Parameters::PartSet)
				{
					const FCraftingPortalPartOptionSet *options = db.PortalPartOptionSets.GetData(node.Data.SubCategory);
					if (options != nullptr)
					{
						for (auto &option : options->Options)
						{
							TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
							bindNodeMetaData(newNode, node, option);
							newNode.Data.PortalPartOption = option;
							newNode.Data.SubCategory = node.Data.SubCategory;
							newNode.Data.SetProperty(BIM::EScope::Portal, BIM::Parameters::PartSet.ToString(), option.Key);

							// Gather all the material channels needed by this part set
							TSet<FName> materialSets;
							for (auto &kvp : option.PartsBySlotType)
							{
								const FPortalPart *portalPart = db.PortalParts.GetData(kvp.Value);
								if (portalPart != nullptr)
								{
									for (auto &smc : portalPart->SupportedMaterialChannels)
									{
										materialSets.Add(smc);
									}
								}
								else
								{
									errors.Add(FString::Printf(TEXT("Unrecognized portal part  %s"), *kvp.Value.ToString()));
								}
							}

							// If the part set supports material channels, make it a Form instead of a Terminal select and add selects of material/color combinations for each channel
							if (materialSets.Num() > 0)
							{
								newNode.Data.Type = EDecisionType::Form;
								for (auto &ms : materialSets)
								{
									BIM::EScope materialScope = BIM::ScopeFromName(ms);
									const FCraftingOptionSet *materialColorSet = db.FinishMaterialColors.GetData(ms);
									if (materialScope != BIM::EScope::Error && materialColorSet != nullptr)
									{
										TDecisionTreeNode<FCraftingItem> &matSetNode = newNode.Subfields.AddDefaulted_GetRef();
										matSetNode.Data.Type = EDecisionType::Select;
										matSetNode.Data.Label = FText::FromString(ms.ToString().Replace(TEXT("_"),TEXT(" ")));
										matSetNode.Data.SelectStyle = EDecisionSelectStyle::Icons;
										matSetNode.Data.SubCategory = newNode.Data.SubCategory;
										for (auto &matCol : materialColorSet->Options)
										{
											TDecisionTreeNode<FCraftingItem> &matColNode = matSetNode.Subfields.AddDefaulted_GetRef();
											matColNode.Data.Type = EDecisionType::Terminal;
											matColNode.Data.SubCategory = matSetNode.Data.SubCategory;
											bindNodeMetaData(matColNode, matSetNode, matCol);
											matColNode.Data.SetProperty(materialScope, BIM::Parameters::Color.ToString(), matCol.CraftingParameters.GetValue(TEXT("MaterialColor.Color")));
											matColNode.Data.SetProperty(materialScope, BIM::Parameters::MaterialKey.ToString(), matCol.CraftingParameters.GetValue(TEXT("MaterialColor.Material")));
										}
										matSetNode.SetSelectedSubfield(0);
									}
									else
									{
										errors.Add(FString::Printf(TEXT("Unrecognized finish material set %s"), *ms.ToString()));
									}
								}
							}
						}
					}
					else
					{
						errors.Add(FString::Printf(TEXT("Unrecognized portal part set %s"), *valueSpec.Name.ToString()));
					}
				}
				else
				// Note: Configuration is a non-terminal select when dimension data is present, other selects are typically simple terminal nodes 
				if (valueSpec.Name == BIM::Parameters::Configuration)
				{
					const FPortalAssemblyConfigurationOptionSet *options = db.PortalConfigurationOptionSets.GetData(node.Data.SubCategory);
					if (options != nullptr)
					{
						auto setDimension = [](TDecisionTreeNode<FCraftingItem> &rootNode, const FString &param, const TArray<FPortalConfigDimensionSet> &dimSet)
						{
							if (dimSet.Num() > 0)
							{
								rootNode.Data.Type = EDecisionType::Form;
								TDecisionTreeNode<FCraftingItem> &dimTable = rootNode.Subfields.AddDefaulted_GetRef();
								dimTable.Data.Type = EDecisionType::Select;
								dimTable.Data.Label = FText::FromString(param);
								dimTable.Data.SelectStyle = EDecisionSelectStyle::Dropdown;
								dimTable.Data.SubCategory = rootNode.Data.SubCategory;
								for (auto &dim : dimSet)
								{
									TDecisionTreeNode<FCraftingItem> &dimNode = dimTable.Subfields.AddDefaulted_GetRef();
									dimNode.Data.Type = EDecisionType::Terminal;
									dimNode.Data.Label = dim.DisplayName;
									dimNode.Data.SetProperty(BIM::EScope::Portal, param, dim.Key.ToString());
								}
								dimTable.SetSelectedSubfield(0);
							}
						};

						for (const auto &option : options->Options)
						{
							TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
							bindNodeMetaData(newNode, node, option);
							newNode.Data.PortalConfigurationOption = option;
							newNode.Data.SetProperty(BIM::EScope::Portal, BIM::Parameters::Configuration.ToString(), option.Key.ToString());

							setDimension(newNode, BIM::Parameters::Width.ToString(), option.SupportedWidths);
							setDimension(newNode, BIM::Parameters::Height.ToString(), option.SupportedHeights);
						}
					}
					else
					{
						errors.Add(FString::Printf(TEXT("Unrecognized portal configuration %s"), *valueSpec.Name.ToString()));
					}
				}
				else
				{
					errors.Add(FString::Printf(TEXT("Unrecognized portal property %s"), *valueSpec.Name.ToString()));
				}
			}
			else if (valueSpec.Scope == BIM::EScope::ToeKick)
			{
				const FCraftingOptionSet *options = db.ToeKickOptionSets.GetData(node.Data.SubCategory);
				if (options != nullptr)
				{
					for (const auto &option : options->Options)
					{
						TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
						bindNodeMetaData(newNode, node, option);
					}
				}
				else
				{
					errors.Add(FString::Printf(TEXT("Unrecognized toe kck subcategoryproperty %s"), *node.Data.SubCategory.ToString()));
				}
			 }
			else
			if (valueSpec.Name == BIM::Parameters::MaterialColor)
			{
				const FCraftingOptionSet *options = nullptr;
				// Find the appropriate material/color table for the given scope
				switch (valueSpec.Scope)
				{
				case BIM::EScope::Layer:
					options = db.LayerMaterialColors.GetData(node.Data.SubCategory);
					break;
				case BIM::EScope::Gap:
					options = db.GapMaterialColors.GetData(node.Data.SubCategory);
					break;
				case BIM::EScope::Module:
					options = db.ModuleMaterialColors.GetData(node.Data.SubCategory);
					break;
				default: break;
				};

				// Fill in the crafting parameters for this scope
				if (options != nullptr)
				{
					for (const auto &option : options->Options)
					{
						TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
						bindNodeMetaData(newNode, node, option);
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::Color.ToString(), option.CraftingParameters.GetValue(TEXT("MaterialColor.Color")));
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::MaterialKey.ToString(), option.CraftingParameters.GetValue(TEXT("MaterialColor.Material")));
					}
				}
				else
				{
					errors.Add(FString::Printf(TEXT("Unrecognized MaterialColor for subcategory %s"), *node.Data.SubCategory.ToString()));
				}
			}
			else
			if (valueSpec.Name == BIM::Parameters::Dimensions)
			{
				const FCraftingOptionSet *options = nullptr;
				switch (valueSpec.Scope)
				{
				case BIM::EScope::Gap:
					options = db.LayerGapOptionSets.GetData(node.Data.SubCategory);
					break;
				case BIM::EScope::Module:
					options = db.LayerModuleOptionSets.GetData(node.Data.SubCategory);
					break;
				case BIM::EScope::ToeKick:
					options = db.ToeKickOptionSets.GetData(node.Data.SubCategory);
					break;
				};

				if (options != nullptr)
				{
					for (const auto &option : options->Options)
					{
						TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
						bindNodeMetaData(newNode, node, option);

						// TODO: reconcile value names more elegantly
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::Length.ToString(), option.CraftingParameters.GetValue(TEXT("Dimension.Length")));
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::Width.ToString(), option.CraftingParameters.GetValue(TEXT("Dimension.Width")));
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::Height.ToString(), option.CraftingParameters.GetValue(TEXT("Dimension.Height")));
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::Depth.ToString(), option.CraftingParameters.GetValue(TEXT("Dimension.Depth")));
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::Thickness.ToString(), option.CraftingParameters.GetValue(TEXT("Dimension.Thickness")));
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::BevelWidth.ToString(), option.CraftingParameters.GetValue(TEXT("Dimension.BevelWidth")));
					}
				}
				else
				{
					errors.Add(FString::Printf(TEXT("Unrecognized Dimension Set scope %s"), *BIM::NameFromScope(valueSpec.Scope).ToString()));
				}
			}
			else
			if (valueSpec.Name == BIM::Parameters::TrimProfile)
			{
				const FCraftingOptionSet *options = db.ProfileOptionSets.GetData(node.Data.SubCategory);
				if (options != nullptr)
				{
					for (const auto &option : options->Options)
					{
						TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
						bindNodeMetaData(newNode, node, option);
						newNode.Data.SetProperty(valueSpec.Scope, BIM::Parameters::TrimProfile.ToString(), option.ProfileMesh.UniqueKey().ToString());
					}
				}
				else
				{
					errors.Add(FString::Printf(TEXT("Unrecognized TrimProfile %s"), *valueSpec.Name.ToString()));
				}
			}
			else if (valueSpec.Scope == BIM::EScope::Layer)
			{
				// Layer patterns
				if (valueSpec.Name == BIM::Parameters::Pattern)
				{
					const FCraftingOptionSet *options = db.PatternOptionSets.GetData(node.Data.SubCategory);
					if (options != nullptr)
					{
						for (const auto &option : options->Options)
						{
							TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
							bindNodeMetaData(newNode, node, option);
						}
					}
					else
					{
						errors.Add(FString::Printf(TEXT("Unsupported layer pattern subcategory %s"), *node.Data.SubCategory.ToString()));
					}
				}
				else if (valueSpec.Name == BIM::Parameters::Thickness)
				{
					const FCraftingOptionSet *options = db.LayerThicknessOptionSets.GetData(node.Data.SubCategory);
					if (options != nullptr)
					{
						for (const auto &option : options->Options)
						{
							TDecisionTreeNode<FCraftingItem> &newNode = node.Subfields.AddDefaulted_GetRef();
							bindNodeMetaData(newNode, node, option);
							FString units = option.CraftingParameters.GetValue(CraftingParameters::ThicknessUnits);
							newNode.Data.Value = option.CraftingParameters.GetValue(CraftingParameters::ThicknessValue);
							if (units == TEXT("cm"))
							{
								newNode.Data.Value = FString::Printf(TEXT("%f"), Units::FUnitValue::WorldCentimeters(FCString::Atof(*newNode.Data.Value)).AsWorldInches());
							}
							// the value of the new select node should be bound to every pertinent variable in the parent
							for (auto &p : node.Data.PropertyValueBindings)
							{
								newNode.Data.Properties.SetValue(p, newNode.Data.Value);
							}
						}
					}
					else
					{
						ret = false;
						errors.Add(FString::Printf(TEXT("No entry for %s in thickness table"), *node.Data.SubCategory.ToString()));
					}
				}
			}
			else
			{
				errors.Add(FString::Printf(TEXT("Unidentified table select for bound var %s"), *valueSpec.Name.ToString()));
				ret = false;
			}
		}
		else
		{
			errors.Add(FString::Printf(TEXT("No bound properties for table select")));
		}

		return ret;
	}

	void FCraftingTreeBuilder::BindFixedValue(
		TDecisionTreeNode<FCraftingItem> &node,
		const BIM::FValueSpec &b) const
	{
		node.Data.Label = FText::FromString(b.Value);
		node.Data.Value = b.Value;
		node.Data.Type = EDecisionType::Terminal;
	}


	/*
	Parse our SQL-like DDL script
	TODO: replace with a table read or move to a custom asset importer
	*/
	bool FCraftingTreeBuilder::ParseDDL(
		const FString &filepath,
		TArray<FString> &output)
	{
		State state = State::Neutral;

		FModumateScriptProcessor compiler;

		static const FString kCollapsible = TEXT("COLLAPSIBLE");
		static const FString kIcons = TEXT("ICONS");
		static const FString kDropdown = TEXT("DROPDOWN");
		static const FString kPreset = TEXT("PRESET");

		static const TArray<FString> validHints = { kCollapsible,kIcons,kDropdown,kPreset };

		/*
		BEGIN either a FORM or a LIST
		*/
		static const FString declarationString = TEXT("[A-Z0-9_]+");
		static const FString beginPattern = TEXT("\\BEGIN[\\s]+(FORM|LIST)[\\s]+(") + declarationString + TEXT(")[\\s]+(SCOPE|OF|EXTENDS)[\\s]+(") + declarationString + TEXT(")");
		compiler.AddRule(beginPattern, [&state, this](const wchar_t *originalLine, const std::wsmatch m, int32 line, FModumateScriptProcessor::TErrorReporter errorReporter) {
			TArray<FString> hints;
			if (m.suffix().length() > 0)
			{
				FString(m.suffix().str().c_str()).ParseIntoArray(hints, TEXT(" "));
			}
			if (state != State::Neutral)
			{
				errorReporter(FString::Printf(TEXT("Unexpected BEGIN statement at line %d"), line));
				return false;
			}

			for (auto &hint : hints)
			{
				if (!validHints.Contains(hint))
				{
					errorReporter(FString::Printf(TEXT("Unrecognized hint %s"), *hint));
				}
			}

			if (m.size() != 5)
			{
				errorReporter(FString::Printf(TEXT("Unexpected BEGIN statement format at line %d"), line));
				return false;
			}
			if (m[1] == TEXT("FORM"))
			{
				state = State::ReadingForm;

				CurrentForm.Elements.Reset();
				CurrentForm.InternalName = m[2].str().c_str();
				CurrentForm.VisibleLabel = FText::GetEmpty();

				CurrentForm.Collapsible = hints.Contains(kCollapsible);
				CurrentForm.HasPreset = hints.Contains(kPreset);

				if (Forms.Find(CurrentForm.InternalName) != nullptr)
				{
					errorReporter(FString::Printf(TEXT("Warning: form %s redefined at line %d"), *CurrentForm.InternalName, line));
				}

				if (m[3] == TEXT("SCOPE"))
				{
					CurrentForm.Scope = m[4].str().c_str();
				}
				else if (m[3] == TEXT("EXTENDS"))
				{
					FForm *extends = Forms.Find(m[4].str().c_str());
					if (extends != nullptr)
					{
						CurrentForm.Elements = extends->Elements;
						CurrentForm.Scope = extends->Scope;
					}
					else
					{
						errorReporter(FString::Printf(TEXT("Unidentified base form %s at line %d"), m[4].str().c_str(), line));
					}
				}
				else
				{
					errorReporter(FString::Printf(TEXT("Unexpected %s at line %d, expected 'SCOPE' or 'EXTENDS' for form"), m[3].str().c_str(), line));
					return false;
				}
			}
			else if (m[1] == TEXT("LIST"))
			{
				state = State::ReadingList;
				if (m[3] != TEXT("OF"))
				{
					errorReporter(FString::Printf(TEXT("Unexpected %s at line %d, expected 'OF' for list"), m[3].str().c_str(), line));
				}

				CurrentList.Name = m[2].str().c_str();
				if (Lists.Find(CurrentList.Name) != nullptr)
				{
					errorReporter(FString::Printf(TEXT("Warning: list %s redefined at line %d"), *CurrentList.Name, line));
				}

				FForm *listOf = Forms.Find(m[4].str().c_str());
				if (listOf == nullptr)
				{
					errorReporter(FString::Printf(TEXT("Unidentified list type %s at line %d"), m[3].str().c_str(), line));
					return false;
				}
				CurrentList.Elements.Reset();
				CurrentList.ElementType = listOf->InternalName;
			}
			else
			{
				errorReporter(FString::Printf(TEXT("Unidentified BEGIN type %s at line %d"), m[1].str().c_str(), line));
				return false;
			}
			return true;
		});

		/*
		Elements of a FORM:
		"Element Name" ElementType : Variable.Binding1..Variable.BindingN
		*/

		// Matches a normal word like Pattern or Layer
		static const FString basicWordPattern = TEXT("[\\w.\\-]+");
		// Matches quotes strings like "12-inch OC"
		static const FString quotedWordPattern = TEXT("\"[\\w,\\. \\-\\+\\)\\(\\*\\/:]+\"");
		// Matches a dotted variable like Layer.Depth
		static const FString variablePattern = basicWordPattern + TEXT("[\\.]") + basicWordPattern;
		// Matches a form element pattern like "Pattern Name" String : Pattern.Name
		static const FString elementPattern = TEXT("(") + quotedWordPattern + TEXT(")[\\s]+(") + basicWordPattern + TEXT(")[\\s]*:[\\s]*(") + variablePattern + TEXT("|") + declarationString + TEXT(")");
		compiler.AddRule(elementPattern, [&state, this](const wchar_t *originalLine, const std::wsmatch m, int32 line, FModumateScriptProcessor::TErrorReporter errorReporter) {

			int32 baseIndex = m[1].str().length() > 0 ? 1 : 4;
			TArray<FString> hints, extraBindings;
			if (m.suffix().length() > 0)
			{
				TArray<FString> suffixes;
				FString(m.suffix().str().c_str()).ParseIntoArray(suffixes, TEXT(" "));
				for (auto &suffix : suffixes)
				{
					if (suffix.Contains(TEXT(".")))
					{
						extraBindings.Add(suffix);
					}
					else
					{
						hints.Add(suffix);
					}
				}
			}
			for (auto &hint : hints)
			{
				if (!validHints.Contains(hint))
				{
					errorReporter(FString::Printf(TEXT("Unrecognized hint %s"), *hint));
				}
			}

			if (state != State::ReadingForm)
			{
				errorReporter(FString::Printf(TEXT("Unexpected form element found at %d"), line));
			}
			else if (m.size() < 4)
			{
				errorReporter(FString::Printf(TEXT("Unexpected form element statement format at line %d"), line));
			}
			else
			{
				BIM::EValueType vt = Modumate::BIM::ValueTypeFromName(m[baseIndex + 1].str().c_str());
				if (vt == BIM::EValueType::Error)
				{
					errorReporter(FString::Printf(TEXT("Unidentified value type %s at line %d"), m[baseIndex + 1].str().c_str(), line));
				}
				else
				{
					FFormElement &element = CurrentForm.Elements.AddDefaulted_GetRef();

					if (hints.Contains(kDropdown))
					{
						element.SelectionStyle = EDecisionSelectStyle::Dropdown;
					}
					else
					{
						element.SelectionStyle = EDecisionSelectStyle::Icons;
					}

					//Element names are wrapped in quotes, so 
					FString name = m[baseIndex].str().c_str();
					if (name.Len() > 2)
					{
						element.Name = FText::FromString(name.Mid(1, name.Len() - 2));
					}
					else
					{
						errorReporter(FString::Printf(TEXT("Empty element assignment at line %d"), line));
						element.Name = FText::FromString(name);
					}
					element.Type = vt;
					element.Targets.Add(m[baseIndex + 2].str().c_str());
					for (auto &eb : extraBindings)
					{
						element.Targets.Add(eb);
					}
				}
			}
		});


		/*
		Element of a list:
		[Scope.Variable1="Value1" Scope.Variable2="Value2"..Scope.VariableN="ValueN"]
		*/
		static const FString bindingValuePattern = TEXT("[A-Z_]+|") + quotedWordPattern;
		static const FString bindingPattern = TEXT("((") + variablePattern + TEXT(")[\\s]*=[\\s]*(") + bindingValuePattern + TEXT(") ?)");
		static const FString bindingListPattern = TEXT("\\[") + bindingPattern + TEXT("+\\]");

		// Matches a list element, which is one or more binding patterns wrapped with brackets
		compiler.AddRule(bindingListPattern, [&state, this](const wchar_t *originalLine, const std::wsmatch m, int32 lineNum, FModumateScriptProcessor::TErrorReporter errorReporter) {
			if (m.suffix().length() > 0)
			{
				errorReporter(FString::Printf(TEXT("Unexpected extra text in list element clause at line %d"), lineNum));
			}

			CurrentList.Elements.Add(FListElement());
			FListElement &listElement = CurrentList.Elements.Last();

			TArray<FString> foundVars;

			std::wstring bindingLine = originalLine;
			std::wregex individualBindingPattern(*bindingPattern);
			std::wsmatch allBindings;
			while (std::regex_search(bindingLine, allBindings, individualBindingPattern))
			{
				if (allBindings.size() != 4)
				{
					errorReporter(FString::Printf(TEXT("Unexpected list element format at line %d"), lineNum));
				}

				FValueBinding &binding = listElement.Bindings.AddDefaulted_GetRef();
				binding.Variable = allBindings[2].str().c_str();
				binding.Value = allBindings[3].str().c_str();

				if (binding.Value[0] == TCHAR('\"'))
				{
					binding.Value.RemoveAt(0);
				}
				if (binding.Value[binding.Value.Len() - 1] == TCHAR('\"'))
				{
					binding.Value.RemoveAt(binding.Value.Len() - 1);
				}

				foundVars.Add(binding.Variable);

				bindingLine = allBindings.suffix();
			}

			FForm *elementType = Forms.Find(CurrentList.ElementType);
			if (elementType == nullptr)
			{
				errorReporter(FString::Printf(TEXT("Unidentified list type %s at line %d"), *CurrentList.ElementType, lineNum));
			}
			else
			{
				// Get all the variable bindings in the form that defines the list
				TArray<FString> elemVars;

				for (auto &el : elementType->Elements)
				{
					for (auto &t : el.Targets)
					{
						elemVars.Add(t);
					}
				}

				for (auto &fv : foundVars)
				{
					if (!elemVars.Contains(fv))
					{
						errorReporter(FString::Printf(TEXT("Usupported variable binding %s at line %d"), *fv, lineNum));
					}
				}

				for (auto &ev : elemVars)
				{
					if (!foundVars.Contains(ev))
					{
						errorReporter(FString::Printf(TEXT("Missing required variable binding %s at line %d"), *ev, lineNum));
					}
				}
			}
		});

		/*
		End a form or a list
		*/
		compiler.AddRule(TEXT("END"), [&state, this] (const wchar_t *originalLine, const std::wsmatch &m, int32 lineNum, FModumateScriptProcessor::TErrorReporter errorReporter){
			if (state == State::ReadingForm)
			{
				static const BIM::FValueSpec formTitle(BIM::EScope::Form, BIM::EValueType::FixedText, BIM::Parameters::Title);
				for (auto &el : CurrentForm.Elements)
				{
					for (auto &target : el.Targets)
					{
						if (target == formTitle.QN().ToString())
						{
							CurrentForm.VisibleLabel = el.Name;
							break;
						}
					}
				}

				if (CurrentForm.VisibleLabel.IsEmpty())
				{
					ensureAlwaysMsgf(!CurrentForm.Collapsible, TEXT("All collapsible forms are user facing and should have user facing names"));
					CurrentForm.VisibleLabel = FText::FromString(CurrentForm.InternalName);
				}

				Forms.Add(CurrentForm.InternalName, CurrentForm);
			}
			else if (state == State::ReadingList)
			{
				Lists.Add(CurrentList.Name, CurrentList);
			}
			else if (state == State::Neutral)
			{
				errorReporter(FString::Printf(TEXT("Unexpected END statement at line %d"), lineNum));
			}
			state = State::Neutral;
		},false);

		return compiler.ParseFile(filepath, [&output](const FString &err) {output.Add(err); });
	}
}