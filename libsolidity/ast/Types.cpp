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
 * Solidity data types
 */

#include <libsolidity/ast/Types.h>
#include <limits>
#include <boost/range/adaptor/reversed.hpp>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/UTF8.h>
#include <libsolidity/interface/Utils.h>
#include <libsolidity/ast/AST.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

void StorageOffsets::computeOffsets(TypePointers const& _types)
{
	bigint slotOffset = 0;
	unsigned byteOffset = 0;
	map<size_t, pair<u256, unsigned>> offsets;
	for (size_t i = 0; i < _types.size(); ++i)
	{
		TypePointer const& type = _types[i];
		if (!type->canBeStored())
			continue;
		if (byteOffset + type->storageBytes() > 32)
		{
			// would overflow, go to next slot
			++slotOffset;
			byteOffset = 0;
		}
		if (slotOffset >= bigint(1) << 256)
			BOOST_THROW_EXCEPTION(Error(Error::Type::TypeError) << errinfo_comment("Object too large for storage."));
		offsets[i] = make_pair(u256(slotOffset), byteOffset);
		solAssert(type->storageSize() >= 1, "Invalid storage size.");
		if (type->storageSize() == 1 && byteOffset + type->storageBytes() <= 32)
			byteOffset += type->storageBytes();
		else
		{
			slotOffset += type->storageSize();
			byteOffset = 0;
		}
	}
	if (byteOffset > 0)
		++slotOffset;
	if (slotOffset >= bigint(1) << 256)
		BOOST_THROW_EXCEPTION(Error(Error::Type::TypeError) << errinfo_comment("Object too large for storage."));
	m_storageSize = u256(slotOffset);
	swap(m_offsets, offsets);
}

pair<u256, unsigned> const* StorageOffsets::offset(size_t _index) const
{
	if (m_offsets.count(_index))
		return &m_offsets.at(_index);
	else
		return nullptr;
}

MemberList& MemberList::operator=(MemberList&& _other)
{
	assert(&_other != this);

	m_memberTypes = move(_other.m_memberTypes);
	m_storageOffsets = move(_other.m_storageOffsets);
	return *this;
}

void MemberList::combine(MemberList const & _other)
{
	m_memberTypes += _other.m_memberTypes;
}

pair<u256, unsigned> const* MemberList::memberStorageOffset(string const& _name) const
{
	if (!m_storageOffsets)
	{
		TypePointers memberTypes;
		memberTypes.reserve(m_memberTypes.size());
		for (auto const& member: m_memberTypes)
			memberTypes.push_back(member.type);
		m_storageOffsets.reset(new StorageOffsets());
		m_storageOffsets->computeOffsets(memberTypes);
	}
	for (size_t index = 0; index < m_memberTypes.size(); ++index)
		if (m_memberTypes[index].name == _name)
			return m_storageOffsets->offset(index);
	return nullptr;
}

u256 const& MemberList::storageSize() const
{
	// trigger lazy computation
	memberStorageOffset("");
	return m_storageOffsets->storageSize();
}

TypePointer Type::fromElementaryTypeName(ElementaryTypeNameToken const& _type)
{
	solAssert(Token::isElementaryTypeName(_type.token()),
		"Expected an elementary type name but got " + _type.toString()
	);

	Token::Value token = _type.token();
	unsigned m = _type.firstNumber();
	unsigned n = _type.secondNumber();

	switch (token)
	{
	case Token::IntM:
		return make_shared<IntegerType>(m, IntegerType::Modifier::Signed);
	case Token::UIntM:
		return make_shared<IntegerType>(m, IntegerType::Modifier::Unsigned);
	case Token::BytesM:
		return make_shared<FixedBytesType>(m);
	case Token::FixedMxN:
		return make_shared<FixedPointType>(m, n, FixedPointType::Modifier::Signed);
	case Token::UFixedMxN:
		return make_shared<FixedPointType>(m, n, FixedPointType::Modifier::Unsigned);
	case Token::Int:
		return make_shared<IntegerType>(256, IntegerType::Modifier::Signed);
	case Token::UInt:
		return make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
	case Token::Fixed:
		return make_shared<FixedPointType>(128, 128, FixedPointType::Modifier::Signed);
	case Token::UFixed:
		return make_shared<FixedPointType>(128, 128, FixedPointType::Modifier::Unsigned);
	case Token::Byte:
		return make_shared<FixedBytesType>(1);
	case Token::Address:
		return make_shared<IntegerType>(0, IntegerType::Modifier::Address);
	case Token::Bool:
		return make_shared<BoolType>();
	case Token::Bytes:
		return make_shared<ArrayType>(DataLocation::Storage);
	case Token::String:
		return make_shared<ArrayType>(DataLocation::Storage, true);
	//no types found
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment(
			"Unable to convert elementary typename " + _type.toString() + " to type."
		));
	}
}

TypePointer Type::fromElementaryTypeName(string const& _name)
{
	unsigned short firstNum;
	unsigned short secondNum;
	Token::Value token;
	tie(token, firstNum, secondNum) = Token::fromIdentifierOrKeyword(_name);
 	return fromElementaryTypeName(ElementaryTypeNameToken(token, firstNum, secondNum));
}

TypePointer Type::forLiteral(Literal const& _literal)
{
	switch (_literal.token())
	{
	case Token::TrueLiteral:
	case Token::FalseLiteral:
		return make_shared<BoolType>();
	case Token::Number:
	{
		tuple<bool, rational> validLiteral = RationalNumberType::isValidLiteral(_literal);
		if (get<0>(validLiteral) == true)
			return make_shared<RationalNumberType>(get<1>(validLiteral));
		else
			return TypePointer();
	}
	case Token::StringLiteral:
		return make_shared<StringLiteralType>(_literal);
	default:
		return TypePointer();
	}
}

TypePointer Type::commonType(TypePointer const& _a, TypePointer const& _b)
{
	if (_b->isImplicitlyConvertibleTo(*_a))
		return _a;
	else if (_a->isImplicitlyConvertibleTo(*_b))
		return _b;
	else
		return TypePointer();
}

MemberList const& Type::members(ContractDefinition const* _currentScope) const
{
	if (!m_members[_currentScope])
	{
		MemberList::MemberMap members = nativeMembers(_currentScope);
		if (_currentScope)
			members += boundFunctions(*this, *_currentScope);
		m_members[_currentScope] = unique_ptr<MemberList>(new MemberList(move(members)));
	}
	return *m_members[_currentScope];
}

MemberList::MemberMap Type::boundFunctions(Type const& _type, ContractDefinition const& _scope)
{
	// Normalise data location of type.
	TypePointer type = ReferenceType::copyForLocationIfReference(DataLocation::Storage, _type.shared_from_this());
	set<Declaration const*> seenFunctions;
	MemberList::MemberMap members;
	for (ContractDefinition const* contract: _scope.annotation().linearizedBaseContracts)
		for (UsingForDirective const* ufd: contract->usingForDirectives())
		{
			if (ufd->typeName() && *type != *ReferenceType::copyForLocationIfReference(
				DataLocation::Storage,
				ufd->typeName()->annotation().type
			))
				continue;
			auto const& library = dynamic_cast<ContractDefinition const&>(
				*ufd->libraryName().annotation().referencedDeclaration
			);
			for (FunctionDefinition const* function: library.definedFunctions())
			{
				if (!function->isVisibleInDerivedContracts() || seenFunctions.count(function))
					continue;
				seenFunctions.insert(function);
				FunctionType funType(*function, false);
				if (auto fun = funType.asMemberFunction(true, true))
					members.push_back(MemberList::Member(function->name(), fun, function));
			}
		}
	return members;
}

IntegerType::IntegerType(int _bits, IntegerType::Modifier _modifier):
	m_bits(_bits), m_modifier(_modifier)
{
	if (isAddress())
		m_bits = 160;
	solAssert(
		m_bits > 0 && m_bits <= 256 && m_bits % 8 == 0,
		"Invalid bit number for integer type: " + dev::toString(_bits)
	);
}

bool IntegerType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.category() == category())
	{
		IntegerType const& convertTo = dynamic_cast<IntegerType const&>(_convertTo);
		if (convertTo.m_bits < m_bits)
			return false;
		if (isAddress())
			return convertTo.isAddress();
		else if (isSigned())
			return convertTo.isSigned();
		else
			return !convertTo.isSigned() || convertTo.m_bits > m_bits;
	}
	else if (_convertTo.category() == Category::FixedPoint)
	{
		FixedPointType const& convertTo = dynamic_cast<FixedPointType const&>(_convertTo);
		if (convertTo.integerBits() < m_bits || isAddress())
			return false;
		else if (isSigned())
			return convertTo.isSigned();
		else
			return !convertTo.isSigned() || convertTo.integerBits() > m_bits;
	}
	else
		return false;
}

