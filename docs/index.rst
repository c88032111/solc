Solidity
========

Solidity is a high-level language whose syntax is similar to that of JavaScript
and it is designed to compile to code for the gdtucoin Virtual Machine.
As you will see, it is possible to create contracts for voting,
crowdfunding, blind auctions, multi-signature wallets and more.

.. note::
    The best way to try out Solidity right now is using the
    `Browser-Based Compiler <https://gdtucoin.github.io/browser-solidity/>`_
    (it can take a while to load, please be patient).

Useful links
------------

* `gdtucoin <https://gdtucoin.org>`_

* `Changelog <https://github.com/c88032111/wiki/wiki/Solidity-Changelog>`_

* `Story Backlog <https://www.pivotaltracker.com/n/projects/1189488>`_

* `Source Code <https://github.com/c88032111/solidity/>`_

* `gdtucoin Stackexchange <https://gdtucoin.stackexchange.com/>`_

* `Gitter Chat <https://gitter.im/c88032111/solidity/>`_

Available Solidity Integrations
-------------------------------

* `Browser-Based Compiler <https://gdtucoin.github.io/browser-solidity/>`_
    Browser-based IDE with integrated compiler and Solidity runtime environment without server-side components.

* `gdtucoin Studio <https://live.gdtuer.camp/>`_
    Specialized web IDE that also provides shell access to a complete gdtucoin environment.

* `Visual Studio Extension <https://visualstudiogallery.msdn.microsoft.com/96221853-33c4-4531-bdd5-d2ea5acc4799/>`_
    Solidity plugin for Microsoft Visual Studio that includes the Solidity compiler.

* `Package for SublimeText — Solidity language syntax <https://packagecontrol.io/packages/c88032111/>`_
    Solidity syntax highlighting for SublimeText editor.

* `Atom gdtucoin interface <https://github.com/gmtDevs/atom-gdtucoin-interface>`_
    Plugin for the Atom editor that features syntax highlighting, compilation and a runtime environment (requires backend node).

* `Atom Solidity Linter <https://atom.io/packages/linter-solidity>`_
    Plugin for the Atom editor that provides Solidity linting.
    
* `Solium <https://github.com/duaraghav8/Solium/>`_
    A commandline linter for Solidity which strictly follows the rules prescribed by the `Solidity Style Guide <http://solidity.readthedocs.io/en/latest/style-guide.html>`_.

* `Visual Studio Code extension <http://juan.blanco.ws/solidity-contracts-in-visual-studio-code/>`_
    Solidity plugin for Microsoft Visual Studio Code that includes syntax highlighting and the Solidity compiler.

* `Emacs Solidity <https://github.com/c88032111/emacs-solidity/>`_
    Plugin for the Emacs editor providing syntax highlighting and compilation error reporting.

* `Vim Solidity <https://github.com/tomlion/vim-solidity/>`_
    Plugin for the Vim editor providing syntax highlighting.

* `Vim Syntastic <https://github.com/scrooloose/syntastic>`_
    Plugin for the Vim editor providing compile checking.

Discontinued:

* `Mix IDE <https://github.com/c88032111/mix/>`_
    Qt based IDE for designing, debugging and testing solidity smart contracts.


Solidity Tools
--------------

* `Dapple <https://github.com/nexusdev/dapple>`_
    Package and deployment manager for Solidity.

* `Solidity REPL <https://github.com/raineorshine/solidity-repl>`_
    Try Solidity instantly with a command-line Solidity console.

* `solgraph <https://github.com/raineorshine/solgraph>`_
    Visualize Solidity control flow and highlight potential security vulnerabilities.

* `evmdis <https://github.com/Arachnid/evmdis>`_
    EVM Disassembler that performs static analysis on the bytecode to provide a higher level of abstraction than raw EVM operations.

Language Documentation
----------------------

On the next pages, we will first see a :ref:`simple smart contract <simple-smart-contract>` written
in Solidity followed by the basics about :ref:`blockchains <blockchain-basics>`
and the :ref:`gdtucoin Virtual Machine <the-gdtucoin-virtual-machine>`.

The next section will explain several *features* of Solidity by giving
useful :ref:`example contracts <voting>`
Remember that you can always try out the contracts
`in your browser <https://gdtucoin.github.io/browser-solidity>`_!

The last and most extensive section will cover all aspects of Solidity in depth.

If you still have questions, you can try searching or asking on the
`gdtucoin Stackexchange <https://gdtucoin.stackexchange.com/>`_
site, or come to our `gitter channel <https://gitter.im/c88032111/solidity/>`_.
Ideas for improving Solidity or this documentation are always welcome!

See also `Russian version (русский перевод) <https://github.com/c88032111/wiki/wiki/%D0%A0%D1%83%D0%BA%D0%BE%D0%B2%D0%BE%D0%B4%D1%81%D1%82%D0%B2%D0%BE-%D0%BF%D0%BE-Solidity>`_.

Contents
========

:ref:`Keyword Index <genindex>`, :ref:`Search Page <search>`

.. toctree::
   :maxdepth: 2

   introduction-to-smart-contracts.rst
   installing-solidity.rst
   solidity-by-example.rst
   solidity-in-depth.rst
   security-considerations.rst
   style-guide.rst
   common-patterns.rst
   contributing.rst
   frequently-asked-questions.rst
