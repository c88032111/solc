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
 * Utilities for the solidity compiler.
 */

#pragma once

#include <ostream>
#include <stack>
#include <queue>
#include <utility>
#include <libevmasm/Instruction.h>
#include <libevmasm/Assembly.h>
#include <libsolidity/ast/ASTForward.h>
#include <libsolidity/ast/Types.h>
#include <libsolidity/ast/ASTAnnotations.h>
#include <libdevcore/Common.h>

namespace dev {
namespace solidity {


/**
 * Context to be shared by all units that compile the same contract.
 * It stores the generated bytecode and the position of identifiers in memory and on the stack.
 */
class CompilerContext
{
public:
	void addMagicGlobal(MagicVariableDeclaration const& _declaration);
	void addStateVariable(VariableDeclaration const& _declaration, u256 const& _storageOffset, unsigned _byteOffset);
	void addVariable(VariableDeclaration const& _declaration, unsigned _offsetToCurrent = 0);
	void removeVariable(VariableDeclaration const& _declaration);

	void setCompiledContracts(std::map<ContractDefinition const*, gdtu::Assembly const*> const& _contracts) { m_compiledContracts = _contracts; }
	gdtu::Assembly const& compiledContract(ContractDefinition const& _contract) const;

	void setStackOffset(int _offset) { m_asm.setDeposit(_offset); }
	void adjustStackOffset(int _adjustment) { m_asm.adjustDeposit(_adjustment); }
	unsigned stackHeight() const { solAssert(m_asm.deposit() >= 0, ""); return unsigned(m_asm.deposit()); }

	bool isMagicGlobal(Declaration const* _declaration) const { return m_magicGlobals.count(_declaration) != 0; }
	bool isLocalVariable(Declaration const* _declaration) const;
	bool isStateVariable(Declaration const* _declaration) const { return m_stateVariables.count(_declaration) != 0; }

	/// @returns the entry label of the given function and creates it if it does not exist yet.
	gdtu::AssemblyItem functionEntryLabel(Declaration const& _declaration);
	/// @returns the entry label of the given function. Might return an AssemblyItem of type
	/// UndefinedItem if it does not exist yet.
	gdtu::AssemblyItem functionEntryLabelIfExists(Declaration const& _declaration) const;
	void setInheritanceHierarchy(std::vector<ContractDefinition const*> const& _hierarchy) { m_inheritanceHierarchy = _hierarchy; }
	/// @returns the entry label of the given function and takes overrides into account.
	gdtu::AssemblyItem virtualFunctionEntryLabel(FunctionDefinition const& _function);
	/// @returns the entry label of a function that overrides the given declaration from the most derived class just
	/// above _base in the current inheritance hierarchy.
	gdtu::AssemblyItem superFunctionEntryLabel(FunctionDefinition const& _function, ContractDefinition const& _base);
	FunctionDefinition const* nextConstructor(ContractDefinition const& _contract) const;

	/// @returns the next function in the queue of functions that are still to be compiled
	/// (i.e. that were referenced during compilation but where we did not yet generate code for).
	/// Returns nullptr if the queue is empty. Does not remove the function from the queue,
	/// that will only be done by startFunction below.
	Declaration const* nextFunctionToCompile() const;
	/// Resets function specific members, inserts the function entry label and marks the function
	/// as "having code".
	void startFunction(Declaration const& _function);

	ModifierDefinition const& functionModifier(std::string const& _name) const;
	/// Returns the distance of the given local variable from the bottom of the stack (of the current function).
	unsigned baseStackOffsetOfVariable(Declaration const& _declaration) const;
	/// If supplied by a value returned by @ref baseStackOffsetOfVariable(variable), returns
	/// the distance of that variable from the current top of the stack.
	unsigned baseToCurrentStackOffset(unsigned _baseOffset) const;
	/// Converts an offset relative to the current stack height to a value that can be used later
	/// with baseToCurrentStackOffset to point to the same stack element.
	unsigned currentToBaseStackOffset(unsigned _offset) const;
	/// @returns pair of slot and byte offset of the value inside this slot.
	std::pair<u256, unsigned> storageLocationOfVariable(Declaration const& _declaration) const;

