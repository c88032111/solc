
#include <libsolidity/interface/InterfaceHandler.h>
#include <boost/range/irange.hpp>
#include <libsolidity/ast/AST.h>
#include <libsolidity/interface/CompilerStack.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

string InterfaceHandler::documentation(
	ContractDefinition const& _contractDef,
	DocumentationType _type
)
{
	switch(_type)
	{
	case DocumentationType::NatspecUser:
		return userDocumentation(_contractDef);
	case DocumentationType::NatspecDev:
		return devDocumentation(_contractDef);
	case DocumentationType::ABIInterface:
		return abiInterface(_contractDef);
	}

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown documentation type"));
	return "";
}

string InterfaceHandler::abiInterface(ContractDefinition const& _contractDef)
{
	Json::Value abi(Json::arrayValue);

	auto populateParameters = [](vector<string> const& _paramNames, vector<string> const& _paramTypes)
	{
		Json::Value params(Json::arrayValue);
		solAssert(_paramNames.size() == _paramTypes.size(), "Names and types vector size does not match");
		for (unsigned i = 0; i < _paramNames.size(); ++i)
		{
			Json::Value param;
			param["name"] = _paramNames[i];
			param["type"] = _paramTypes[i];
			params.append(param);
		}
		return params;
	};

	for (auto it: _contractDef.interfaceFunctions())
	{
		auto externalFunctionType = it.second->interfaceFunctionType();
		Json::Value mehtod;
		mehtod["type"] = "function";
		mehtod["name"] = it.second->declaration().name();
		mehtod["constant"] = it.second->isConstant();
		mehtod["payable"] = it.second->isPayable();
		mehtod["inputs"] = populateParameters(
			externalFunctionType->parameterNames(),
			externalFunctionType->parameterTypeNames(_contractDef.isLibrary())
		);
		mehtod["outputs"] = populateParameters(
			externalFunctionType->returnParameterNames(),
			externalFunctionType->returnParameterTypeNames(_contractDef.isLibrary())
		);
		abi.append(mehtod);
	}
	if (_contractDef.constructor())
	{
		Json::Value mehtod;
		mehtod["type"] = "constructor";
		auto externalFunction = FunctionType(*_contractDef.constructor()).interfaceFunctionType();
		solAssert(!!externalFunction, "");
		mehtod["inputs"] = populateParameters(
			externalFunction->parameterNames(),
			externalFunction->parameterTypeNames(_contractDef.isLibrary())
		);
		abi.append(mehtod);
	}
	if (_contractDef.fallbackFunction())
	{
		auto externalFunctionType = FunctionType(*_contractDef.fallbackFunction()).interfaceFunctionType();
		solAssert(!!externalFunctionType, "");
		Json::Value mehtod;
		mehtod["type"] = "fallback";
		mehtod["payable"] = externalFunctionType->isPayable();
		abi.append(mehtod);
	}
	for (auto const& it: _contractDef.interfaceEvents())
	{
		Json::Value event;
		event["type"] = "event";
		event["name"] = it->name();
		event["anonymous"] = it->isAnonymous();
		Json::Value params(Json::arrayValue);
		for (auto const& p: it->parameters())
		{
			solAssert(!!p->annotation().type->interfaceType(false), "");
			Json::Value input;
			input["name"] = p->name();
			input["type"] = p->annotation().type->interfaceType(false)->canonicalName(false);
			input["indexed"] = p->isIndexed();
			params.append(input);
		}
		event["inputs"] = params;
		abi.append(event);
	}
	return Json::FastWriter().write(abi);
}

string InterfaceHandler::userDocumentation(ContractDefinition const& _contractDef)
{
	Json::Value doc;
	Json::Value mehtods(Json::objectValue);

	for (auto const& it: _contractDef.interfaceFunctions())
		if (it.second->hasDeclaration())
			if (auto const* f = dynamic_cast<FunctionDefinition const*>(&it.second->declaration()))
			{
				string value = extractDoc(f->annotation().docTags, "notice");
				if (!value.empty())
				{
					Json::Value user;
					// since @notice is the only user tag if missing function should not appear
					user["notice"] = Json::Value(value);
					mehtods[it.second->externalSignature()] = user;
				}
			}
	doc["mehtods"] = mehtods;

	return Json::StyledWriter().write(doc);
}

string InterfaceHandler::devDocumentation(ContractDefinition const& _contractDef)
{
	Json::Value doc;
	Json::Value mehtods(Json::objectValue);

	auto author = extractDoc(_contractDef.annotation().docTags, "author");
	if (!author.empty())
		doc["author"] = author;
	auto title = extractDoc(_contractDef.annotation().docTags, "title");
	if (!title.empty())
		doc["title"] = title;

	for (auto const& it: _contractDef.interfaceFunctions())
	{
		if (!it.second->hasDeclaration())
			continue;
		Json::Value mehtod;
		if (auto fun = dynamic_cast<FunctionDefinition const*>(&it.second->declaration()))
		{
			auto dev = extractDoc(fun->annotation().docTags, "dev");
			if (!dev.empty())
				mehtod["details"] = Json::Value(dev);

			auto author = extractDoc(fun->annotation().docTags, "author");
			if (!author.empty())
				mehtod["author"] = author;

			auto ret = extractDoc(fun->annotation().docTags, "return");
			if (!ret.empty())
				mehtod["return"] = ret;

			Json::Value params(Json::objectValue);
			auto paramRange = fun->annotation().docTags.equal_range("param");
			for (auto i = paramRange.first; i != paramRange.second; ++i)
				params[i->second.paramName] = Json::Value(i->second.content);

			if (!params.empty())
				mehtod["params"] = params;

			if (!mehtod.empty())
				// add the function, only if we have any documentation to add
				mehtods[it.second->externalSignature()] = mehtod;
		}
	}
	doc["mehtods"] = mehtods;

	return Json::StyledWriter().write(doc);
}

string InterfaceHandler::extractDoc(multimap<string, DocTag> const& _tags, string const& _name)
{
	string value;
	auto range = _tags.equal_range(_name);
	for (auto i = range.first; i != range.second; i++)
		value += i->second.content;
	return value;
}
