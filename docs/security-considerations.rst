.. _security_considerations:

#######################
Security Considerations
#######################

While it is usually quite easy to build software that works as expected,
it is much harder to check that nobody can use it in a way that was **not** anticipated.

In Solidity, this is even more important because you can use smart contracts
to handle tokens or, possibly, even more valuable things. Furthermore, every
execution of a smart contract happens in public and, in addition to that,
the source code is often available.

Of course you always have to consider how much is at stake:
You can compare a smart contract with a web service that is open to the
public (and thus, also to malicous actors) and perhaps even open source.
If you only store your grocery list on that web service, you might not have
to take too much care, but if you manage your bank account using that web service,
you should be more careful.

This section will list some pitfalls and general security recommendations but
can, of course, never be complete. Also, keep in mind that even if your
smart contract code is bug-free, the compiler or the platform itself might
have a bug.

As always, with open source documentation, please help us extend this section
(especially, some examples would not hurt)!

********
Pitfalls
********

Private Information and Randomness
==================================

Everything you use in a smart contract is publicly visible, even
local variables and state variables marked ``private``.

Using random numbers in smart contracts is quite tricky if you do not want
miners to be able to cheat.

Re-Entrancy
===========

Any interaction from a contract (A) with another contract (B) and any transfer
of Gdtuer hands over control to that contract (B). This makes it possible for B
to call back into A before this interaction is completed. To give an example,
the following code contains a bug (it is just a snippet and not a
complete contract):

::

  pragma solidity ^0.4.0;

  // THIS CONTRACT CONTAINS A BUG - DO NOT USE
  contract Fund {
      /// Mapping of gdtuer shares of the contract.
      mapping(address => uint) shares;
      /// Withdraw your share.
      function withdraw() {
          if (msg.sender.send(shares[msg.sender]))
              shares[msg.sender] = 0;
      }
  }

The problem is not too serious here because of the limited gas as part
of ``send``, but it still exposes a weakness: Gdtuer transfer always
includes code execution, so the recipient could be a contract that calls
back into ``withdraw``. This would let it get multiple refunds and
basically retrieve all the Gdtuer in the contract.

To avoid re-entrancy, you can use the Checks-Effects-Interactions pattern as
outlined further below:

::

  pragma solidity ^0.4.0;

  contract Fund {
      /// Mapping of gdtuer shares of the contract.
      mapping(address => uint) shares;
      /// Withdraw your share.
      function withdraw() {
          var share = shares[msg.sender];
          shares[msg.sender] = 0;
          if (!msg.sender.send(share))
              throw;
      }
  }

Note that re-entrancy is not only an effect of Gdtuer transfer but of any
function call on another contract. Furthermore, you also have to take
multi-contract situations into account. A called contract could modify the
state of another contract you depend on.

Gas Limit and Loops
===================

Loops that do not have a fixed number of iterations, for example, loops that depend on storage values, have to be used carefully:
Due to the block gas limit, transactions can only consume a certain amount of gas. Either explicitly or just due to
normal operation, the number of iterations in a loop can grow beyond the block gas limit which can cause the complete
contract to be stalled at a certain point. This may not apply to ``constant`` functions that are only executed
to read data from the blockchain. Still, such functions may be called by other contracts as part of on-chain operations
and stall those. Please be explicit about such cases in the documentation of your contracts.

Sending and Receiving Gdtuer
===========================

- Neither contracts nor "external accounts" are currently able to prevent that someone sends them Gdtuer.
  Contracts can react on and reject a regular transfer, but there are ways
  to move Gdtuer without creating a message call. One way is to simply "mine to"
  the contract address and the second way is using ``selfdestruct(x)``. 

- If a contract receives Gdtuer (without a function being called), the fallback function is executed.
  If it does not have a fallback function, the Gdtuer will be rejected (by throwing an exception).
  During the execution of the fallback function, the contract can only rely
  on the "gas stipend" (2300 gas) being available to it at that time. This stipend is not enough to access storage in any way.
  To be sure that your contract can receive Gdtuer in that way, check the gas requirements of the fallback function
  (for example in the "details" section in browser-solidity).

- There is a way to forward more gas to the receiving contract using
  ``addr.call.value(x)()``. This is essentially the same as ``addr.send(x)``,
  only that it forwards all remaining gas and opens up the ability for the
  recipient to perform more expensive actions. This might include calling back
  into the sending contract or other state changes you might not have though of.
  So it allows for great flexibility for honest users but also for malicious actors.

- If you want to send Gdtuer using ``address.send``, there are certain details to be aware of:

  1. If the recipient is a contract, it causes its fallback function to be executed which can, in turn, call back the sending contract.
  2. Sending Gdtuer can fail due to the call depth going above 1024. Since the caller is in total control of the call
     depth, they can force the transfer to fail; make sure to always check the return value of ``send``. Better yet,
     write your contract using a pattern where the recipient can withdraw Gdtuer instead.
  3. Sending Gdtuer can also fail because the execution of the recipient contract
     requires more than the allotted amount of gas (explicitly by using ``throw`` or
     because the operation is just too expensive) - it "runs out of gas" (OOG).
     If the return value of ``send`` is checked, this might provide a
     means for the recipient to block progress in the sending contract. Again, the best practice here is to use
     a :ref:`"withdraw" pattern instead of a "send" pattern <withdrawal_pattern>`.