bool IntegerType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	return _convertTo.category() == category() ||
		_convertTo.category() == Category::Contract ||
		_convertTo.category() == Category::Enum ||
		_convertTo.category() == Category::FixedBytes ||
		_convertTo.category() == Category::FixedPoint;
}

TypePointer IntegerType::unaryOperatorResult(Token::Value _operator) const
{
	// "delete" is ok for all integer types
	if (_operator == Token::Delete)
		return make_shared<TupleType>();
	// no further unary operators for addresses
	else if (isAddress())
		return TypePointer();
	// for non-address integers, we allow +, -, ++ and --
	else if (_operator == Token::Add || _operator == Token::Sub ||
			_operator == Token::Inc || _operator == Token::Dec ||
			_operator == Token::BitNot)
		return shared_from_this();
	else
		return TypePointer();
}

bool IntegerType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	IntegerType const& other = dynamic_cast<IntegerType const&>(_other);
	return other.m_bits == m_bits && other.m_modifier == m_modifier;
}

string IntegerType::toString(bool) const
{
	if (isAddress())
		return "address";
	string prefix = isSigned() ? "int" : "uint";
	return prefix + dev::toString(m_bits);
}

TypePointer IntegerType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (
		_other->category() != Category::RationalNumber &&
		_other->category() != Category::FixedPoint &&
		_other->category() != category()
	)
		return TypePointer();
	auto commonType = Type::commonType(shared_from_this(), _other); //might be a integer or fixed point
	if (!commonType)
		return TypePointer();

	// All integer types can be compared
	if (Token::isCompareOp(_operator))
		return commonType;
	if (Token::isBooleanOp(_operator))
		return TypePointer();
	// Nothing else can be done with addresses
	if (auto intType = dynamic_pointer_cast<IntegerType const>(commonType))
	{
		if (intType->isAddress())
			return TypePointer();
	}
	else if (auto fixType = dynamic_pointer_cast<FixedPointType const>(commonType))
		if (Token::Exp == _operator)
			return TypePointer();
	return commonType;
}

MemberList::MemberMap IntegerType::nativeMembers(ContractDefinition const*) const
{
	if (isAddress())
		return {
			{"balance", make_shared<IntegerType >(256)},
			{"call", make_shared<FunctionType>(strings(), strings{"bool"}, FunctionType::Location::Bare, true, false, true)},
			{"callcode", make_shared<FunctionType>(strings(), strings{"bool"}, FunctionType::Location::BareCallCode, true, false, true)},
			{"delegatecall", make_shared<FunctionType>(strings(), strings{"bool"}, FunctionType::Location::BareDelegateCall, true)},
			{"send", make_shared<FunctionType>(strings{"uint"}, strings{"bool"}, FunctionType::Location::Send)}
		};
	else
		return MemberList::MemberMap();
}

FixedPointType::FixedPointType(int _integerBits, int _fractionalBits, FixedPointType::Modifier _modifier):
	m_integerBits(_integerBits), m_fractionalBits(_fractionalBits), m_modifier(_modifier)
{
	solAssert(
		m_integerBits + m_fractionalBits > 0 && 
		m_integerBits + m_fractionalBits <= 256 && 
		m_integerBits % 8 == 0 && 
		m_fractionalBits % 8 == 0,
		"Invalid bit number(s) for fixed type: " + 
		dev::toString(_integerBits) + "x" + dev::toString(_fractionalBits)
	);
}

bool FixedPointType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.category() == category())
	{
		FixedPointType const& convertTo = dynamic_cast<FixedPointType const&>(_convertTo);
		if (convertTo.m_integerBits < m_integerBits || convertTo.m_fractionalBits < m_fractionalBits)
			return false;
		else if (isSigned())
			return convertTo.isSigned();
		else
			return !convertTo.isSigned() || (convertTo.m_integerBits > m_integerBits);
	}
	return false;
}

bool FixedPointType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	return _convertTo.category() == category() ||
		_convertTo.category() == Category::Integer ||
		_convertTo.category() == Category::FixedBytes;
}

TypePointer FixedPointType::unaryOperatorResult(Token::Value _operator) const
{
	// "delete" is ok for all fixed types
	if (_operator == Token::Delete)
		return make_shared<TupleType>();
	// for fixed, we allow +, -, ++ and --
	else if (
		_operator == Token::Add || 
		_operator == Token::Sub ||
		_operator == Token::Inc || 
		_operator == Token::Dec
	)
		return shared_from_this();
	else
		return TypePointer();
}

bool FixedPointType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	FixedPointType const& other = dynamic_cast<FixedPointType const&>(_other);
	return other.m_integerBits == m_integerBits && other.m_fractionalBits == m_fractionalBits && other.m_modifier == m_modifier;
}

string FixedPointType::toString(bool) const
{
	string prefix = isSigned() ? "fixed" : "ufixed";
	return prefix + dev::toString(m_integerBits) + "x" + dev::toString(m_fractionalBits);
}

TypePointer FixedPointType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (
		_other->category() != Category::RationalNumber &&
		_other->category() != category() &&
		_other->category() != Category::Integer
	)
		return TypePointer();
	auto commonType = Type::commonType(shared_from_this(), _other); //might be fixed point or integer

	if (!commonType)
		return TypePointer();

	// All fixed types can be compared
	if (Token::isCompareOp(_operator))
		return commonType;
	if (Token::isBitOp(_operator) || Token::isBooleanOp(_operator))
		return TypePointer();
	if (auto fixType = dynamic_pointer_cast<FixedPointType const>(commonType))
	{
		if (Token::Exp == _operator)
			return TypePointer();
	}
	else if (auto intType = dynamic_pointer_cast<IntegerType const>(commonType))
		if (intType->isAddress())
			return TypePointer();
	return commonType;
}

tuple<bool, rational> RationalNumberType::isValidLiteral(Literal const& _literal)
{
	rational x;
	try
	{
		rational numerator;
		rational denominator(1);
		
		auto radixPoint = find(_literal.value().begin(), _literal.value().end(), '.');
		if (radixPoint != _literal.value().end())
		{
			if (
				!all_of(radixPoint + 1, _literal.value().end(), ::isdigit) || 
				!all_of(_literal.value().begin(), radixPoint, ::isdigit) 
			)
				throw;
			//Only decimal notation allowed here, leading zeros would switch to octal.
			auto fractionalBegin = find_if_not(
				radixPoint + 1, 
				_literal.value().end(), 
				[](char const& a) { return a == '0'; }
			);

			denominator = bigint(string(fractionalBegin, _literal.value().end()));
			denominator /= boost::multiprecision::pow(
				bigint(10), 
				distance(radixPoint + 1, _literal.value().end())
			);
			numerator = bigint(string(_literal.value().begin(), radixPoint));
			x = numerator + denominator;
		}
		else
			x = bigint(_literal.value());
	}
	catch (...)
	{
		return make_tuple(false, rational(0));
	}
	switch (_literal.subDenomination())
	{
		case Literal::SubDenomination::None:
		case Literal::SubDenomination::Wei:
		case Literal::SubDenomination::Second:
			break;
		case Literal::SubDenomination::Szabo:
			x *= bigint("1000000000000");
			break;
		case Literal::SubDenomination::Finney:
			x *= bigint("1000000000000000");
			break;
		case Literal::SubDenomination::Gdtuer:
			x *= bigint("1000000000000000000");
			break;
		case Literal::SubDenomination::Minute:
			x *= bigint("60");
			break;
		case Literal::SubDenomination::Hour:
			x *= bigint("3600");
			break;
		case Literal::SubDenomination::Day:
			x *= bigint("86400");
			break;
		case Literal::SubDenomination::Week:
			x *= bigint("604800");
			break;
		case Literal::SubDenomination::Year:
			x *= bigint("31536000");
			break;
	}


	return make_tuple(true, x);
}

bool RationalNumberType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.category() == Category::Integer)
	{
		auto targetType = dynamic_cast<IntegerType const*>(&_convertTo);
		if (m_value == 0)
			return true;
		if (isFractional())
			return false;
		int forSignBit = (targetType->isSigned() ? 1 : 0);
		if (m_value > 0)
		{
			if (m_value.numerator() <= (u256(-1) >> (256 - targetType->numBits() + forSignBit)))
				return true;
		}
		else if (targetType->isSigned() && -m_value.numerator() <= (u256(1) << (targetType->numBits() - forSignBit)))
			return true;
		return false;
	}
	else if (_convertTo.category() == Category::FixedPoint)
	{
		if (auto fixed = fixedPointType())
		{
			// We disallow implicit conversion if we would have to truncate (fixedPointType()
			// can return a type that requires truncation).
			rational value = m_value * (bigint(1) << fixed->fractionalBits());
			return value.denominator() == 1 && fixed->isImplicitlyConvertibleTo(_convertTo);
		}
		return false;
	}
	else if (_convertTo.category() == Category::FixedBytes)
	{
		FixedBytesType const& fixedBytes = dynamic_cast<FixedBytesType const&>(_convertTo);
		if (!isFractional())
			return fixedBytes.numBytes() * 8 >= integerType()->numBits();
		else
			return false;
	}
	return false;
}

