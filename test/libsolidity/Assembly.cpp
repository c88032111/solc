/*
	This file is part of cpp-gdtucoin.

	cpp-gdtucoin is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-gdtucoin is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-gdtucoin.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Lefteris Karapetsas <lefteris@gdtudev.com>
 * @date 2015
 * Unit tests for Assembly Items from evmasm/Assembly.h
 */

#include <string>
#include <iostream>
#include <boost/test/unit_test.hpp>
#include <libevmasm/SourceLocation.h>
#include <libevmasm/Assembly.h>
#include <libsolidity/parsing/Scanner.h>
#include <libsolidity/parsing/Parser.h>
#include <libsolidity/analysis/NameAndTypeResolver.h>
#include <libsolidity/codegen/Compiler.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/TypeChecker.h>

using namespace std;
using namespace dev::gdtu;

namespace dev
{
namespace solidity
{
namespace test
{

namespace
{

gdtu::AssemblyItems compileContract(const string& _sourceCode)
{
	ErrorList errors;
	Parser parser(errors);
	ASTPointer<SourceUnit> sourceUnit;
	BOOST_REQUIRE_NO_THROW(sourceUnit = parser.parse(make_shared<Scanner>(CharStream(_sourceCode))));
	BOOST_CHECK(!!sourceUnit);

	NameAndTypeResolver resolver({}, errors);
	solAssert(Error::containsOnlyWarnings(errors), "");
	resolver.registerDeclarations(*sourceUnit);
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			BOOST_REQUIRE_NO_THROW(resolver.resolveNamesAndTypes(*contract));
			if (!Error::containsOnlyWarnings(errors))
				return AssemblyItems();
		}
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			TypeChecker checker(errors);
			BOOST_REQUIRE_NO_THROW(checker.checkTypeRequirements(*contract));
			if (!Error::containsOnlyWarnings(errors))
				return AssemblyItems();
		}
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			Compiler compiler;
			compiler.compileContract(*contract, map<ContractDefinition const*, Assembly const*>{});

			return compiler.runtimeAssemblyItems();
		}
	BOOST_FAIL("No contract found in source.");
	return AssemblyItems();
}

void checkAssemblyLocations(AssemblyItems const& _items, vector<SourceLocation> const& _locations)
{
	BOOST_CHECK_EQUAL(_items.size(), _locations.size());
	for (size_t i = 0; i < min(_items.size(), _locations.size()); ++i)
	{
		BOOST_CHECK_MESSAGE(
			_items[i].location() == _locations[i],
			"Location mismatch for assembly item " + to_string(i) + ". Found: " +
					to_string(_items[i].location().start) + "-" +
					to_string(_items[i].location().end) + ", expected: " +
					to_string(_locations[i].start) + "-" +
					to_string(_locations[i].end));
	}
}

} // end anonymous namespace

BOOST_AUTO_TEST_SUITE(Assembly)

BOOST_AUTO_TEST_CASE(location_test)
{
	char const* sourceCode = R"(
	contract test {
		function f() returns (uint256 a) {
			return 16;
		}
	}
	)";
	shared_ptr<string const> n = make_shared<string>("source");
	AssemblyItems items = compileContract(sourceCode);
	vector<SourceLocation> locations =
		vector<SourceLocation>(18, SourceLocation(2, 75, n)) +
		vector<SourceLocation>(31, SourceLocation(20, 72, n)) +
		vector<SourceLocation>{SourceLocation(42, 51, n), SourceLocation(65, 67, n)} +
		vector<SourceLocation>(4, SourceLocation(58, 67, n)) +
		vector<SourceLocation>(3, SourceLocation(20, 72, n));
	checkAssemblyLocations(items, locations);
}

BOOST_AUTO_TEST_SUITE_END()

}
}
} // end namespaces
