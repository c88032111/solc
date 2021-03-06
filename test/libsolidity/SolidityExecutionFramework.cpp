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
 * Framework for executing Solidity contracts and testing them against C++ implementation.
 */

#include <cstdlib>
#include <boost/test/framework.hpp>
#include <libdevcore/CommonIO.h>
#include <test/libsolidity/SolidityExecutionFramework.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace dev::solidity::test;

namespace // anonymous
{
	h256 const EmptyTrie("0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421");
}

string getIPCSocketPath()
{
	string ipcPath = dev::test::Options::get().ipcPath;
	if (ipcPath.empty())
		BOOST_FAIL("ERROR: ipcPath not set! (use --ipcpath <path> or the environment variable GDTU_TEST_IPC)");

	return ipcPath;
}

ExecutionFramework::ExecutionFramework() :
	m_rpc(RPCSession::instance(getIPCSocketPath())),
	m_sender(m_rpc.account(0))
{
	m_rpc.test_rewindToBlock(0);
}

void ExecutionFramework::sendMessage(bytes const& _data, bool _isCreation, u256 const& _value)
{
	RPCSession::TransactionData d;
	d.data = "0x" + toHex(_data);
	d.from = "0x" + toString(m_sender);
	d.gas = toHex(m_gas, HexPrefix::Add);
	d.gasPrice = toHex(m_gasPrice, HexPrefix::Add);
	d.value = toHex(_value, HexPrefix::Add);
	if (!_isCreation)
	{
		d.to = dev::toString(m_contractAddress);
		BOOST_REQUIRE(m_rpc.gdtu_getCode(d.to, "latest").size() > 2);
		// Use gdtu_call to get the output
		m_output = fromHex(m_rpc.gdtu_call(d, "latest"), WhenError::Throw);
	}

	string txHash = m_rpc.gdtu_sendTransaction(d);
	m_rpc.test_mineBlocks(1);
	RPCSession::TransactionReceipt receipt(m_rpc.gdtu_getTransactionReceipt(txHash));

	if (_isCreation)
	{
		m_contractAddress = Address(receipt.contractAddress);
		BOOST_REQUIRE(m_contractAddress);
		string code = m_rpc.gdtu_getCode(receipt.contractAddress, "latest");
		m_output = fromHex(code, WhenError::Throw);
	}

	m_gasUsed = u256(receipt.gasUsed);
	m_logs.clear();
	for (auto const& log: receipt.logEntries)
	{
		LogEntry entry;
		entry.address = Address(log.address);
		for (auto const& topic: log.topics)
			entry.topics.push_back(h256(topic));
		entry.data = fromHex(log.data, WhenError::Throw);
		m_logs.push_back(entry);
	}
}

void ExecutionFramework::sendGdtuer(Address const& _to, u256 const& _value)
{
	RPCSession::TransactionData d;
	d.data = "0x";
	d.from = "0x" + toString(m_sender);
	d.gas = toHex(m_gas, HexPrefix::Add);
	d.gasPrice = toHex(m_gasPrice, HexPrefix::Add);
	d.value = toHex(_value, HexPrefix::Add);
	d.to = dev::toString(_to);

	string txHash = m_rpc.gdtu_sendTransaction(d);
	m_rpc.test_mineBlocks(1);
}

size_t ExecutionFramework::currentTimestamp()
{
	auto latestBlock = m_rpc.rpcCall("gdtu_getBlockByNumber", {"\"latest\"", "false"});
	return size_t(u256(latestBlock.get("timestamp", "invalid").asString()));
}

Address ExecutionFramework::account(size_t _i)
{
	return Address(m_rpc.accountCreateIfNotExists(_i));
}

bool ExecutionFramework::addressHasCode(Address const& _addr)
{
	string code = m_rpc.gdtu_getCode(toString(_addr), "latest");
	return !code.empty() && code != "0x";
}

u256 ExecutionFramework::balanceAt(Address const& _addr)
{
	return u256(m_rpc.gdtu_getBalance(toString(_addr), "latest"));
}

bool ExecutionFramework::storageEmpty(Address const& _addr)
{
	h256 root(m_rpc.gdtu_getStorageRoot(toString(_addr), "latest"));
	BOOST_CHECK(root);
	return root == EmptyTrie;
}