bool RationalNumberType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	TypePointer mobType = mobileType();
	return mobType && mobType->isExplicitlyConvertibleTo(_convertTo);
}

TypePointer RationalNumberType::unaryOperatorResult(Token::Value _operator) const
{
	rational value;
	switch (_operator)
	{
	case Token::BitNot:
		if (isFractional())
			return TypePointer();
		value = ~m_value.numerator();
		break;
	case Token::Add:
		value = +(m_value);
		break;
	case Token::Sub:
		value = -(m_value);
		break;
	case Token::After:
		return shared_from_this();
	default:
		return TypePointer();
	}
	return make_shared<RationalNumberType>(value);
}

TypePointer RationalNumberType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (_other->category() == Category::Integer || _other->category() == Category::FixedPoint)
	{
		auto mobile = mobileType();
		if (!mobile)
			return TypePointer();
		return mobile->binaryOperatorResult(_operator, _other);
	}
	else if (_other->category() != category())
		return TypePointer();

	RationalNumberType const& other = dynamic_cast<RationalNumberType const&>(*_other);
	if (Token::isCompareOp(_operator))
	{
		// Since we do not have a "BoolConstantType", we have to do the acutal comparison
		// at runtime and convert to mobile typse first. Such a comparison is not a very common
		// use-case and will be optimized away.
		TypePointer thisMobile = mobileType();
		TypePointer otherMobile = other.mobileType();
		if (!thisMobile || !otherMobile)
			return TypePointer();
		return thisMobile->binaryOperatorResult(_operator, otherMobile);
	}
	else
	{
		rational value;
		bool fractional = isFractional() || other.isFractional();
		switch (_operator)
		{
		//bit operations will only be enabled for integers and fixed types that resemble integers
		case Token::BitOr:
			if (fractional)
				return TypePointer();
			value = m_value.numerator() | other.m_value.numerator();
			break;
		case Token::BitXor:
			if (fractional)
				return TypePointer();
			value = m_value.numerator() ^ other.m_value.numerator();
			break;
		case Token::BitAnd:
			if (fractional)
				return TypePointer();
			value = m_value.numerator() & other.m_value.numerator();
			break;
		case Token::Add:
			value = m_value + other.m_value;
			break;
		case Token::Sub:
			value = m_value - other.m_value;
			break;
		case Token::Mul:
			value = m_value * other.m_value;
			break;
		case Token::Div:
			if (other.m_value == 0)
				return TypePointer();
			else
				value = m_value / other.m_value;
			break;
		case Token::Mod:
			if (other.m_value == 0)
				return TypePointer();
			else if (fractional)
			{
				rational tempValue = m_value / other.m_value;
				value = m_value - (tempValue.numerator() / tempValue.denominator()) * other.m_value;
			}
			else
				value = m_value.numerator() % other.m_value.numerator();
			break;	
		case Token::Exp:
		{
			using boost::multiprecision::pow;
			if (other.isFractional())
				return TypePointer();
			else if (abs(other.m_value) > numeric_limits<uint32_t>::max())
				return TypePointer(); // This will need too much memory to represent.
			uint32_t exponent = abs(other.m_value).numerator().convert_to<uint32_t>();
			bigint numerator = pow(m_value.numerator(), exponent);
			bigint denominator = pow(m_value.denominator(), exponent);
			if (other.m_value >= 0)
				value = rational(numerator, denominator);
			else
				// invert
				value = rational(denominator, numerator);
			break;
		}
		default:
			return TypePointer();
		}
		return make_shared<RationalNumberType>(value);
	}
}

bool RationalNumberType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	RationalNumberType const& other = dynamic_cast<RationalNumberType const&>(_other);
	return m_value == other.m_value;
}

string RationalNumberType::toString(bool) const
{
	if (!isFractional())
		return "int_const " + m_value.numerator().str();
	return "rational_const " + m_value.numerator().str() + '/' + m_value.denominator().str();
}

u256 RationalNumberType::literalValue(Literal const*) const
{
	// We ignore the literal and hope that the type was correctly determined to represent
	// its value.

	u256 value;
	bigint shiftedValue; 

	if (!isFractional())
		shiftedValue = m_value.numerator();
	else
	{
		auto fixed = fixedPointType();
		solAssert(!!fixed, "");
		rational shifted = m_value * (bigint(1) << fixed->fractionalBits());
		// truncate
		shiftedValue = shifted.numerator() / shifted.denominator();
	}

	// we ignore the literal and hope that the type was correctly determined
	solAssert(shiftedValue <= u256(-1), "Integer constant too large.");
	solAssert(shiftedValue >= -(bigint(1) << 255), "Number constant too small.");

	if (m_value >= 0)
		value = u256(shiftedValue);
	else
		value = s2u(s256(shiftedValue));
	return value;
}

TypePointer RationalNumberType::mobileType() const
{
	if (!isFractional())
		return integerType();
	else
		return fixedPointType();
}

shared_ptr<IntegerType const> RationalNumberType::integerType() const
{
	solAssert(!isFractional(), "integerType() called for fractional number.");
	bigint value = m_value.numerator();
	bool negative = (value < 0);
	if (negative) // convert to positive number of same bit requirements
		value = ((0 - value) - 1) << 1;
	if (value > u256(-1))
		return shared_ptr<IntegerType const>();
	else
		return make_shared<IntegerType>(
			max(bytesRequired(value), 1u) * 8,
			negative ? IntegerType::Modifier::Signed : IntegerType::Modifier::Unsigned
		);
}

shared_ptr<FixedPointType const> RationalNumberType::fixedPointType() const
{
	bool negative = (m_value < 0);
	unsigned fractionalBits = 0;
	rational value = abs(m_value); // We care about the sign later.
	rational maxValue = negative ? 
		rational(bigint(1) << 255, 1):
		rational((bigint(1) << 256) - 1, 1);

	while (value * 0x100 <= maxValue && value.denominator() != 1 && fractionalBits < 256)
	{
		value *= 0x100;
		fractionalBits += 8;
	}
	
	if (value > maxValue)
		return shared_ptr<FixedPointType const>();
	// u256(v) is the actual value that will be put on the stack
	// From here on, very similar to integerType()
	bigint v = value.numerator() / value.denominator();
	if (negative)
		// modify value to satisfy bit requirements for negative numbers:
		// add one bit for sign and decrement because negative numbers can be larger
		v = (v - 1) << 1;

	if (v > u256(-1))
		return shared_ptr<FixedPointType const>();

	unsigned totalBits = bytesRequired(v) * 8;
	solAssert(totalBits <= 256, "");
	unsigned integerBits = totalBits >= fractionalBits ? totalBits - fractionalBits : 0;
	// Special case: Numbers between -1 and 0 have their sign bit in the fractional part.
	if (negative && abs(m_value) < 1 && totalBits > fractionalBits)
	{
		fractionalBits += 8;
		integerBits = 0;
	}

	if (integerBits > 256 || fractionalBits > 256 || fractionalBits + integerBits > 256)
		return shared_ptr<FixedPointType const>();
	if (integerBits == 0 && fractionalBits == 0)
	{
		integerBits = 0;
		fractionalBits = 8;
	}

	return make_shared<FixedPointType>(
		integerBits, fractionalBits,
		negative ? FixedPointType::Modifier::Signed : FixedPointType::Modifier::Unsigned
	);
}

StringLiteralType::StringLiteralType(Literal const& _literal):
	m_value(_literal.value())
{
}

bool StringLiteralType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (auto fixedBytes = dynamic_cast<FixedBytesType const*>(&_convertTo))
		return size_t(fixedBytes->numBytes()) >= m_value.size();
	else if (auto arrayType = dynamic_cast<ArrayType const*>(&_convertTo))
		return
			arrayType->isByteArray() &&
			!(arrayType->dataStoredIn(DataLocation::Storage) && arrayType->isPointer());
	else
		return false;
}

bool StringLiteralType::operator==(const Type& _other) const
{
	if (_other.category() != category())
		return false;
	return m_value == dynamic_cast<StringLiteralType const&>(_other).m_value;
}

std::string StringLiteralType::toString(bool) const
{
	size_t invalidSequence;

	if (!dev::validate(m_value, invalidSequence))
		return "literal_string (contains invalid UTF-8 sequence at position " + dev::toString(invalidSequence) + ")";

	return "literal_string \"" + m_value + "\"";
}

TypePointer StringLiteralType::mobileType() const
{
	return make_shared<ArrayType>(DataLocation::Memory, true);
}

shared_ptr<FixedBytesType> FixedBytesType::smallestTypeForLiteral(string const& _literal)
{
	if (_literal.length() <= 32)
		return make_shared<FixedBytesType>(_literal.length());
	return shared_ptr<FixedBytesType>();
}

FixedBytesType::FixedBytesType(int _bytes): m_bytes(_bytes)
{
	solAssert(m_bytes >= 0 && m_bytes <= 32,
			  "Invalid byte number for fixed bytes type: " + dev::toString(m_bytes));
}

