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
 * @date 2015
 * Parses and analyses the doc strings.
 * Stores the parsing results in the AST annotations and reports errors.
 */

#include <libsolidity/analysis/DocStringAnalyser.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/parsing/DocStringParser.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

bool DocStringAnalyser::analyseDocStrings(SourceUnit const& _sourceUnit)
{
	m_errorOccured = false;
	_sourceUnit.accept(*this);

	return !m_errorOccured;
}

bool DocStringAnalyser::visit(ContractDefinition const& _node)
{
	static const set<string> validTags = set<string>{"author", "title", "dev", "notice", "why3"};
	parseDocStrings(_node, _node.annotation(), validTags, "contracts");

	return true;
}

bool DocStringAnalyser::visit(FunctionDefinition const& _node)
{
	handleCallable(_node, _node, _node.annotation());
	return true;
}

bool DocStringAnalyser::visit(ModifierDefinition const& _node)
{
	handleCallable(_node, _node, _node.annotation());

	return true;
}

bool DocStringAnalyser::visit(EventDefinition const& _node)
{
	handleCallable(_node, _node, _node.annotation());

	return true;
}

bool DocStringAnalyser::visitNode(ASTNode const& _node)
{
	if (auto node = dynamic_cast<Statement const*>(&_node))
	{
		static const set<string> validTags = {"why3"};
		parseDocStrings(*node, node->annotation(), validTags, "statements");
	}
	return true;
}

void DocStringAnalyser::handleCallable(
	CallableDeclaration const& _callable,
	Documented const& _node,
	DocumentedAnnotation& _annotation
)
{
	static const set<string> validTags = set<string>{"author", "dev", "notice", "return", "param", "why3"};
	parseDocStrings(_node, _annotation, validTags, "functions");

	set<string> validParams;
	for (auto const& p: _callable.parameters())
		validParams.insert(p->name());
	if (_callable.returnParameterList())
		for (auto const& p: _callable.returnParameterList()->parameters())
			validParams.insert(p->name());
	auto paramRange = _annotation.docTags.equal_range("param");
	for (auto i = paramRange.first; i != paramRange.second; ++i)
		if (!validParams.count(i->second.paramName))
			appendError(
				"Documented parameter \"" +
				i->second.paramName +
				"\" not found in the parameter list of the function."
			);
}

void DocStringAnalyser::parseDocStrings(
	Documented const& _node,
	DocumentedAnnotation& _annotation,
	set<string> const& _validTags,
	string const& _nodeName
)
{
	DocStringParser parser;
	if (_node.documentation() && !_node.documentation()->empty())
	{
		if (!parser.parse(*_node.documentation(), m_errors))
			m_errorOccured = true;
		_annotation.docTags = parser.tags();
	}
	for (auto const& docTag: _annotation.docTags)
		if (!_validTags.count(docTag.first))
			appendError("Doc tag @" + docTag.first + " not valid for " + _nodeName + ".");
}

void DocStringAnalyser::appendError(string const& _description)
{
	auto err = make_shared<Error>(Error::Type::DocstringParsingError);
	*err << errinfo_comment(_description);
	m_errors.push_back(err);
	m_errorOccured = true;
}