	/// Appends a JUMPI instruction to a new tag and @returns the tag
	gdtu::AssemblyItem appendConditionalJump() { return m_asm.appendJumpI().tag(); }
	/// Appends a JUMPI instruction to @a _tag
	CompilerContext& appendConditionalJumpTo(gdtu::AssemblyItem const& _tag) { m_asm.appendJumpI(_tag); return *this; }
	/// Appends a JUMP to a new tag and @returns the tag
	gdtu::AssemblyItem appendJumpToNew() { return m_asm.appendJump().tag(); }
	/// Appends a JUMP to a tag already on the stack
	CompilerContext&  appendJump(gdtu::AssemblyItem::JumpType _jumpType = gdtu::AssemblyItem::JumpType::Ordinary);
	/// Returns an "ErrorTag"
	gdtu::AssemblyItem errorTag() { return m_asm.errorTag(); }
	/// Appends a JUMP to a specific tag
	CompilerContext& appendJumpTo(gdtu::AssemblyItem const& _tag) { m_asm.appendJump(_tag); return *this; }
	/// Appends pushing of a new tag and @returns the new tag.
	gdtu::AssemblyItem pushNewTag() { return m_asm.append(m_asm.newPushTag()).tag(); }
	/// @returns a new tag without pushing any opcodes or data
	gdtu::AssemblyItem newTag() { return m_asm.newTag(); }
	/// Adds a subroutine to the code (in the data section) and pushes its size (via a tag)
	/// on the stack. @returns the assembly item corresponding to the pushed subroutine, i.e. its offset.
	gdtu::AssemblyItem addSubroutine(gdtu::Assembly const& _assembly) { return m_asm.appendSubSize(_assembly); }
	/// Pushes the size of the final program
	void appendProgramSize() { return m_asm.appendProgramSize(); }
	/// Adds data to the data section, pushes a reference to the stack
	gdtu::AssemblyItem appendData(bytes const& _data) { return m_asm.append(_data); }
	/// Appends the address (virtual, will be filled in by linker) of a library.
	void appendLibraryAddress(std::string const& _identifier) { m_asm.appendLibraryAddress(_identifier); }
	/// Resets the stack of visited nodes with a new stack having only @c _node
	void resetVisitedNodes(ASTNode const* _node);
	/// Pops the stack of visited nodes
	void popVisitedNodes() { m_visitedNodes.pop(); updateSourceLocation(); }
	/// Pushes an ASTNode to the stack of visited nodes
	void pushVisitedNodes(ASTNode const* _node) { m_visitedNodes.push(_node); updateSourceLocation(); }

	/// Append elements to the current instruction list and adjust @a m_stackOffset.
	CompilerContext& operator<<(gdtu::AssemblyItem const& _item) { m_asm.append(_item); return *this; }
	CompilerContext& operator<<(Instruction _instruction) { m_asm.append(_instruction); return *this; }
	CompilerContext& operator<<(u256 const& _value) { m_asm.append(_value); return *this; }
	CompilerContext& operator<<(bytes const& _data) { m_asm.append(_data); return *this; }

	/// Appends inline assembly. @a _replacements are string-matching replacements that are performed
	/// prior to parsing the inline assembly.
	/// @param _localVariables assigns stack positions to variables with the last one being the stack top
	void appendInlineAssembly(
		std::string const& _assembly,
		std::vector<std::string> const& _localVariables = std::vector<std::string>(),
		std::map<std::string, std::string> const& _replacements = std::map<std::string, std::string>{}
	);

	/// Prepends "PUSH <compiler version number> POP"
	void injectVersionStampIntoSub(size_t _subIndex);

	void optimise(unsigned _runs = 200) { m_asm.optimise(true, true, _runs); }

	gdtu::Assembly const& assembly() const { return m_asm; }
	/// @returns non-const reference to the underlying assembly. Should be avoided in favour of
	/// wrappers in this class.
	gdtu::Assembly& nonConstAssembly() { return m_asm; }