bool FixedBytesType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (_convertTo.category() != category())
		return false;
	FixedBytesType const& convertTo = dynamic_cast<FixedBytesType const&>(_convertTo);
	return convertTo.m_bytes >= m_bytes;
}

bool FixedBytesType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	return _convertTo.category() == Category::Integer ||
		_convertTo.category() == Category::FixedPoint ||
		_convertTo.category() == Category::Contract ||
		_convertTo.category() == category();
}

TypePointer FixedBytesType::unaryOperatorResult(Token::Value _operator) const
{
	// "delete" and "~" is okay for FixedBytesType
	if (_operator == Token::Delete)
		return make_shared<TupleType>();
	else if (_operator == Token::BitNot)
		return shared_from_this();

	return TypePointer();
}

TypePointer FixedBytesType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	auto commonType = dynamic_pointer_cast<FixedBytesType const>(Type::commonType(shared_from_this(), _other));
	if (!commonType)
		return TypePointer();

	// FixedBytes can be compared and have bitwise operators applied to them
	if (Token::isCompareOp(_operator) || Token::isBitOp(_operator))
		return commonType;

	return TypePointer();
}

MemberList::MemberMap FixedBytesType::nativeMembers(const ContractDefinition*) const
{
	return MemberList::MemberMap{MemberList::Member{"length", make_shared<IntegerType>(8)}};
}

bool FixedBytesType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	FixedBytesType const& other = dynamic_cast<FixedBytesType const&>(_other);
	return other.m_bytes == m_bytes;
}

u256 BoolType::literalValue(Literal const* _literal) const
{
	solAssert(_literal, "");
	if (_literal->token() == Token::TrueLiteral)
		return u256(1);
	else if (_literal->token() == Token::FalseLiteral)
		return u256(0);
	else
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Bool type constructed from non-boolean literal."));
}

TypePointer BoolType::unaryOperatorResult(Token::Value _operator) const
{
	if (_operator == Token::Delete)
		return make_shared<TupleType>();
	return (_operator == Token::Not) ? shared_from_this() : TypePointer();
}

TypePointer BoolType::binaryOperatorResult(Token::Value _operator, TypePointer const& _other) const
{
	if (category() != _other->category())
		return TypePointer();
	if (Token::isCompareOp(_operator) || _operator == Token::And || _operator == Token::Or)
		return _other;
	else
		return TypePointer();
}

bool ContractType::isImplicitlyConvertibleTo(Type const& _convertTo) const
{
	if (*this == _convertTo)
		return true;
	if (_convertTo.category() == Category::Integer)
		return dynamic_cast<IntegerType const&>(_convertTo).isAddress();
	if (_convertTo.category() == Category::Contract)
	{
		auto const& bases = contractDefinition().annotation().linearizedBaseContracts;
		if (m_super && bases.size() <= 1)
			return false;
		return find(m_super ? ++bases.begin() : bases.begin(), bases.end(),
					&dynamic_cast<ContractType const&>(_convertTo).contractDefinition()) != bases.end();
	}
	return false;
}

bool ContractType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	return
		isImplicitlyConvertibleTo(_convertTo) ||
		_convertTo.category() == Category::Integer ||
		_convertTo.category() == Category::Contract;
}

TypePointer ContractType::unaryOperatorResult(Token::Value _operator) const
{
	return _operator == Token::Delete ? make_shared<TupleType>() : TypePointer();
}

TypePointer ReferenceType::unaryOperatorResult(Token::Value _operator) const
{
	if (_operator != Token::Delete)
		return TypePointer();
	// delete can be used on everything except calldata references or storage pointers
	// (storage references are ok)
	switch (location())
	{
	case DataLocation::CallData:
		return TypePointer();
	case DataLocation::Memory:
		return make_shared<TupleType>();
	case DataLocation::Storage:
		return m_isPointer ? TypePointer() : make_shared<TupleType>();
	default:
		solAssert(false, "");
	}
	return TypePointer();
}

TypePointer ReferenceType::copyForLocationIfReference(DataLocation _location, TypePointer const& _type)
{
	if (auto type = dynamic_cast<ReferenceType const*>(_type.get()))
		return type->copyForLocation(_location, false);
	return _type;
}

TypePointer ReferenceType::copyForLocationIfReference(TypePointer const& _type) const
{
	return copyForLocationIfReference(m_location, _type);
}

string ReferenceType::stringForReferencePart() const
{
	switch (m_location)
	{
	case DataLocation::Storage:
		return string("storage ") + (m_isPointer ? "pointer" : "ref");
	case DataLocation::CallData:
		return "calldata";
	case DataLocation::Memory:
		return "memory";
	}
	solAssert(false, "");
	return "";
}

bool ArrayType::isImplicitlyConvertibleTo(const Type& _convertTo) const
{
	if (_convertTo.category() != category())
		return false;
	auto& convertTo = dynamic_cast<ArrayType const&>(_convertTo);
	if (convertTo.isByteArray() != isByteArray() || convertTo.isString() != isString())
		return false;
	// memory/calldata to storage can be converted, but only to a direct storage reference
	if (convertTo.location() == DataLocation::Storage && location() != DataLocation::Storage && convertTo.isPointer())
		return false;
	if (convertTo.location() == DataLocation::CallData && location() != convertTo.location())
		return false;
	if (convertTo.location() == DataLocation::Storage && !convertTo.isPointer())
	{
		// Less restrictive conversion, since we need to copy anyway.
		if (!baseType()->isImplicitlyConvertibleTo(*convertTo.baseType()))
			return false;
		if (convertTo.isDynamicallySized())
			return true;
		return !isDynamicallySized() && convertTo.length() >= length();
	}
	else
	{
		// Conversion to storage pointer or to memory, we de not copy element-for-element here, so
		// require that the base type is the same, not only convertible.
		// This disallows assignment of nested dynamic arrays from storage to memory for now.
		if (
			*copyForLocationIfReference(location(), baseType()) !=
			*copyForLocationIfReference(location(), convertTo.baseType())
		)
			return false;
		if (isDynamicallySized() != convertTo.isDynamicallySized())
			return false;
		// We also require that the size is the same.
		if (!isDynamicallySized() && length() != convertTo.length())
			return false;
		return true;
	}
}

bool ArrayType::isExplicitlyConvertibleTo(const Type& _convertTo) const
{
	if (isImplicitlyConvertibleTo(_convertTo))
		return true;
	// allow conversion bytes <-> string
	if (_convertTo.category() != category())
		return false;
	auto& convertTo = dynamic_cast<ArrayType const&>(_convertTo);
	if (convertTo.location() != location())
		return false;
	if (!isByteArray() || !convertTo.isByteArray())
		return false;
	return true;
}

bool ArrayType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	ArrayType const& other = dynamic_cast<ArrayType const&>(_other);
	if (
		!ReferenceType::operator==(other) ||
		other.isByteArray() != isByteArray() ||
		other.isString() != isString() ||
		other.isDynamicallySized() != isDynamicallySized()
	)
		return false;
	if (*other.baseType() != *baseType())
		return false;
	return isDynamicallySized() || length()  == other.length();
}

unsigned ArrayType::calldataEncodedSize(bool _padded) const
{
	if (isDynamicallySized())
		return 32;
	bigint size = bigint(length()) * (isByteArray() ? 1 : baseType()->calldataEncodedSize(_padded));
	size = ((size + 31) / 32) * 32;
	solAssert(size <= numeric_limits<unsigned>::max(), "Array size does not fit unsigned.");
	return unsigned(size);
}

u256 ArrayType::storageSize() const
{
	if (isDynamicallySized())
		return 1;

	bigint size;
	unsigned baseBytes = baseType()->storageBytes();
	if (baseBytes == 0)
		size = 1;
	else if (baseBytes < 32)
	{
		unsigned itemsPerSlot = 32 / baseBytes;
		size = (bigint(length()) + (itemsPerSlot - 1)) / itemsPerSlot;
	}
	else
		size = bigint(length()) * baseType()->storageSize();
	if (size >= bigint(1) << 256)
		BOOST_THROW_EXCEPTION(Error(Error::Type::TypeError) << errinfo_comment("Array too large for storage."));
	return max<u256>(1, u256(size));
}

unsigned ArrayType::sizeOnStack() const
{
	if (m_location == DataLocation::CallData)
		// offset [length] (stack top)
		return 1 + (isDynamicallySized() ? 1 : 0);
	else
		// storage slot or memory offset
		// byte offset inside storage value is omitted
		return 1;
}

string ArrayType::toString(bool _short) const
{
	string ret;
	if (isString())
		ret = "string";
	else if (isByteArray())
		ret = "bytes";
	else
	{
		ret = baseType()->toString(_short) + "[";
		if (!isDynamicallySized())
			ret += length().str();
		ret += "]";
	}
	if (!_short)
		ret += " " + stringForReferencePart();
	return ret;
}

