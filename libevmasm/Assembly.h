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
/** @file Assembly.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <iostream>
#include <sstream>
#include <libdevcore/Common.h>
#include <libdevcore/Assertions.h>
#include <libdevcore/SHA3.h>
#include <libevmasm/Instruction.h>
#include <libevmasm/SourceLocation.h>
#include <libevmasm/AssemblyItem.h>
#include <libevmasm/LinkerObject.h>
#include "Exceptions.h"
#include <json/json.h>

namespace dev
{
namespace gdtu
{

class Assembly
{
public:
	Assembly() {}

	AssemblyItem newTag() { return AssemblyItem(Tag, m_usedTags++); }
	AssemblyItem newPushTag() { return AssemblyItem(PushTag, m_usedTags++); }
	AssemblyItem newData(bytes const& _data) { h256 h(sha3(asString(_data))); m_data[h] = _data; return AssemblyItem(PushData, h); }
	AssemblyItem newSub(Assembly const& _sub) { m_subs.push_back(_sub); return AssemblyItem(PushSub, m_subs.size() - 1); }
	Assembly const& sub(size_t _sub) const { return m_subs.at(_sub); }
	Assembly& sub(size_t _sub) { return m_subs.at(_sub); }
	AssemblyItem newPushString(std::string const& _data) { h256 h(sha3(_data)); m_strings[h] = _data; return AssemblyItem(PushString, h); }
	AssemblyItem newPushSubSize(u256 const& _subId) { return AssemblyItem(PushSubSize, _subId); }
	AssemblyItem newPushLibraryAddress(std::string const& _identifier);

	AssemblyItem append() { return append(newTag()); }
	void append(Assembly const& _a);
	void append(Assembly const& _a, int _deposit);
	AssemblyItem const& append(AssemblyItem const& _i);
	AssemblyItem const& append(std::string const& _data) { return append(newPushString(_data)); }
	AssemblyItem const& append(bytes const& _data) { return append(newData(_data)); }
	AssemblyItem appendSubSize(Assembly const& _a) { auto ret = newSub(_a); append(newPushSubSize(ret.data())); return ret; }
	/// Pushes the final size of the current assembly itself. Use this when the code is modified
	/// after compilation and CODESIZE is not an option.
	void appendProgramSize() { append(AssemblyItem(PushProgramSize)); }
	void appendLibraryAddress(std::string const& _identifier) { append(newPushLibraryAddress(_identifier)); }

	AssemblyItem appendJump() { auto ret = append(newPushTag()); append(solidity::Instruction::JUMP); return ret; }
	AssemblyItem appendJumpI() { auto ret = append(newPushTag()); append(solidity::Instruction::JUMPI); return ret; }
	AssemblyItem appendJump(AssemblyItem const& _tag) { auto ret = append(_tag.pushTag()); append(solidity::Instruction::JUMP); return ret; }
	AssemblyItem appendJumpI(AssemblyItem const& _tag) { auto ret = append(_tag.pushTag()); append(solidity::Instruction::JUMPI); return ret; }
	AssemblyItem errorTag() { return AssemblyItem(PushTag, 0); }

	template <class T> Assembly& operator<<(T const& _d) { append(_d); return *this; }
	AssemblyItems const& items() const { return m_items; }
	AssemblyItem const& back() const { return m_items.back(); }
	std::string backString() const { return m_items.size() && m_items.back().type() == PushString ? m_strings.at((h256)m_items.back().data()) : std::string(); }

	void onePath() { if (asserts(!m_totalDeposit && !m_baseDeposit)) BOOST_THROW_EXCEPTION(InvalidDeposit()); m_baseDeposit = m_deposit; m_totalDeposit = INT_MAX; }
	void otherPath() { donePath(); m_totalDeposit = m_deposit; m_deposit = m_baseDeposit; }
	void donePaths() { donePath(); m_totalDeposit = m_baseDeposit = 0; }
	void ignored() { m_baseDeposit = m_deposit; }
	void endIgnored() { m_deposit = m_baseDeposit; m_baseDeposit = 0; }

	void popTo(int _deposit) { while (m_deposit > _deposit) append(solidity::Instruction::POP); }

	void injectStart(AssemblyItem const& _i);
	std::string out() const;
	int deposit() const { return m_deposit; }
	void adjustDeposit(int _adjustment) { m_deposit += _adjustment; if (asserts(m_deposit >= 0)) BOOST_THROW_EXCEPTION(InvalidDeposit()); }
	void setDeposit(int _deposit) { m_deposit = _deposit; if (asserts(m_deposit >= 0)) BOOST_THROW_EXCEPTION(InvalidDeposit()); }

	/// Changes the source location used for each appended item.
	void setSourceLocation(SourceLocation const& _location) { m_currentSourceLocation = _location; }

	/// Assembles the assembly into bytecode. The assembly should not be modified after this call.
	LinkerObject const& assemble() const;
	bytes const& data(h256 const& _i) const { return m_data.at(_i); }

	/// Modify (if @a _enable is set) and return the current assembly such that creation and
	/// execution gas usage is optimised. @a _isCreation should be true for the top-level assembly.
	/// @a _runs specifes an estimate on how often each opcode in this assembly will be executed,
	/// i.e. use a small value to optimise for size and a large value to optimise for runtime.
	Assembly& optimise(bool _enable, bool _isCreation = true, size_t _runs = 200);
	Json::Value stream(
		std::ostream& _out,
		std::string const& _prefix = "",
		const StringMap &_sourceCodes = StringMap(),
		bool _inJsonFormat = false
	) const;

protected:
	std::string locationFromSources(StringMap const& _sourceCodes, SourceLocation const& _location) const;
	void donePath() { if (m_totalDeposit != INT_MAX && m_totalDeposit != m_deposit) BOOST_THROW_EXCEPTION(InvalidDeposit()); }
	unsigned bytesRequired() const;

private:
	Json::Value streamAsmJson(std::ostream& _out, StringMap const& _sourceCodes) const;
	std::ostream& streamAsm(std::ostream& _out, std::string const& _prefix, StringMap const& _sourceCodes) const;
	Json::Value createJsonValue(std::string _name, int _begin, int _end, std::string _value = std::string(), std::string _jumpType = std::string()) const;

protected:
	// 0 is reserved for exception
	unsigned m_usedTags = 1;
	AssemblyItems m_items;
	std::map<h256, bytes> m_data;
	std::vector<Assembly> m_subs;
	std::map<h256, std::string> m_strings;
	std::map<h256, std::string> m_libraries; ///< Identifiers of libraries to be linked.

	mutable LinkerObject m_assembledObject;

	int m_deposit = 0;
	int m_baseDeposit = 0;
	int m_totalDeposit = 0;

	SourceLocation m_currentSourceLocation;
};

inline std::ostream& operator<<(std::ostream& _out, Assembly const& _a)
{
	_a.stream(_out);
	return _out;
}

}
}