	/// @arg _sourceCodes is the map of input files to source code strings
	/// @arg _inJsonFormat shows whether the out should be in Json format
	Json::Value streamAssembly(std::ostream& _stream, StringMap const& _sourceCodes = StringMap(), bool _inJsonFormat = false) const
	{
		return m_asm.stream(_stream, "", _sourceCodes, _inJsonFormat);
	}

	gdtu::LinkerObject const& assembledObject() { return m_asm.assemble(); }
	gdtu::LinkerObject const& assembledRuntimeObject(size_t _subIndex) { return m_asm.sub(_subIndex).assemble(); }

	/**
	 * Helper class to pop the visited nodes stack when a scope closes
	 */
	class LocationSetter: public ScopeGuard
	{
	public:
		LocationSetter(CompilerContext& _compilerContext, ASTNode const& _node):
			ScopeGuard([&]{ _compilerContext.popVisitedNodes(); }) { _compilerContext.pushVisitedNodes(&_node); }
	};

private:
	/// @returns the entry label of the given function - searches the inheritance hierarchy
	/// startig from the given point towards the base.
	gdtu::AssemblyItem virtualFunctionEntryLabel(
		FunctionDefinition const& _function,
		std::vector<ContractDefinition const*>::const_iterator _searchStart
	);
	/// @returns an iterator to the contract directly above the given contract.
	std::vector<ContractDefinition const*>::const_iterator superContract(const ContractDefinition &_contract) const;
	/// Updates source location set in the assembly.
	void updateSourceLocation();

	/**
	 * Helper class that manages function labels and ensures that referenced functions are
	 * compiled in a specific order.
	 */
	struct FunctionCompilationQueue
	{
		/// @returns the entry label of the given function and creates it if it does not exist yet.
		/// @param _context compiler context used to create a new tag if needed
		gdtu::AssemblyItem entryLabel(Declaration const& _declaration, CompilerContext& _context);
		/// @returns the entry label of the given function. Might return an AssemblyItem of type
		/// UndefinedItem if it does not exist yet.
		gdtu::AssemblyItem entryLabelIfExists(Declaration const& _declaration) const;

		/// @returns the next function in the queue of functions that are still to be compiled
		/// (i.e. that were referenced during compilation but where we did not yet generate code for).
		/// Returns nullptr if the queue is empty. Does not remove the function from the queue,
		/// that will only be done by startFunction below.
		Declaration const* nextFunctionToCompile() const;
		/// Informs the queue that we are about to compile the given function, i.e. removes
		/// the function from the queue of functions to compile.
		void startFunction(const Declaration &_function);

		/// Labels pointing to the entry points of functions.
		std::map<Declaration const*, gdtu::AssemblyItem> m_entryLabels;
		/// Set of functions for which we did not yet generate code.
		std::set<Declaration const*> m_alreadyCompiledFunctions;
		/// Queue of functions that still need to be compiled (important to be a queue to maintain
		/// determinism even in the presence of a non-deterministic allocator).
		/// Mutable because we will throw out some functions earlier than needed.
		mutable std::queue<Declaration const*> m_functionsToCompile;
	} m_functionCompilationQueue;

	gdtu::Assembly m_asm;
	/// Magic global variables like msg, tx or this, distinguished by type.
	std::set<Declaration const*> m_magicGlobals;
	/// Other already compiled contracts to be used in contract creation calls.
	std::map<ContractDefinition const*, gdtu::Assembly const*> m_compiledContracts;
	/// Storage offsets of state variables
	std::map<Declaration const*, std::pair<u256, unsigned>> m_stateVariables;
	/// Offsets of local variables on the stack (relative to stack base).
	std::map<Declaration const*, unsigned> m_localVariables;
	/// List of current inheritance hierarchy from derived to base.
	std::vector<ContractDefinition const*> m_inheritanceHierarchy;
	/// Stack of current visited AST nodes, used for location attachment
	std::stack<ASTNode const*> m_visitedNodes;
};

}
}