string ArrayType::canonicalName(bool _addDataLocation) const
{
	string ret;
	if (isString())
		ret = "string";
	else if (isByteArray())
		ret = "bytes";
	else
	{
		ret = baseType()->canonicalName(false) + "[";
		if (!isDynamicallySized())
			ret += length().str();
		ret += "]";
	}
	if (_addDataLocation && location() == DataLocation::Storage)
		ret += " storage";
	return ret;
}

MemberList::MemberMap ArrayType::nativeMembers(ContractDefinition const*) const
{
	MemberList::MemberMap members;
	if (!isString())
	{
		members.push_back({"length", make_shared<IntegerType>(256)});
		if (isDynamicallySized() && location() == DataLocation::Storage)
			members.push_back({"push", make_shared<FunctionType>(
				TypePointers{baseType()},
				TypePointers{make_shared<IntegerType>(256)},
				strings{string()},
				strings{string()},
				isByteArray() ? FunctionType::Location::ByteArrayPush : FunctionType::Location::ArrayPush
			)});
	}
	return members;
}

TypePointer ArrayType::encodingType() const
{
	if (location() == DataLocation::Storage)
		return make_shared<IntegerType>(256);
	else
		return this->copyForLocation(DataLocation::Memory, true);
}

TypePointer ArrayType::decodingType() const
{
	if (location() == DataLocation::Storage)
		return make_shared<IntegerType>(256);
	else
		return shared_from_this();
}

TypePointer ArrayType::interfaceType(bool _inLibrary) const
{
	if (_inLibrary && location() == DataLocation::Storage)
		return shared_from_this();

	if (m_arrayKind != ArrayKind::Ordinary)
		return this->copyForLocation(DataLocation::Memory, true);
	TypePointer baseExt = m_baseType->interfaceType(_inLibrary);
	if (!baseExt)
		return TypePointer();
	if (m_baseType->category() == Category::Array && m_baseType->isDynamicallySized())
		return TypePointer();

	if (isDynamicallySized())
		return make_shared<ArrayType>(DataLocation::Memory, baseExt);
	else
		return make_shared<ArrayType>(DataLocation::Memory, baseExt, m_length);
}

u256 ArrayType::memorySize() const
{
	solAssert(!isDynamicallySized(), "");
	solAssert(m_location == DataLocation::Memory, "");
	bigint size = bigint(m_length) * m_baseType->memoryHeadSize();
	solAssert(size <= numeric_limits<unsigned>::max(), "Array size does not fit u256.");
	return u256(size);
}

TypePointer ArrayType::copyForLocation(DataLocation _location, bool _isPointer) const
{
	auto copy = make_shared<ArrayType>(_location);
	copy->m_isPointer = _isPointer;
	copy->m_arrayKind = m_arrayKind;
	copy->m_baseType = copy->copyForLocationIfReference(m_baseType);
	copy->m_hasDynamicLength = m_hasDynamicLength;
	copy->m_length = m_length;
	return copy;
}

bool ContractType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	ContractType const& other = dynamic_cast<ContractType const&>(_other);
	return other.m_contract == m_contract && other.m_super == m_super;
}

string ContractType::toString(bool) const
{
	return
		string(m_contract.isLibrary() ? "library " : "contract ") +
		string(m_super ? "super " : "") +
		m_contract.name();
}

string ContractType::canonicalName(bool) const
{
	return m_contract.annotation().canonicalName;
}

MemberList::MemberMap ContractType::nativeMembers(ContractDefinition const*) const
{
	// All address members and all interface functions
	MemberList::MemberMap members(IntegerType(120, IntegerType::Modifier::Address).nativeMembers(nullptr));
	if (m_super)
	{
		// add the most derived of all functions which are visible in derived contracts
		for (ContractDefinition const* base: m_contract.annotation().linearizedBaseContracts)
			for (FunctionDefinition const* function: base->definedFunctions())
			{
				if (!function->isVisibleInDerivedContracts())
					continue;
				auto functionType = make_shared<FunctionType>(*function, true);
				bool functionWithEqualArgumentsFound = false;
				for (auto const& member: members)
				{
					if (member.name != function->name())
						continue;
					auto memberType = dynamic_cast<FunctionType const*>(member.type.get());
					solAssert(!!memberType, "Override changes type.");
					if (!memberType->hasEqualArgumentTypes(*functionType))
						continue;
					functionWithEqualArgumentsFound = true;
					break;
				}
				if (!functionWithEqualArgumentsFound)
					members.push_back(MemberList::Member(
						function->name(),
						functionType,
						function
					));
			}
	}
	else if (!m_contract.isLibrary())
	{
		for (auto const& it: m_contract.interfaceFunctions())
			members.push_back(MemberList::Member(
				it.second->declaration().name(),
				it.second->asMemberFunction(m_contract.isLibrary()),
				&it.second->declaration()
			));
	}
	return members;
}

shared_ptr<FunctionType const> const& ContractType::newExpressionType() const
{
	if (!m_constructorType)
		m_constructorType = FunctionType::newExpressionType(m_contract);
	return m_constructorType;
}

vector<tuple<VariableDeclaration const*, u256, unsigned>> ContractType::stateVariables() const
{
	vector<VariableDeclaration const*> variables;
	for (ContractDefinition const* contract: boost::adaptors::reverse(m_contract.annotation().linearizedBaseContracts))
		for (VariableDeclaration const* variable: contract->stateVariables())
			if (!variable->isConstant())
				variables.push_back(variable);
	TypePointers types;
	for (auto variable: variables)
		types.push_back(variable->annotation().type);
	StorageOffsets offsets;
	offsets.computeOffsets(types);

	vector<tuple<VariableDeclaration const*, u256, unsigned>> variablesAndOffsets;
	for (size_t index = 0; index < variables.size(); ++index)
		if (auto const* offset = offsets.offset(index))
			variablesAndOffsets.push_back(make_tuple(variables[index], offset->first, offset->second));
	return variablesAndOffsets;
}

bool StructType::isImplicitlyConvertibleTo(const Type& _convertTo) const
{
	if (_convertTo.category() != category())
		return false;
	auto& convertTo = dynamic_cast<StructType const&>(_convertTo);
	// memory/calldata to storage can be converted, but only to a direct storage reference
	if (convertTo.location() == DataLocation::Storage && location() != DataLocation::Storage && convertTo.isPointer())
		return false;
	if (convertTo.location() == DataLocation::CallData && location() != convertTo.location())
		return false;
	return this->m_struct == convertTo.m_struct;
}

bool StructType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	StructType const& other = dynamic_cast<StructType const&>(_other);
	return ReferenceType::operator==(other) && other.m_struct == m_struct;
}

unsigned StructType::calldataEncodedSize(bool _padded) const
{
	unsigned size = 0;
	for (auto const& member: members(nullptr))
		if (!member.type->canLiveOutsideStorage())
			return 0;
		else
		{
			unsigned memberSize = member.type->calldataEncodedSize(_padded);
			if (memberSize == 0)
				return 0;
			size += memberSize;
		}
	return size;
}

u256 StructType::memorySize() const
{
	u256 size;
	for (auto const& member: members(nullptr))
		if (member.type->canLiveOutsideStorage())
			size += member.type->memoryHeadSize();
	return size;
}

u256 StructType::storageSize() const
{
	return max<u256>(1, members(nullptr).storageSize());
}

string StructType::toString(bool _short) const
{
	string ret = "struct " + m_struct.name();
	if (!_short)
		ret += " " + stringForReferencePart();
	return ret;
}

MemberList::MemberMap StructType::nativeMembers(ContractDefinition const*) const
{
	MemberList::MemberMap members;
	for (ASTPointer<VariableDeclaration> const& variable: m_struct.members())
	{
		TypePointer type = variable->annotation().type;
		// Skip all mapping members if we are not in storage.
		if (location() != DataLocation::Storage && !type->canLiveOutsideStorage())
			continue;
		members.push_back(MemberList::Member(
			variable->name(),
			copyForLocationIfReference(type),
			variable.get())
		);
	}
	return members;
}

TypePointer StructType::interfaceType(bool _inLibrary) const
{
	if (_inLibrary && location() == DataLocation::Storage)
		return shared_from_this();
	else
		return TypePointer();
}

TypePointer StructType::copyForLocation(DataLocation _location, bool _isPointer) const
{
	auto copy = make_shared<StructType>(m_struct, _location);
	copy->m_isPointer = _isPointer;
	return copy;
}

string StructType::canonicalName(bool _addDataLocation) const
{
	string ret = m_struct.annotation().canonicalName;
	if (_addDataLocation && location() == DataLocation::Storage)
		ret += " storage";
	return ret;
}

