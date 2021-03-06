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
 * @author Gav Wood <g@gdtudev.com>
 * @date 2014
 * Full-stack compiler that converts a source code string to bytecode.
 */

#pragma once

#include <ostream>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <boost/noncopyable.hpp>
#include <json/json.h>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>
#include <libevmasm/SourceLocation.h>
#include <libevmasm/LinkerObject.h>
#include <libsolidity/interface/Exceptions.h>

namespace dev
{

namespace gdtu
{
class Assembly;
class AssemblyItem;
using AssemblyItems = std::vector<AssemblyItem>;
}

namespace solidity
{

// forward declarations
class Scanner;
class ContractDefinition;
class FunctionDefinition;
class SourceUnit;
class Compiler;
class GlobalContext;
class InterfaceHandler;
class Error;

enum class DocumentationType: uint8_t
{
	NatspecUser = 1,
	NatspecDev,
	ABIInterface
};

/**
 * Easy to use and self-contained Solidity compiler with as few header dependencies as possible.
 * It holds state and can be used to either step through the compilation stages (and abort e.g.
 * before compilation to bytecode) or run the whole compilation in one call.
 */
class CompilerStack: boost::noncopyable
{
public:
	struct ReadFileResult
	{
		bool success;
		std::string contentsOrErrorMesage;
	};

	/// File reading callback.
	using ReadFileCallback = std::function<ReadFileResult(std::string const&)>;

	/// Creates a new compiler stack.
	/// @param _readFile callback to used to read files for import statements. Should return
	explicit CompilerStack(ReadFileCallback const& _readFile = ReadFileCallback());

	/// Sets path remappings in the format "context:prefix=target"
	void setRemappings(std::vector<std::string> const& _remappings);

	/// Resets the compiler to a state where the sources are not parsed or even removed.
	void reset(bool _keepSources = false);

	/// Adds a source object (e.g. file) to the parser. After this, parse has to be called again.
	/// @returns true if a source object by the name already existed and was replaced.
	void addSources(StringMap const& _nameContents, bool _isLibrary = false)
	{
		for (auto const& i: _nameContents) addSource(i.first, i.second, _isLibrary);
	}
	bool addSource(std::string const& _name, std::string const& _content, bool _isLibrary = false);
	void setSource(std::string const& _sourceCode);
	/// Parses all source units that were added
	/// @returns false on error.
	bool parse();
	/// Sets the given source code as the only source unit apart from standard sources and parses it.
	/// @returns false on error.
	bool parse(std::string const& _sourceCode);
	/// @returns a list of the contract names in the sources.
	std::vector<std::string> contractNames() const;
	std::string defaultContractName() const;

	/// Compiles the source units that were previously added and parsed.
	/// @returns false on error.
	bool compile(bool _optimize = false, unsigned _runs = 200);
	/// Parses and compiles the given source code.
	/// @returns false on error.
	bool compile(std::string const& _sourceCode, bool _optimize = false);

	/// Inserts the given addresses into the linker objects of all compiled contracts.
	void link(std::map<std::string, h160> const& _libraries);

	/// Tries to translate all source files into a language suitable for formal analysis.
	/// @param _errors list to store errors - defaults to the internal error list.
	/// @returns false on error.
	bool prepareFormalAnalysis(ErrorList* _errors = nullptr);
	std::string const& formalTranslation() const { return m_formalTranslation; }

	/// @returns the assembled object for a contract.
	gdtu::LinkerObject const& object(std::string const& _contractName = "") const;
	/// @returns the runtime object for the contract.
	gdtu::LinkerObject const& runtimeObject(std::string const& _contractName = "") const;
	/// @returns the bytecode of a contract that uses an already deployed contract via DELEGATECALL.
	/// The returned bytes will contain a sequence of 20 bytes of the format "XXX...XXX" which have to
	/// substituted by the actual address. Note that this sequence starts end ends in three X
	/// characters but can contain anything in between.
	gdtu::LinkerObject const& cloneObject(std::string const& _contractName = "") const;
	/// @returns normal contract assembly items
	gdtu::AssemblyItems const* assemblyItems(std::string const& _contractName = "") const;
	/// @returns runtime contract assembly items
	gdtu::AssemblyItems const* runtimeAssemblyItems(std::string const& _contractName = "") const;
	/// @returns the string that provides a mapping between bytecode and sourcecode or a nullptr
	/// if the contract does not (yet) have bytecode.
	std::string const* sourceMapping(std::string const& _contractName = "") const;
	/// @returns the string that provides a mapping between runtime bytecode and sourcecode.
	/// if the contract does not (yet) have bytecode.
	std::string const* runtimeSourceMapping(std::string const& _contractName = "") const;
	/// @returns hash of the runtime bytecode for the contract, i.e. the code that is
	/// returned by the constructor or the zero-h256 if the contract still needs to be linked or
	/// does not have runtime code.
	dev::h256 contractCodeHash(std::string const& _contractName = "") const;