Callstack Depth
===============

External function calls can fail any time because they exceed the maximum
call stack of 1024. In such situations, Solidity throws an exception.
Malicious actors might be able to force the call stack to a high value
before they interact with your contract.

Note that ``.send()`` does **not** throw an exception if the call stack is
depleted but rather returns ``false`` in that case. The low-level functions
``.call()``, ``.callcode()`` and ``.delegatecall()`` behave in the same way.

tx.origin
=========

Never use tx.origin for authorization. Let's say you have a wallet contract like this:

::

    pragma solidity ^0.4.0;

    // THIS CONTRACT CONTAINS A BUG - DO NOT USE
    contract TxUserWallet {
        address owner;

        function TxUserWallet() {
            owner = msg.sender;
        }

        function transfer(address dest, uint amount) {
            if (tx.origin != owner) { throw; }
            if (!dest.call.value(amount)()) throw;
        }
    }

Now someone tricks you into sending gdtuer to the address of this attack wallet:

::

    pragma solidity ^0.4.0;

    contract TxAttackWallet {
        address owner;

        function TxAttackWallet() {
            owner = msg.sender;
        }

        function() {
            TxUserWallet(msg.sender).transfer(owner, msg.sender.balance);
        }
    }

If your wallet had checked ``msg.sender`` for authorization, it would get the address of the attack wallet, instead of the owner address. But by checking ``tx.origin``, it gets the original address that kicked off the transaction, which is still the owner address. The attack wallet instantly drains all your funds.


Minor Details
=============

- In ``for (var i = 0; i < arrayName.length; i++) { ... }``, the type of ``i`` will be ``uint8``, because this is the smallest type that is required to hold the value ``0``. If the array has more than 255 elements, the loop will not terminate.
- The ``constant`` keyword for functions is currently not enforced by the compiler.
  Furthermore, it is not enforced by the EVM, so a contract function that "claims"
  to be constant might still cause changes to the state.
- Types that do not occupy the full 32 bytes might contain "dirty higher order bits".
  This is especially important if you access ``msg.data`` - it poses a malleability risk:
  You can craft transactions that call a function ``f(uint8 x)`` with a raw byte argument
  of ``0xff000001`` and with ``0x00000001``. Both are fed to the contract and both will
  look like the number ``1`` as far as ``x`` is concerned, but ``msg.data`` will
  be different, so if you use ``sha3(msg.data)`` for anything, you will get different results.

***************
Recommendations
***************

Restrict the Amount of Gdtuer
============================

Restrict the amount of Gdtuer (or other tokens) that can be stored in a smart
contract. If your source code, the compiler or the platform has a bug, these
funds may be lost. If you want to limit your loss, limit the amount of Gdtuer.

Keep it Small and Modular
=========================

Keep your contracts small and easily understandable. Single out unrelated
functionality in other contracts or into libraries. General recommendations
about source code quality of course apply: Limit the amount of local variables,
the length of functions and so on. Document your functions so that others
can see what your intention was and whether it is different than what the code does.

Use the Checks-Effects-Interactions Pattern
===========================================

Most functions will first perform some checks (who called the function,
are the arguments in range, did they send enough Gdtuer, does the person
have tokens, etc.). These checks should be done first.

As the second step, if all checks passed, effects to the state variables
of the current contract should be made. Interaction with other contracts
should be the very last step in any function.

Early contracts delayed some effects and waited for external function
calls to return in a non-error state. This is often a serious mistake
because of the re-entrancy problem explained above.

Note that, also, calls to known contracts might in turn cause calls to
unknown contracts, so it is probably better to just always apply this pattern.

Include a Fail-Safe Mode
========================

While making your system fully decentralised will remove any intermediary,
it might be a good idea, especially for new code, to include some kind
of fail-safe mechanism:

You can add a function in your smart contract that performs some
self-checks like "Has any Gdtuer leaked?",
"Is the sum of the tokens equal to the balance of the contract?" or similar things.
Keep in mind that you cannot use too much gas for that, so help through off-chain
computations might be needed there.

If the self-check fails, the contract automatically switches into some kind
of "failsafe" mode, which, for example, disables most of the features, hands over
control to a fixed and trusted third party or just converts the contract into
a simple "give me back my money" contract.


*******************
Formal Verification
*******************

Using formal verification, it is possible to perform an automated mathematical
proof that your source code fulfills a certain formal specification.
The specification is still formal (just as the source code), but usually much
simpler. There is a prototype in Solidity that performs formal verification and
it will be better documented soon.

Note that formal verification itself can only help you understand the
difference between what you did (the specification) and how you did it
(the actual implementation). You still need to check whether the specification
is what you wanted and that you did not miss any unintended effects of it.