FunctionTypePointer StructType::constructorType() const
{
	TypePointers paramTypes;
	strings paramNames;
	for (auto const& member: members(nullptr))
	{
		if (!member.type->canLiveOutsideStorage())
			continue;
		paramNames.push_back(member.name);
		paramTypes.push_back(copyForLocationIfReference(DataLocation::Memory, member.type));
	}
	return make_shared<FunctionType>(
		paramTypes,
		TypePointers{copyForLocation(DataLocation::Memory, false)},
		paramNames,
		strings(),
		FunctionType::Location::Internal
	);
}

pair<u256, unsigned> const& StructType::storageOffsetsOfMember(string const& _name) const
{
	auto const* offsets = members(nullptr).memberStorageOffset(_name);
	solAssert(offsets, "Storage offset of non-existing member requested.");
	return *offsets;
}

u256 StructType::memoryOffsetOfMember(string const& _name) const
{
	u256 offset;
	for (auto const& member: members(nullptr))
		if (member.name == _name)
			return offset;
		else
			offset += member.type->memoryHeadSize();
	solAssert(false, "Member not found in struct.");
	return 0;
}

set<string> StructType::membersMissingInMemory() const
{
	set<string> missing;
	for (ASTPointer<VariableDeclaration> const& variable: m_struct.members())
		if (!variable->annotation().type->canLiveOutsideStorage())
			missing.insert(variable->name());
	return missing;
}

TypePointer EnumType::unaryOperatorResult(Token::Value _operator) const
{
	return _operator == Token::Delete ? make_shared<TupleType>() : TypePointer();
}

bool EnumType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	EnumType const& other = dynamic_cast<EnumType const&>(_other);
	return other.m_enum == m_enum;
}

unsigned EnumType::storageBytes() const
{
	size_t elements = m_enum.members().size();
	if (elements <= 1)
		return 1;
	else
		return dev::bytesRequired(elements - 1);
}

string EnumType::toString(bool) const
{
	return string("enum ") + m_enum.name();
}

string EnumType::canonicalName(bool) const
{
	return m_enum.annotation().canonicalName;
}

bool EnumType::isExplicitlyConvertibleTo(Type const& _convertTo) const
{
	return _convertTo.category() == category() || _convertTo.category() == Category::Integer;
}

unsigned EnumType::memberValue(ASTString const& _member) const
{
	unsigned index = 0;
	for (ASTPointer<EnumValue> const& decl: m_enum.members())
	{
		if (decl->name() == _member)
			return index;
		++index;
	}
	BOOST_THROW_EXCEPTION(m_enum.createTypeError("Requested unknown enum value ." + _member));
}

bool TupleType::isImplicitlyConvertibleTo(Type const& _other) const
{
	if (auto tupleType = dynamic_cast<TupleType const*>(&_other))
	{
		TypePointers const& targets = tupleType->components();
		if (targets.empty())
			return components().empty();
		if (components().size() != targets.size() && !targets.front() && !targets.back())
			return false; // (,a,) = (1,2,3,4) - unable to position `a` in the tuple.
		size_t minNumValues = targets.size();
		if (!targets.back() || !targets.front())
			--minNumValues; // wildcards can also match 0 components
		if (components().size() < minNumValues)
			return false;
		if (components().size() > targets.size() && targets.front() && targets.back())
			return false; // larger source and no wildcard
		bool fillRight = !targets.back() || targets.front();
		for (size_t i = 0; i < min(targets.size(), components().size()); ++i)
		{
			auto const& s = components()[fillRight ? i : components().size() - i - 1];
			auto const& t = targets[fillRight ? i : targets.size() - i - 1];
			if (!s && t)
				return false;
			else if (s && t && !s->isImplicitlyConvertibleTo(*t))
				return false;
		}
		return true;
	}
	else
		return false;
}

bool TupleType::operator==(Type const& _other) const
{
	if (auto tupleType = dynamic_cast<TupleType const*>(&_other))
		return components() == tupleType->components();
	else
		return false;
}

string TupleType::toString(bool _short) const
{
	if (components().empty())
		return "tuple()";
	string str = "tuple(";
	for (auto const& t: components())
		str += (t ? t->toString(_short) : "") + ",";
	str.pop_back();
	return str + ")";
}

u256 TupleType::storageSize() const
{
	BOOST_THROW_EXCEPTION(
		InternalCompilerError() <<
		errinfo_comment("Storage size of non-storable tuple type requested.")
	);
}

unsigned TupleType::sizeOnStack() const
{
	unsigned size = 0;
	for (auto const& t: components())
		size += t ? t->sizeOnStack() : 0;
	return size;
}

TypePointer TupleType::mobileType() const
{
	TypePointers mobiles;
	for (auto const& c: components())
		mobiles.push_back(c ? c->mobileType() : TypePointer());
	return make_shared<TupleType>(mobiles);
}

TypePointer TupleType::closestTemporaryType(TypePointer const& _targetType) const
{
	solAssert(!!_targetType, "");
	TypePointers const& targetComponents = dynamic_cast<TupleType const&>(*_targetType).components();
	bool fillRight = !targetComponents.empty() && (!targetComponents.back() || targetComponents.front());
	TypePointers tempComponents(targetComponents.size());
	for (size_t i = 0; i < min(targetComponents.size(), components().size()); ++i)
	{
		size_t si = fillRight ? i : components().size() - i - 1;
		size_t ti = fillRight ? i : targetComponents.size() - i - 1;
		if (components()[si] && targetComponents[ti])
			tempComponents[ti] = components()[si]->closestTemporaryType(targetComponents[ti]);
	}
	return make_shared<TupleType>(tempComponents);
}

FunctionType::FunctionType(FunctionDefinition const& _function, bool _isInternal):
	m_location(_isInternal ? Location::Internal : Location::External),
	m_isConstant(_function.isDeclaredConst()),
	m_isPayable(_function.isPayable()),
	m_declaration(&_function)
{
	TypePointers params;
	vector<string> paramNames;
	TypePointers retParams;
	vector<string> retParamNames;

	params.reserve(_function.parameters().size());
	paramNames.reserve(_function.parameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _function.parameters())
	{
		paramNames.push_back(var->name());
		params.push_back(var->annotation().type);
	}
	retParams.reserve(_function.returnParameters().size());
	retParamNames.reserve(_function.returnParameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _function.returnParameters())
	{
		retParamNames.push_back(var->name());
		retParams.push_back(var->annotation().type);
	}
	swap(params, m_parameterTypes);
	swap(paramNames, m_parameterNames);
	swap(retParams, m_returnParameterTypes);
	swap(retParamNames, m_returnParameterNames);
}

FunctionType::FunctionType(VariableDeclaration const& _varDecl):
	m_location(Location::External), m_isConstant(true), m_declaration(&_varDecl)
{
	TypePointers paramTypes;
	vector<string> paramNames;
	auto returnType = _varDecl.annotation().type;

	while (true)
	{
		if (auto mappingType = dynamic_cast<MappingType const*>(returnType.get()))
		{
			paramTypes.push_back(mappingType->keyType());
			paramNames.push_back("");
			returnType = mappingType->valueType();
		}
		else if (auto arrayType = dynamic_cast<ArrayType const*>(returnType.get()))
		{
			if (arrayType->isByteArray())
				// Return byte arrays as as whole.
				break;
			returnType = arrayType->baseType();
			paramNames.push_back("");
			paramTypes.push_back(make_shared<IntegerType>(256));
		}
		else
			break;
	}

	TypePointers retParams;
	vector<string> retParamNames;
	if (auto structType = dynamic_cast<StructType const*>(returnType.get()))
	{
		for (auto const& member: structType->members(nullptr))
			if (member.type->category() != Category::Mapping)
			{
				if (auto arrayType = dynamic_cast<ArrayType const*>(member.type.get()))
					if (!arrayType->isByteArray())
						continue;
				retParams.push_back(member.type);
				retParamNames.push_back(member.name);
			}
	}
	else
	{
		retParams.push_back(ReferenceType::copyForLocationIfReference(
			DataLocation::Memory,
			returnType
		));
		retParamNames.push_back("");
	}

	swap(paramTypes, m_parameterTypes);
	swap(paramNames, m_parameterNames);
	swap(retParams, m_returnParameterTypes);
	swap(retParamNames, m_returnParameterNames);
}

FunctionType::FunctionType(EventDefinition const& _event):
	m_location(Location::Event), m_isConstant(true), m_declaration(&_event)
{
	TypePointers params;
	vector<string> paramNames;
	params.reserve(_event.parameters().size());
	paramNames.reserve(_event.parameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _event.parameters())
	{
		paramNames.push_back(var->name());
		params.push_back(var->annotation().type);
	}
	swap(params, m_parameterTypes);
	swap(paramNames, m_parameterNames);
}

FunctionTypePointer FunctionType::newExpressionType(ContractDefinition const& _contract)
{
	FunctionDefinition const* constructor = _contract.constructor();
	TypePointers parameters;
	strings parameterNames;
	bool payable = false;

	if (constructor)
	{
		for (ASTPointer<VariableDeclaration> const& var: constructor->parameters())
		{
			parameterNames.push_back(var->name());
			parameters.push_back(var->annotation().type);
		}
		payable = constructor->isPayable();
	}
	return make_shared<FunctionType>(
		parameters,
		TypePointers{make_shared<ContractType>(_contract)},
		parameterNames,
		strings{""},
		Location::Creation,
		false,
		nullptr,
		false,
		payable
	);
}

