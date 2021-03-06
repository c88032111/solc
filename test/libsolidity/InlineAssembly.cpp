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
 * @author Christian <c@gdtudev.com>
 * @date 2016
 * Unit tests for inline assembly.
 */

#include <string>
#include <memory>
#include <libevmasm/Assembly.h>
#include <libsolidity/parsing/Scanner.h>
#include <libsolidity/inlineasm/AsmStack.h>
#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/ast/AST.h>
#include "../TestHelper.h"

using namespace std;

namespace dev
{
namespace solidity
{
namespace test
{

namespace
{

bool successParse(std::string const& _source, bool _assemble = false)
{
	assembly::InlineAssemblyStack stack;
	try
	{
		if (!stack.parse(std::make_shared<Scanner>(CharStream(_source))))
			return false;
		if (_assemble)
		{
			stack.assemble();
			if (!stack.errors().empty())
				return false;
		}
	}
	catch (FatalError const&)
	{
		if (Error::containsErrorOfType(stack.errors(), Error::Type::ParserError))
			return false;
	}
	if (Error::containsErrorOfType(stack.errors(), Error::Type::ParserError))
		return false;

	BOOST_CHECK(Error::containsOnlyWarnings(stack.errors()));
	return true;
}

bool successAssemble(string const& _source)
{
	return successParse(_source, true);
}

}


BOOST_AUTO_TEST_SUITE(SolidityInlineAssembly)

BOOST_AUTO_TEST_CASE(smoke_test)
{
	BOOST_CHECK(successParse("{ }"));
}

BOOST_AUTO_TEST_CASE(simple_instructions)
{
	BOOST_CHECK(successParse("{ dup1 dup1 mul dup1 sub }"));
}

BOOST_AUTO_TEST_CASE(keywords)
{
	BOOST_CHECK(successParse("{ byte return }"));
}

BOOST_AUTO_TEST_CASE(constants)
{
	BOOST_CHECK(successParse("{ 7 8 mul }"));
}

BOOST_AUTO_TEST_CASE(vardecl)
{
	BOOST_CHECK(successParse("{ let x := 7 }"));
}

BOOST_AUTO_TEST_CASE(assignment)
{
	BOOST_CHECK(successParse("{ 7 8 add =: x }"));
}

BOOST_AUTO_TEST_CASE(label)
{
	BOOST_CHECK(successParse("{ 7 abc: 8 eq abc jump }"));
}

BOOST_AUTO_TEST_CASE(label_complex)
{
	BOOST_CHECK(successParse("{ 7 abc: 8 eq jump(abc) jumpi(eq(7, 8), abc) }"));
}

BOOST_AUTO_TEST_CASE(functional)
{
	BOOST_CHECK(successParse("{ add(7, mul(6, x)) add mul(7, 8) }"));
}

BOOST_AUTO_TEST_CASE(functional_assignment)
{
	BOOST_CHECK(successParse("{ x := 7 }"));
}

BOOST_AUTO_TEST_CASE(functional_assignment_complex)
{
	BOOST_CHECK(successParse("{ x := add(7, mul(6, x)) add mul(7, 8) }"));
}

BOOST_AUTO_TEST_CASE(vardecl_complex)
{
	BOOST_CHECK(successParse("{ let x := add(7, mul(6, x)) add mul(7, 8) }"));
}

BOOST_AUTO_TEST_CASE(blocks)
{
	BOOST_CHECK(successParse("{ let x := 7 { let y := 3 } { let z := 2 } }"));
}

BOOST_AUTO_TEST_CASE(string_literals)
{
	BOOST_CHECK(successAssemble("{ let x := \"12345678901234567890123456789012\" }"));
}

BOOST_AUTO_TEST_CASE(oversize_string_literals)
{
	BOOST_CHECK(!successAssemble("{ let x := \"123456789012345678901234567890123\" }"));
}

BOOST_AUTO_TEST_SUITE_END()

}
}
} // end namespaces