	/// Streams a verbose version of the assembly to @a _outStream.
	/// @arg _sourceCodes is the map of input files to source code strings
	/// @arg _inJsonFromat shows whether the out should be in Json format
	/// Prerequisite: Successful compilation.
	Json::Value streamAssembly(std::ostream& _outStream, std::string const& _contractName = "", StringMap _sourceCodes = StringMap(), bool _inJsonFormat = false) const;

	/// @returns the list of sources (paths) used
	std::vector<std::string> sourceNames() const;
	/// @returns a mapping assigning each source name its index inside the vector returned
	/// by sourceNames().
	std::map<std::string, unsigned> sourceIndices() const;
	/// @returns a string representing the contract interface in JSON.
	/// Prerequisite: Successful call to parse or compile.
	std::string const& interface(std::string const& _contractName = "") const;
	/// @returns a string representing the contract's documentation in JSON.
	/// Prerequisite: Successful call to parse or compile.
	/// @param type The type of the documentation to get.
	/// Can be one of 4 types defined at @c DocumentationType
	std::string const& metadata(std::string const& _contractName, DocumentationType _type) const;

	/// @returns the previously used scanner, useful for counting lines during error reporting.
	Scanner const& scanner(std::string const& _sourceName = "") const;
	/// @returns the parsed source unit with the supplied name.
	SourceUnit const& ast(std::string const& _sourceName = "") const;
	/// @returns the parsed contract with the supplied name. Throws an exception if the contract
	/// does not exist.
	ContractDefinition const& contractDefinition(std::string const& _contractName) const;

	/// @returns the offset of the entry point of the given function into the list of assembly items
	/// or zero if it is not found or does not exist.
	size_t functionEntryPoint(
		std::string const& _contractName,
		FunctionDefinition const& _function
	) const;

	/// Helper function for logs printing. Do only use in error cases, it's quite expensive.
	/// line and columns are numbered starting from 1 with following order:
	/// start line, start column, end line, end column
	std::tuple<int, int, int, int> positionFromSourceLocation(SourceLocation const& _sourceLocation) const;

	/// @returns the list of errors that occured during parsing and type checking.
	ErrorList const& errors() const { return m_errors; }

private:
	/**
	 * Information pertaining to one source unit, filled gradually during parsing and compilation.
	 */
	struct Source
	{
		std::shared_ptr<Scanner> scanner;
		std::shared_ptr<SourceUnit> ast;
		bool isLibrary = false;
		void reset() { scanner.reset(); ast.reset(); }
	};

	struct Contract
	{
		ContractDefinition const* contract = nullptr;
		std::shared_ptr<Compiler> compiler;
		gdtu::LinkerObject object;
		gdtu::LinkerObject runtimeObject;
		gdtu::LinkerObject cloneObject;
		mutable std::unique_ptr<std::string const> interface;
		mutable std::unique_ptr<std::string const> userDocumentation;
		mutable std::unique_ptr<std::string const> devDocumentation;
		mutable std::unique_ptr<std::string const> sourceMapping;
		mutable std::unique_ptr<std::string const> runtimeSourceMapping;
	};

	/// Loads the missing sources from @a _ast (named @a _path) using the callback
	/// @a m_readFile and stores the absolute paths of all imports in the AST annotations.
	/// @returns the newly loaded sources.
	StringMap loadMissingSources(SourceUnit const& _ast, std::string const& _path);
	std::string applyRemapping(std::string const& _path, std::string const& _context);
	void resolveImports();
	/// Checks whether there are libraries with the same name, reports that as an error and
	/// @returns false in this case.
	bool checkLibraryNameClashes();
	/// @returns the absolute path corresponding to @a _path relative to @a _reference.
	std::string absolutePath(std::string const& _path, std::string const& _reference) const;
	/// Compile a single contract and put the result in @a _compiledContracts.
	void compileContract(
		bool _optimize,
		unsigned _runs,
		ContractDefinition const& _contract,
		std::map<ContractDefinition const*, gdtu::Assembly const*>& _compiledContracts
	);

	Contract const& contract(std::string const& _contractName = "") const;
	Source const& source(std::string const& _sourceName = "") const;

	std::string computeSourceMapping(gdtu::AssemblyItems const& _items) const;

	struct Remapping
	{
		std::string context;
		std::string prefix;
		std::string target;
	};

	ReadFileCallback m_readFile;
	/// list of path prefix remappings, e.g. mylibrary: github.com/gdtucoin = /usr/local/gdtucoin
	/// "context:prefix=target"
	std::vector<Remapping> m_remappings;
	bool m_parseSuccessful;
	std::map<std::string const, Source> m_sources;
	std::shared_ptr<GlobalContext> m_globalContext;
	std::vector<Source const*> m_sourceOrder;
	std::map<std::string const, Contract> m_contracts;
	std::string m_formalTranslation;
	ErrorList m_errors;
};

}
}