vector<string> FunctionType::parameterNames() const
{
	if (!bound())
		return m_parameterNames;
	return vector<string>(m_parameterNames.cbegin() + 1, m_parameterNames.cend());
}

TypePointers FunctionType::parameterTypes() const
{
	if (!bound())
		return m_parameterTypes;
	return TypePointers(m_parameterTypes.cbegin() + 1, m_parameterTypes.cend());
}

bool FunctionType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	FunctionType const& other = dynamic_cast<FunctionType const&>(_other);

	if (m_location != other.m_location)
		return false;
	if (m_isConstant != other.isConstant())
		return false;

	if (m_parameterTypes.size() != other.m_parameterTypes.size() ||
			m_returnParameterTypes.size() != other.m_returnParameterTypes.size())
		return false;
	auto typeCompare = [](TypePointer const& _a, TypePointer const& _b) -> bool { return *_a == *_b; };

	if (!equal(m_parameterTypes.cbegin(), m_parameterTypes.cend(),
			   other.m_parameterTypes.cbegin(), typeCompare))
		return false;
	if (!equal(m_returnParameterTypes.cbegin(), m_returnParameterTypes.cend(),
			   other.m_returnParameterTypes.cbegin(), typeCompare))
		return false;
	//@todo this is ugly, but cannot be prevented right now
	if (m_gasSet != other.m_gasSet || m_valueSet != other.m_valueSet)
		return false;
	if (bound() != other.bound())
		return false;
	if (bound() && *selfType() != *other.selfType())
		return false;
	return true;
}

string FunctionType::toString(bool _short) const
{
	string name = "function (";
	for (auto it = m_parameterTypes.begin(); it != m_parameterTypes.end(); ++it)
		name += (*it)->toString(_short) + (it + 1 == m_parameterTypes.end() ? "" : ",");
	name += ") returns (";
	for (auto it = m_returnParameterTypes.begin(); it != m_returnParameterTypes.end(); ++it)
		name += (*it)->toString(_short) + (it + 1 == m_returnParameterTypes.end() ? "" : ",");
	return name + ")";
}

u256 FunctionType::storageSize() const
{
	BOOST_THROW_EXCEPTION(
		InternalCompilerError()
			<< errinfo_comment("Storage size of non-storable function type requested."));
}

unsigned FunctionType::sizeOnStack() const
{
	Location location = m_location;
	if (m_location == Location::SetGas || m_location == Location::SetValue)
	{
		solAssert(m_returnParameterTypes.size() == 1, "");
		location = dynamic_cast<FunctionType const&>(*m_returnParameterTypes.front()).m_location;
	}

	unsigned size = 0;
	if (location == Location::External || location == Location::CallCode || location == Location::DelegateCall)
		size = 2;
	else if (location == Location::Bare || location == Location::BareCallCode || location == Location::BareDelegateCall)
		size = 1;
	else if (location == Location::Internal)
		size = 1;
	else if (location == Location::ArrayPush || location == Location::ByteArrayPush)
		size = 1;
	if (m_gasSet)
		size++;
	if (m_valueSet)
		size++;
	if (bound())
		size += m_parameterTypes.front()->sizeOnStack();
	return size;
}

FunctionTypePointer FunctionType::interfaceFunctionType() const
{
	// Note that m_declaration might also be a state variable!
	solAssert(m_declaration, "Declaration needed to determine interface function type.");
	bool isLibraryFunction = dynamic_cast<ContractDefinition const&>(*m_declaration->scope()).isLibrary();

	TypePointers paramTypes;
	TypePointers retParamTypes;

	for (auto type: m_parameterTypes)
	{
		if (auto ext = type->interfaceType(isLibraryFunction))
			paramTypes.push_back(ext);
		else
			return FunctionTypePointer();
	}
	for (auto type: m_returnParameterTypes)
	{
		if (auto ext = type->interfaceType(isLibraryFunction))
			retParamTypes.push_back(ext);
		else
			return FunctionTypePointer();
	}
	auto variable = dynamic_cast<VariableDeclaration const*>(m_declaration);
	if (variable && retParamTypes.empty())
		return FunctionTypePointer();

	return make_shared<FunctionType>(
		paramTypes, retParamTypes,
		m_parameterNames, m_returnParameterNames,
		m_location, m_arbitraryParameters,
		m_declaration, m_isConstant, m_isPayable
	);
}

MemberList::MemberMap FunctionType::nativeMembers(ContractDefinition const*) const
{
	switch (m_location)
	{
	case Location::External:
	case Location::Creation:
	case Location::ECRecover:
	case Location::SHA256:
	case Location::RIPEMD160:
	case Location::Bare:
	case Location::BareCallCode:
	case Location::BareDelegateCall:
	{
		MemberList::MemberMap members;
		if (m_location != Location::BareDelegateCall && m_location != Location::DelegateCall)
		{
			if (m_isPayable)
				members.push_back(MemberList::Member(
					"value",
					make_shared<FunctionType>(
						parseElementaryTypeVector({"uint"}),
						TypePointers{copyAndSetGasOrValue(false, true)},
						strings(),
						strings(),
						Location::SetValue,
						false,
						nullptr,
						false,
						false,
						m_gasSet,
						m_valueSet
					)
				));
		}
		if (m_location != Location::Creation)
			members.push_back(MemberList::Member(
				"gas",
				make_shared<FunctionType>(
					parseElementaryTypeVector({"uint"}),
					TypePointers{copyAndSetGasOrValue(true, false)},
					strings(),
					strings(),
					Location::SetGas,
					false,
					nullptr,
					false,
					false,
					m_gasSet,
					m_valueSet
				)
			));
		return members;
	}
	default:
		return MemberList::MemberMap();
	}
}

bool FunctionType::canTakeArguments(TypePointers const& _argumentTypes, TypePointer const& _selfType) const
{
	solAssert(!bound() || _selfType, "");
	if (bound() && !_selfType->isImplicitlyConvertibleTo(*selfType()))
		return false;
	TypePointers paramTypes = parameterTypes();
	if (takesArbitraryParameters())
		return true;
	else if (_argumentTypes.size() != paramTypes.size())
		return false;
	else
		return equal(
			_argumentTypes.cbegin(),
			_argumentTypes.cend(),
			paramTypes.cbegin(),
			[](TypePointer const& argumentType, TypePointer const& parameterType)
			{
				return argumentType->isImplicitlyConvertibleTo(*parameterType);
			}
		);
}

bool FunctionType::hasEqualArgumentTypes(FunctionType const& _other) const
{
	if (m_parameterTypes.size() != _other.m_parameterTypes.size())
		return false;
	return equal(
		m_parameterTypes.cbegin(),
		m_parameterTypes.cend(),
		_other.m_parameterTypes.cbegin(),
		[](TypePointer const& _a, TypePointer const& _b) -> bool { return *_a == *_b; }
	);
}

bool FunctionType::isBareCall() const
{
	switch (m_location)
	{
	case Location::Bare:
	case Location::BareCallCode:
	case Location::BareDelegateCall:
	case Location::ECRecover:
	case Location::SHA256:
	case Location::RIPEMD160:
		return true;
	default:
		return false;
	}
}

string FunctionType::externalSignature() const
{
	solAssert(m_declaration != nullptr, "External signature of function needs declaration");

	bool _inLibrary = dynamic_cast<ContractDefinition const&>(*m_declaration->scope()).isLibrary();

	string ret = m_declaration->name() + "(";

	FunctionTypePointer external = interfaceFunctionType();
	solAssert(!!external, "External function type requested.");
	TypePointers externalParameterTypes = external->parameterTypes();
	for (auto it = externalParameterTypes.cbegin(); it != externalParameterTypes.cend(); ++it)
	{
		solAssert(!!(*it), "Parameter should have external type");
		ret += (*it)->canonicalName(_inLibrary) + (it + 1 == externalParameterTypes.cend() ? "" : ",");
	}

	return ret + ")";
}

u256 FunctionType::externalIdentifier() const
{
	return FixedHash<4>::Arith(FixedHash<4>(dev::sha3(externalSignature())));
}

TypePointers FunctionType::parseElementaryTypeVector(strings const& _types)
{
	TypePointers pointers;
	pointers.reserve(_types.size());
	for (string const& type: _types)
		pointers.push_back(Type::fromElementaryTypeName(type));
	return pointers;
}

TypePointer FunctionType::copyAndSetGasOrValue(bool _setGas, bool _setValue) const
{
	return make_shared<FunctionType>(
		m_parameterTypes,
		m_returnParameterTypes,
		m_parameterNames,
		m_returnParameterNames,
		m_location,
		m_arbitraryParameters,
		m_declaration,
		m_isConstant,
		m_isPayable,
		m_gasSet || _setGas,
		m_valueSet || _setValue,
		m_bound
	);
}

