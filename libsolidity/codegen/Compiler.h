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
 * @date 2014
 * Solidity AST to EVM bytecode compiler.
 */

#pragma once

#include <ostream>
#include <functional>
#include <libsolidity/codegen/CompilerContext.h>
#include <libevmasm/Assembly.h>

namespace dev {
namespace solidity {

class Compiler
{
public:
	explicit Compiler(bool _optimize = false, unsigned _runs = 200):
		m_optimize(_optimize),
		m_optimizeRuns(_runs)
	{ }

	void compileContract(
		ContractDefinition const& _contract,
		std::map<ContractDefinition const*, gdtu::Assembly const*> const& _contracts
	);
	/// Compiles a contract that uses DELEGATECALL to call into a pre-deployed version of the given
	/// contract at runtime, but contains the full creation-time code.
	void compileClone(
		ContractDefinition const& _contract,
		std::map<ContractDefinition const*, gdtu::Assembly const*> const& _contracts
	);
	gdtu::Assembly const& assembly() { return m_context.assembly(); }
	gdtu::LinkerObject assembledObject() { return m_context.assembledObject(); }
	gdtu::LinkerObject runtimeObject() { return m_context.assembledRuntimeObject(m_runtimeSub); }
	/// @arg _sourceCodes is the map of input files to source code strings
	/// @arg _inJsonFromat shows whether the out should be in Json format
	Json::Value streamAssembly(std::ostream& _stream, StringMap const& _sourceCodes = StringMap(), bool _inJsonFormat = false) const
	{
		return m_context.streamAssembly(_stream, _sourceCodes, _inJsonFormat);
	}
	/// @returns Assembly items of the normal compiler context
	gdtu::AssemblyItems const& assemblyItems() const { return m_context.assembly().items(); }
	/// @returns Assembly items of the runtime compiler context
	gdtu::AssemblyItems const& runtimeAssemblyItems() const { return m_context.assembly().sub(m_runtimeSub).items(); }

	/// @returns the entry label of the given function. Might return an AssemblyItem of type
	/// UndefinedItem if it does not exist yet.
	gdtu::AssemblyItem functionEntryLabel(FunctionDefinition const& _function) const;

private:
	bool const m_optimize;
	unsigned const m_optimizeRuns;
	CompilerContext m_context;
	size_t m_runtimeSub = size_t(-1); ///< Identifier of the runtime sub-assembly, if present.
	CompilerContext m_runtimeContext;
};

}
}
