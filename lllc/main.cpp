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
/** @file main.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * gdtucoin client.
 */

#include <fstream>
#include <iostream>
#include <clocale>
#include <liblll/Compiler.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonData.h>
#include <libevmasm/Instruction.h>
using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace dev::gdtu;

void help()
{
	cout
		<< "Usage lllc [OPTIONS] <file>" << endl
        << "Options:" << endl
		<< "    -b,--binary  Parse, compile and assemble; output byte code in binary." << endl
		<< "    -x,--hex  Parse, compile and assemble; output byte code in hex." << endl
		<< "    -a,--assembly  Only parse and compile; show assembly." << endl
		<< "    -t,--parse-tree  Only parse; show parse tree." << endl
		<< "    -h,--help  Show this help message and exit." << endl
		<< "    -V,--version  Show the version and exit." << endl;
        exit(0);
}

void version()
{
	cout << "LLLC, the Lovely Little Language Compiler " << endl;
	cout << "  By Gav Wood, (c) 2014." << endl;
	exit(0);
}

/*
The equivalent of setlocale(LC_ALL, "C") is called before any user code is run.
If the user has an invalid environment setting then it is possible for the call
to set locale to fail, so there are only two possible actions, the first is to
throw a runtime exception and cause the program to quit (default behaviour),
or the second is to modify the environment to something sensible (least
surprising behaviour).

The follow code produces the least surprising behaviour. It will use the user
specified default locale if it is valid, and if not then it will modify the
environment the process is running in to use a sensible default. This also means
that users do not need to install language packs for their OS.
*/
void setDefaultOrCLocale()
{
#if __unix__
	if (!std::setlocale(LC_ALL, ""))
	{
		setenv("LC_ALL", "C", 1);
	}
#endif
}

enum Mode { Binary, Hex, Assembly, ParseTree, Disassemble };

int main(int argc, char** argv)
{
	setDefaultOrCLocale();
	unsigned optimise = 1;
	string infile;
	Mode mode = Hex;

	for (int i = 1; i < argc; ++i)
	{
		string arg = argv[i];
		if (arg == "-h" || arg == "--help")
			help();
		else if (arg == "-b" || arg == "--binary")
			mode = Binary;
		else if (arg == "-x" || arg == "--hex")
			mode = Hex;
		else if (arg == "-a" || arg == "--assembly")
			mode = Assembly;
		else if (arg == "-t" || arg == "--parse-tree")
			mode = ParseTree;
		else if ((arg == "-o" || arg == "--optimise") && argc > i + 1)
			optimise = atoi(argv[++i]);
		else if (arg == "-d" || arg == "--disassemble")
			mode = Disassemble;
		else if (arg == "-V" || arg == "--version")
			version();
		else
			infile = argv[i];
	}

	string src;
	if (infile.empty())
	{
		string s;
		while (!cin.eof())
		{
			getline(cin, s);
			src.append(s);
		}
	}
	else
		src = contentsString(infile);

	vector<string> errors;
	if (src.empty())
		errors.push_back("Empty file.");
	else if (mode == Disassemble)
	{
		cout << disassemble(fromHex(src)) << endl;
	}
	else if (mode == Binary || mode == Hex)
	{
		auto bs = compileLLL(src, optimise ? true : false, &errors);
		if (mode == Hex)
			cout << toHex(bs) << endl;
		else if (mode == Binary)
			cout.write((char const*)bs.data(), bs.size());
	}
	else if (mode == ParseTree)
		cout << parseLLL(src) << endl;
	else if (mode == Assembly)
		cout << compileLLLToAsm(src, optimise ? true : false, &errors) << endl;
	for (auto const& i: errors)
		cerr << i << endl;
	if ( errors.size() )
		return 1;
	return 0;
}