FunctionTypePointer FunctionType::asMemberFunction(bool _inLibrary, bool _bound) const
{
	TypePointers parameterTypes;
	for (auto const& t: m_parameterTypes)
	{
		auto refType = dynamic_cast<ReferenceType const*>(t.get());
		if (refType && refType->location() == DataLocation::CallData)
			parameterTypes.push_back(refType->copyForLocation(DataLocation::Memory, false));
		else
			parameterTypes.push_back(t);
	}

	Location location = m_location;
	if (_inLibrary)
	{
		solAssert(!!m_declaration, "Declaration has to be available.");
		if (!m_declaration->isPublic())
			location = Location::Internal; // will be inlined
		else
			location = Location::DelegateCall;
	}

	TypePointers returnParameterTypes = m_returnParameterTypes;
	if (location != Location::Internal)
	{
		// Alter dynamic types to be non-accessible.
		for (auto& param: returnParameterTypes)
			if (param->isDynamicallySized())
				param = make_shared<InaccessibleDynamicType>();
	}

	return make_shared<FunctionType>(
		parameterTypes,
		returnParameterTypes,
		m_parameterNames,
		m_returnParameterNames,
		location,
		m_arbitraryParameters,
		m_declaration,
		m_isConstant,
		m_isPayable,
		m_gasSet,
		m_valueSet,
		_bound
	);
}

vector<string> const FunctionType::parameterTypeNames(bool _addDataLocation) const
{
	vector<string> names;
	for (TypePointer const& t: parameterTypes())
		names.push_back(t->canonicalName(_addDataLocation));

	return names;
}

vector<string> const FunctionType::returnParameterTypeNames(bool _addDataLocation) const
{
	vector<string> names;
	for (TypePointer const& t: m_returnParameterTypes)
		names.push_back(t->canonicalName(_addDataLocation));

	return names;
}

TypePointer FunctionType::selfType() const
{
	solAssert(bound(), "Function is not bound.");
	solAssert(m_parameterTypes.size() > 0, "Function has no self type.");
	return m_parameterTypes.at(0);
}

ASTPointer<ASTString> FunctionType::documentation() const
{
	auto function = dynamic_cast<Documented const*>(m_declaration);
	if (function)
		return function->documentation();

	return ASTPointer<ASTString>();
}

bool MappingType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	MappingType const& other = dynamic_cast<MappingType const&>(_other);
	return *other.m_keyType == *m_keyType && *other.m_valueType == *m_valueType;
}

string MappingType::toString(bool _short) const
{
	return "mapping(" + keyType()->toString(_short) + " => " + valueType()->toString(_short) + ")";
}

string MappingType::canonicalName(bool) const
{
	return "mapping(" + keyType()->canonicalName(false) + " => " + valueType()->canonicalName(false) + ")";
}

bool TypeType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	TypeType const& other = dynamic_cast<TypeType const&>(_other);
	return *actualType() == *other.actualType();
}

u256 TypeType::storageSize() const
{
	BOOST_THROW_EXCEPTION(
		InternalCompilerError()
			<< errinfo_comment("Storage size of non-storable type type requested."));
}

unsigned TypeType::sizeOnStack() const
{
	if (auto contractType = dynamic_cast<ContractType const*>(m_actualType.get()))
		if (contractType->contractDefinition().isLibrary())
			return 1;
	return 0;
}

MemberList::MemberMap TypeType::nativeMembers(ContractDefinition const* _currentScope) const
{
	MemberList::MemberMap members;
	if (m_actualType->category() == Category::Contract)
	{
		ContractDefinition const& contract = dynamic_cast<ContractType const&>(*m_actualType).contractDefinition();
		bool isBase = false;
		if (_currentScope != nullptr)
		{
			auto const& currentBases = _currentScope->annotation().linearizedBaseContracts;
			isBase = (find(currentBases.begin(), currentBases.end(), &contract) != currentBases.end());
		}
		if (contract.isLibrary())
			for (FunctionDefinition const* function: contract.definedFunctions())
				if (function->isVisibleInDerivedContracts())
					members.push_back(MemberList::Member(
						function->name(),
						FunctionType(*function).asMemberFunction(true),
						function
					));
		if (isBase)
		{
			// We are accessing the type of a base contract, so add all public and protected
			// members. Note that this does not add inherited functions on purpose.
			for (Declaration const* decl: contract.inheritableMembers())
				members.push_back(MemberList::Member(decl->name(), decl->type(), decl));
		}
		else
		{
			for (auto const& stru: contract.definedStructs())
				members.push_back(MemberList::Member(stru->name(), stru->type(), stru));
			for (auto const& enu: contract.definedEnums())
				members.push_back(MemberList::Member(enu->name(), enu->type(), enu));
		}
	}
	else if (m_actualType->category() == Category::Enum)
	{
		EnumDefinition const& enumDef = dynamic_cast<EnumType const&>(*m_actualType).enumDefinition();
		auto enumType = make_shared<EnumType>(enumDef);
		for (ASTPointer<EnumValue> const& enumValue: enumDef.members())
			members.push_back(MemberList::Member(enumValue->name(), enumType));
	}
	return members;
}

ModifierType::ModifierType(const ModifierDefinition& _modifier)
{
	TypePointers params;
	params.reserve(_modifier.parameters().size());
	for (ASTPointer<VariableDeclaration> const& var: _modifier.parameters())
		params.push_back(var->annotation().type);
	swap(params, m_parameterTypes);
}

u256 ModifierType::storageSize() const
{
	BOOST_THROW_EXCEPTION(
		InternalCompilerError()
			<< errinfo_comment("Storage size of non-storable type type requested."));
}

bool ModifierType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	ModifierType const& other = dynamic_cast<ModifierType const&>(_other);

	if (m_parameterTypes.size() != other.m_parameterTypes.size())
		return false;
	auto typeCompare = [](TypePointer const& _a, TypePointer const& _b) -> bool { return *_a == *_b; };

	if (!equal(m_parameterTypes.cbegin(), m_parameterTypes.cend(),
			   other.m_parameterTypes.cbegin(), typeCompare))
		return false;
	return true;
}

string ModifierType::toString(bool _short) const
{
	string name = "modifier (";
	for (auto it = m_parameterTypes.begin(); it != m_parameterTypes.end(); ++it)
		name += (*it)->toString(_short) + (it + 1 == m_parameterTypes.end() ? "" : ",");
	return name + ")";
}

bool ModuleType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	return &m_sourceUnit == &dynamic_cast<ModuleType const&>(_other).m_sourceUnit;
}

MemberList::MemberMap ModuleType::nativeMembers(ContractDefinition const*) const
{
	MemberList::MemberMap symbols;
	for (auto const& symbolName: m_sourceUnit.annotation().exportedSymbols)
		for (Declaration const* symbol: symbolName.second)
			symbols.push_back(MemberList::Member(symbolName.first, symbol->type(), symbol));
	return symbols;
}

string ModuleType::toString(bool) const
{
	return string("module \"") + m_sourceUnit.annotation().path + string("\"");
}

bool MagicType::operator==(Type const& _other) const
{
	if (_other.category() != category())
		return false;
	MagicType const& other = dynamic_cast<MagicType const&>(_other);
	return other.m_kind == m_kind;
}

MemberList::MemberMap MagicType::nativeMembers(ContractDefinition const*) const
{
	switch (m_kind)
	{
	case Kind::Block:
		return MemberList::MemberMap({
			{"coinbase", make_shared<IntegerType>(0, IntegerType::Modifier::Address)},
			{"timestamp", make_shared<IntegerType>(256)},
			{"blockhash", make_shared<FunctionType>(strings{"uint"}, strings{"bytes32"}, FunctionType::Location::BlockHash)},
			{"difficulty", make_shared<IntegerType>(256)},
			{"number", make_shared<IntegerType>(256)},
			{"gaslimit", make_shared<IntegerType>(256)}
		});
	case Kind::Message:
		return MemberList::MemberMap({
			{"sender", make_shared<IntegerType>(0, IntegerType::Modifier::Address)},
			{"gas", make_shared<IntegerType>(256)},
			{"value", make_shared<IntegerType>(256)},
			{"data", make_shared<ArrayType>(DataLocation::CallData)},
			{"sig", make_shared<FixedBytesType>(4)}
		});
	case Kind::Transaction:
		return MemberList::MemberMap({
			{"origin", make_shared<IntegerType>(0, IntegerType::Modifier::Address)},
			{"gasprice", make_shared<IntegerType>(256)}
		});
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown kind of magic."));
	}
}

string MagicType::toString(bool) const
{
	switch (m_kind)
	{
	case Kind::Block:
		return "block";
	case Kind::Message:
		return "msg";
	case Kind::Transaction:
		return "tx";
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown kind of magic."));
	}
}
