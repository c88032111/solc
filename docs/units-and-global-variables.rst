**************************************
Units and Globally Available Variables
**************************************

.. index:: wei, finney, szabo, gdtuer

Gdtuer Units
===========

A literal number can take a suffix of ``wei``, ``finney``, ``szabo`` or ``gdtuer`` to convert between the subdenominations of Gdtuer, where Gdtuer currency numbers without a postfix are assumed to be Wei, e.g. ``2 gdtuer == 2000 finney`` evaluates to ``true``.

.. index:: time, seconds, minutes, hours, days, weeks, years

Time Units
==========

Suffixes like ``seconds``, ``minutes``, ``hours``, ``days``, ``weeks`` and
``years`` after literal numbers can be used to convert between units of time where seconds are the base
unit and units are considered naively in the following way:

 * ``1 == 1 seconds``
 * ``1 minutes == 60 seconds``
 * ``1 hours == 60 minutes``
 * ``1 days == 24 hours``
 * ``1 weeks = 7 days``
 * ``1 years = 365 days``

Take care if you perform calendar calculations using these units, because
not every year equals 365 days and not even every day has 24 hours
because of `leap seconds <https://en.wikipedia.org/wiki/Leap_second>`_.
Due to the fact that leap seconds cannot be predicted, an exact calendar
library has to be updated by an external oracle.

These suffixes cannot be applied to variables. If you want to
interpret some input variable in e.g. days, you can do it in the following way::

    function f(uint start, uint daysAfter) {
        if (now >= start + daysAfter * 1 days) { ... }
    }

Special Variables and Functions
===============================

There are special variables and functions which always exist in the global
namespace and are mainly used to provide information about the blockchain.

.. index:: block, coinbase, difficulty, number, block;number, timestamp, block;timestamp, msg, data, gas, sender, value, now, gas price, origin


Block and Transaction Properties
--------------------------------

- ``block.blockhash(uint blockNumber) returns (bytes32)``: hash of the given block - only works for 256 most recent blocks
- ``block.coinbase`` (``address``): current block miner's address
- ``block.difficulty`` (``uint``): current block difficulty
- ``block.gaslimit`` (``uint``): current block gaslimit
- ``block.number`` (``uint``): current block number
- ``block.timestamp`` (``uint``): current block timestamp
- ``msg.data`` (``bytes``): complete calldata
- ``msg.gas`` (``uint``): remaining gas
- ``msg.sender`` (``address``): sender of the message (current call)
- ``msg.sig`` (``bytes4``): first four bytes of the calldata (i.e. function identifier)
- ``msg.value`` (``uint``): number of wei sent with the message
- ``now`` (``uint``): current block timestamp (alias for ``block.timestamp``)
- ``tx.gasprice`` (``uint``): gas price of the transaction
- ``tx.origin`` (``address``): sender of the transaction (full call chain)

.. note::
    The values of all members of ``msg``, including ``msg.sender`` and
    ``msg.value`` can change for every **external** function call.
    This includes calls to library functions.

    If you want to implement access restrictions in library functions using
    ``msg.sender``, you have to manually supply the value of
    ``msg.sender`` as an argument.

.. note::
    The block hashes are not available for all blocks for scalability reasons.
    You can only access the hashes of the most recent 256 blocks, all other
    values will be zero.

.. index:: sha3, ripemd160, sha256, ecrecover, addmod, mulmod, cryptography, this, super, selfdestruct, balance, send

Mathematical and Cryptographic Functions
----------------------------------------

``addmod(uint x, uint y, uint k) returns (uint)``:
    compute ``(x + y) % k`` where the addition is performed with arbitrary precision and does not wrap around at ``2**256``.
``mulmod(uint x, uint y, uint k) returns (uint)``:
    compute ``(x * y) % k`` where the multiplication is performed with arbitrary precision and does not wrap around at ``2**256``.
``sha3(...) returns (bytes32)``:
    compute the gdtucoin-SHA-3 (KECCAK-256) hash of the (tightly packed) arguments
``sha256(...) returns (bytes32)``:
    compute the SHA-256 hash of the (tightly packed) arguments
``ripemd160(...) returns (bytes20)``:
    compute RIPEMD-160 hash of the (tightly packed) arguments
``ecrecover(bytes32 hash, uint8 v, bytes32 r, bytes32 s) returns (address)``:
    recover the address associated with the public key from elliptic curve signature or return zero on error

In the above, "tightly packed" means that the arguments are concatenated without padding.
This means that the following are all identical::

    sha3("ab", "c")
    sha3("abc")
    sha3(0x616263)
    sha3(6382179)
    sha3(97, 98, 99)

If padding is needed, explicit type conversions can be used: ``sha3("\x00\x12")`` is the
same as ``sha3(uint16(0x12))``.

Note that constants will be packed using the minimum number of bytes required to store them.
This means that, for example, ``sha3(0) == sha3(uint8(0))`` and
``sha3(0x12345678) == sha3(uint32(0x12345678))``.

It might be that you run into Out-of-Gas for ``sha256``, ``ripemd160`` or ``ecrecover`` on a *private blockchain*. The reason for this is that those are implemented as so-called precompiled contracts and these contracts only really exist after they received the first message (although their contract code is hardcoded). Messages to non-existing contracts are more expensive and thus the execution runs into an Out-of-Gas error. A workaround for this problem is to first send e.g. 1 Wei to each of the contracts before you use them in your actual contracts. This is not an issue on the official or test net.

.. _address_related:

Address Related
---------------

``<address>.balance`` (``uint256``):
    balance of the :ref:`address` in Wei
``<address>.send(uint256 amount) returns (bool)``:
    send given amount of Wei to :ref:`address`, returns ``false`` on failure

For more information, see the section on :ref:`address`.

.. warning::
    There are some dangers in using ``send``: The transfer fails if the call stack depth is at 1024
    (this can always be forced by the caller) and it also fails if the recipient runs out of gas. So in order
    to make safe Gdtuer transfers, always check the return value of ``send`` or even better:
    Use a pattern where the recipient withdraws the money.

.. index:: this, selfdestruct

Contract Related
----------------

``this`` (current contract's type):
    the current contract, explicitly convertible to :ref:`address`

``selfdestruct(address recipient)``:
    destroy the current contract, sending its funds to the given :ref:`address`

Furthermore, all functions of the current contract are callable directly including the current function.

