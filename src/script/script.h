// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SCRIPT_H
#define BITCOIN_SCRIPT_SCRIPT_H

#include <attributes.h>
#include <crypto/common.h>
#include <prevector.h> // IWYU pragma: export
#include <serialize.h>
#include <uint256.h>
#include <util/hash_type.h>
#include "script_error.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <optional>

// Maximum number of bytes pushable to the stack
static const unsigned int MAX_SCRIPT_ELEMENT_SIZE = 520;

// Maximum number of non-push operations per script
static const int MAX_OPS_PER_SCRIPT = 201;

// Maximum number of public keys per multisig
static const int MAX_PUBKEYS_PER_MULTISIG = 20;

/** The limit of keys in OP_CHECKSIGADD-based scripts. It is due to the stack limit in BIP342. */
static constexpr unsigned int MAX_PUBKEYS_PER_MULTI_A = 999;

// Maximum script length in bytes
static const int MAX_SCRIPT_SIZE = 10000;

// Maximum number of values on script interpreter stack
static const int MAX_STACK_SIZE = 1000;

// Threshold for nLockTime: below this value it is interpreted as block number,
// otherwise as UNIX timestamp.
static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC

// Maximum nLockTime. Since a lock time indicates the last invalid timestamp, a
// transaction with this lock time will never be valid unless lock time
// checking is disabled (by setting all input sequence numbers to
// SEQUENCE_FINAL).
static const uint32_t LOCKTIME_MAX = 0xFFFFFFFFU;

typedef std::vector<uint8_t> valtype;
// Tag for input annex. If there are at least two witness elements for a transaction input,
// and the first byte of the last element is 0x50, this last element is called annex, and
// has meanings independent of the script
static constexpr unsigned int ANNEX_TAG = 0x50;

// Validation weight per passing signature (Tapscript only, see BIP 342).
static constexpr int64_t VALIDATION_WEIGHT_PER_SIGOP_PASSED{50};

// How much weight budget is added to the witness size (Tapscript only, see BIP 342).
static constexpr int64_t VALIDATION_WEIGHT_OFFSET{50};

template <typename T>
std::vector<uint8_t> ToByteVector(const T& in)
{
    return std::vector<uint8_t>(in.begin(), in.end());
}

/** Script opcodes */
enum opcodetype
{
    // push value
    OP_0 = 0x00,
    OP_FALSE = OP_0,
    OP_PUSHDATA1 = 0x4c,
    OP_PUSHDATA2 = 0x4d,
    OP_PUSHDATA4 = 0x4e,
    OP_1NEGATE = 0x4f,
    OP_RESERVED = 0x50,
    OP_1 = 0x51,
    OP_TRUE=OP_1,
    OP_2 = 0x52,
    OP_3 = 0x53,
    OP_4 = 0x54,
    OP_5 = 0x55,
    OP_6 = 0x56,
    OP_7 = 0x57,
    OP_8 = 0x58,
    OP_9 = 0x59,
    OP_10 = 0x5a,
    OP_11 = 0x5b,
    OP_12 = 0x5c,
    OP_13 = 0x5d,
    OP_14 = 0x5e,
    OP_15 = 0x5f,
    OP_16 = 0x60,

    // control
    OP_NOP = 0x61,
    OP_VER = 0x62,
    OP_IF = 0x63,
    OP_NOTIF = 0x64,
    OP_VERIF = 0x65,
    OP_VERNOTIF = 0x66,
    OP_ELSE = 0x67,
    OP_ENDIF = 0x68,
    OP_VERIFY = 0x69,
    OP_RETURN = 0x6a,

    // stack ops
    OP_TOALTSTACK = 0x6b,
    OP_FROMALTSTACK = 0x6c,
    OP_2DROP = 0x6d,
    OP_2DUP = 0x6e,
    OP_3DUP = 0x6f,
    OP_2OVER = 0x70,
    OP_2ROT = 0x71,
    OP_2SWAP = 0x72,
    OP_IFDUP = 0x73,
    OP_DEPTH = 0x74,
    OP_DROP = 0x75,
    OP_DUP = 0x76,
    OP_NIP = 0x77,
    OP_OVER = 0x78,
    OP_PICK = 0x79,
    OP_ROLL = 0x7a,
    OP_ROT = 0x7b,
    OP_SWAP = 0x7c,
    OP_TUCK = 0x7d,

    // splice ops
    OP_CAT = 0x7e,
    OP_SUBSTR = 0x7f,
    OP_LEFT = 0x80,
    OP_RIGHT = 0x81,
    OP_SIZE = 0x82,

    // bit logic
    OP_INVERT = 0x83,
    OP_AND = 0x84,
    OP_OR = 0x85,
    OP_XOR = 0x86,
    OP_EQUAL = 0x87,
    OP_EQUALVERIFY = 0x88,
    OP_RESERVED1 = 0x89,
    OP_RESERVED2 = 0x8a,

    // numeric
    OP_1ADD = 0x8b,
    OP_1SUB = 0x8c,
    OP_2MUL = 0x8d,
    OP_2DIV = 0x8e,
    OP_NEGATE = 0x8f,
    OP_ABS = 0x90,
    OP_NOT = 0x91,
    OP_0NOTEQUAL = 0x92,

    OP_ADD = 0x93,
    OP_SUB = 0x94,
    OP_MUL = 0x95,
    OP_DIV = 0x96,
    OP_MOD = 0x97,
    OP_LSHIFT = 0x98,
    OP_RSHIFT = 0x99,

    OP_BOOLAND = 0x9a,
    OP_BOOLOR = 0x9b,
    OP_NUMEQUAL = 0x9c,
    OP_NUMEQUALVERIFY = 0x9d,
    OP_NUMNOTEQUAL = 0x9e,
    OP_LESSTHAN = 0x9f,
    OP_GREATERTHAN = 0xa0,
    OP_LESSTHANOREQUAL = 0xa1,
    OP_GREATERTHANOREQUAL = 0xa2,
    OP_MIN = 0xa3,
    OP_MAX = 0xa4,

    OP_WITHIN = 0xa5,

    // crypto
    OP_RIPEMD160 = 0xa6,
    OP_SHA1 = 0xa7,
    OP_SHA256 = 0xa8,
    OP_HASH160 = 0xa9,
    OP_HASH256 = 0xaa,
    OP_CODESEPARATOR = 0xab,
    OP_CHECKSIG = 0xac,
    OP_CHECKSIGVERIFY = 0xad,
    OP_CHECKMULTISIG = 0xae,
    OP_CHECKMULTISIGVERIFY = 0xaf,

    // expansion
    OP_NOP1 = 0xb0,
    OP_CHECKLOCKTIMEVERIFY = 0xb1,
    OP_NOP2 = OP_CHECKLOCKTIMEVERIFY,
    OP_CHECKSEQUENCEVERIFY = 0xb2,
    OP_NOP3 = OP_CHECKSEQUENCEVERIFY,
    OP_CHECKTEMPLATEVERIFY = 0xb3,
    OP_NOP4 = OP_CHECKTEMPLATEVERIFY,
    OP_NOP5 = 0xb4,
    OP_NOP6 = 0xb5,
    OP_NOP7 = 0xb6,
    OP_NOP8 = 0xb7,
    OP_NOP9 = 0xb8,
    OP_NOP10 = 0xb9,

    // Opcode added by BIP 342 (Tapscript)
    OP_CHECKSIGADD = 0xba,

    OP_INVALIDOPCODE = 0xff,
};

// Maximum value that an opcode can be
static const unsigned int MAX_OPCODE = OP_NOP10;

std::string GetOpName(opcodetype opcode);

struct scriptnum_error : std::runtime_error {
    ScriptError scriptError;
    explicit
    scriptnum_error(const std::string &str, ScriptError err = ScriptError::SCRIPT_ERR_UNKNOWN_ERROR)
        : std::runtime_error(str), scriptError(err)
    {}
};


/**
 * Base template class for CScriptNum and ScriptInt. This class implements
 * some of the functionality common to both subclasses, and also captures
 * some enforcement of the consensus rules related to:
 *
 *  - valid 64 bit range (INT64_MIN is forbidden)
 *  - trapping for arithmetic operations that overflow or that produce a
 *    result equal to INT64_MIN
 */
template <typename Derived>
struct ScriptIntBase {
public:
    /**
     * Factory method to safely construct an instance from a raw int64_t.
     *
     * Note the unusual enforcement of the rules regarding valid 64-bit
     * ranges. We enforce a strict range of [INT64_MIN+1, INT64_MAX].
     */
    static constexpr
    std::optional<Derived> fromInt(int64_t x) noexcept {
        if ( ! valid64BitRange(x)) {
            return std::nullopt;
        }
        return Derived(x);
    }

    /// Performance/convenience optimization: Construct an instance from a raw
    /// int64_t where the caller already knows that the supplied value is in range.
    static constexpr
    Derived fromIntUnchecked(int64_t x) noexcept {
        return Derived(x);
    }

    constexpr
    bool operator==(int64_t x) const noexcept { return value_ == x; }

    constexpr
    bool operator!=(int64_t x) const noexcept { return value_ != x; }

    constexpr
    bool operator<=(int64_t x) const noexcept { return value_ <= x; }

    constexpr
    bool operator<(int64_t x) const noexcept { return value_ < x; }

    constexpr
    bool operator>=(int64_t x) const noexcept { return value_ >= x; }

    constexpr
    bool operator>(int64_t x) const noexcept { return value_ > x; }

    constexpr
    bool operator==(Derived const& x) const noexcept {
        return operator==(x.value_);
    }

    constexpr
    bool operator!=(Derived const& x) const noexcept {
        return operator!=(x.value_);
    }

    constexpr
    bool operator<=(Derived const& x) const noexcept {
        return operator<=(x.value_);
    }

    constexpr
    bool operator<(Derived const& x) const noexcept {
        return operator<(x.value_);
    }

    constexpr
    bool operator>=(Derived const& x) const noexcept {
        return operator>=(x.value_);
    }

    constexpr
    bool operator>(Derived const& x) const noexcept {
        return operator>(x.value_);
    }

    // Arithmetic operations
    std::optional<Derived> safeAdd(int64_t x) const noexcept {
        bool const res = __builtin_add_overflow(value_, x, &x);
        if (res) {
            return std::nullopt;
        }
        if ( ! valid64BitRange(x)) {
            return std::nullopt;
        }
        return Derived(x);
    }

    std::optional<Derived> safeAdd(Derived const& x) const noexcept {
        return safeAdd(x.value_);
    }

    std::optional<Derived> safeSub(int64_t x) const noexcept {
        bool const res = __builtin_sub_overflow(value_, x, &x);
        if (res) {
            return std::nullopt;
        }
        if ( ! valid64BitRange(x)) {
            return std::nullopt;
        }
        return Derived(x);
    }

    std::optional<Derived> safeSub(Derived const& x) const noexcept {
        return safeSub(x.value_);
    }

    std::optional<Derived> safeMul(int64_t x) const noexcept {
        bool const res = __builtin_mul_overflow(value_, x, &x);
        if (res) {
            return std::nullopt;
        }
        if ( ! valid64BitRange(x)) {
            return std::nullopt;
        }
        return Derived(x);
    }

    std::optional<Derived> safeMul(Derived const& x) const noexcept {
        return safeMul(x.value_);
    }

    constexpr
    Derived operator/(int64_t x) const noexcept {
        if (x == -1 && ! valid64BitRange(value_)) {
            // Guard against overflow, which can't normally happen unless class is misused
            // by the fromIntUnchecked() factory method (may happen in tests).
            // This will return INT64_MIN which is what ARM & x86 does anyway for INT64_MIN / -1.
            return Derived(value_);
        }
        return Derived(value_ / x);
    }

    constexpr
    Derived operator/(Derived const& x) const noexcept {
        return operator/(x.value_);
    }

    constexpr
    Derived operator%(int64_t x) const noexcept {
        if (x == -1 && ! valid64BitRange(value_)) {
            // INT64_MIN % -1 is UB in C++, but mathematically it would yield 0
            return Derived(0);
        }
        return Derived(value_ % x);
    }

    constexpr
    Derived operator%(Derived const& x) const noexcept {
        return operator%(x.value_);
    }

    // Bitwise operations
    std::optional<Derived> safeBitwiseAnd(int64_t x) const noexcept {
        x = value_ & x;
        if ( ! valid64BitRange(x)) {
            return std::nullopt;
        }
        return Derived(x);
    }

    std::optional<Derived> safeBitwiseAnd(Derived const& x) const noexcept {
        return safeBitwiseAnd(x.value_);
    }

    constexpr
    Derived operator-() const noexcept {
        // Defensive programming: -INT64_MIN is UB
        return Derived(valid64BitRange(value_) ? -value_ : value_);
    }

    constexpr
    int64_t getint64() const noexcept {
        return value_;
    }

protected:
    static constexpr
    bool valid64BitRange(int64_t x) {
        return x != std::numeric_limits<int64_t>::min();
    }

    explicit constexpr
    ScriptIntBase(int64_t x)
        : value_(x)
    {}

    int64_t value_;
};

/**
 * A ScriptInt is a "write-only" class designed to be used with
 * CScript in order to tell the CScript serialization engine to
 * represent small numbers in a more compact way.  It is
 * interchangeable with CScriptNum for serialization purposes,
 * except that for small numbers in the range [-1, 16] ScriptInt
 * ends up serializing slightly smaller, saving one byte.
 *
 * This is because the CScript class serializes ScriptInt differently
 * than it does CScriptNum for integers in the range [-1, 16].
 *
 * Whereas CScriptNum is always pushed as an encapsulated byte blob,
 * ScriptInt instances in the range [-1, 16] are pushed as raw bytes
 * directly (with some offsetting around OP_16 as the anchor).
 *
 * For numbers outside the [-1, 16] range, ScriptInt serializes
 * identically to CScriptNum.
 *
 * When the resulting script is interpreted by the script interpreter,
 * any values that are serialized in this more compact way are internally
 * transformed and normalized into CScriptNum instances on the stack
 * (see interpreter.cpp).  So the purpose of this class is simply as
 * a "type tag" to tell CScript to serialize in the more compact form,
 * if possible.
 *
 * In short, these two serialize differently:
 *
 *   CScript() << CScriptNum::fromIntUnchecked(10); // [PUSH(1) 0x0a] (2 bytes)
 *   CScript() << ScriptInt::fromIntUnchecked(10);  // [0x5a] (1 byte)
 *
 * However, for integers outside the range [-1, 16], the serialization
 * is identical:
 *
 *   CScript() << CScriptNum::fromIntUnchecked(42); // [PUSH(1) 0x2a] (2 bytes)
 *   CScript() << ScriptInt::fromIntUnchecked(42);  // Same as above
 *
 * Note that due to quirks in how CScriptNum serializes 0, these two
 * also serialize identically:
 *
 *   CScript() << CScriptNum::fromIntUnchecked(0);  // [PUSH(0)] == [0x00] (1 byte)
 *   CScript() << ScriptInt::fromIntUnchecked(0);   // [0x00] (1 byte)
 */
struct ScriptInt : ScriptIntBase<ScriptInt> {
    friend ScriptIntBase;

private:
    explicit constexpr
    ScriptInt(int64_t x) noexcept
        : ScriptIntBase(x)
    {}
};

/**
 * CScriptNum is used to encapsulate signed numbers as byte blobs in a CScript.
 * Its specified range is over [INT64_MIN+1, INT64_MAX].  Attempts to encapsulate
 * a number outside this range are undefined behavior.
 *
 * Note that before Upgrade8, consensus rules forbade a CScriptNum significantly
 * outside the 32 bit range (with some corner case exceptions for temporaries in
 * the interpreter).
 *
 * After Upgrade8 we allow 64 bit numbers in the range [INT64_MIN+1, INT64_MAX].
 * We forbid INT64_MIN, however, since this would encode to a 9 byte CScriptNum
 * and we prefer to keep things simple and restrict CScriptNum to 8 bytes.
 *
 * A CScriptNum gets serialized to a non-2's complement notation, in little-endian
 * byte order. The most significant bit (in the last, most significant byte) is
 * the sign bit. The other bits preceeding it (little endian) are the magnitude.
 *
 * This means that INT64_MIN would get serialized to 9 bytes in this encoding,
 * which is why it is forbidden (since we prefer to limit them to 8 serialized
 * bytes, for simplicity's sake).
 */
class CScriptNum : public ScriptIntBase<CScriptNum> {
/**
 * Pre Upgrade8 Hardfork semantics:
 * Numeric opcodes (OP_1ADD, etc) are restricted to operating on 4-byte
 * integers. The semantics are subtle, though: operands must be in the range
 * [-2^31 + 1, 2^31 - 1], but results may overflow (and are valid as long as
 * they are not used in a subsequent numeric operation). CScriptNum enforces
 * those semantics by storing results as an int64 and allowing out-of-range
 * values to be returned as a vector of bytes but throwing an exception if
 * arithmetic is done or the result is interpreted as an integer.
 *
 * Post Upgrade8 Hardfork semantics:
 * Arithmetic opcodes (OP_1ADD, etc) are restricted to operating on 8-byte signed integers.
 * Negative integers are encoding using sign and magnitude, so operands must be in the range
 * [-2^63 + 1, 2^63 - 1].
 * Arithmetic operators throw an exception if overflow is detected.
 */
public:
    friend ScriptIntBase;

    static constexpr size_t MAXIMUM_ELEMENT_SIZE_32_BIT = 4;
    static constexpr size_t MAXIMUM_ELEMENT_SIZE_64_BIT = 8;

    typedef std::optional<CScriptNum> ScriptNumResult;

private:
    explicit constexpr
    CScriptNum(int64_t x) noexcept
        : ScriptIntBase(x)
    {}

public:

    explicit
    CScriptNum(const std::vector<uint8_t> &vch, bool fRequireMinimal, size_t maxIntegerSize)
        : ScriptIntBase(fromBytes(vch, fRequireMinimal, maxIntegerSize))
    {}

    static
    bool IsMinimallyEncoded(const std::vector<uint8_t> &vch, size_t maxIntegerSize);

    static
    bool MinimallyEncode(std::vector<uint8_t> &data);

    constexpr
    int32_t getint32() const noexcept {
        if (value_ > std::numeric_limits<int32_t>::max()) {
            return std::numeric_limits<int32_t>::max();
        } else if (value_ < std::numeric_limits<int32_t>::min()) {
            return std::numeric_limits<int32_t>::min();
        }
        return value_;
    }

    std::vector<uint8_t> getvch() const { return serialize(value_); }

    static
    std::vector<uint8_t> serialize(int64_t value) {
        if (value == 0) {
            return {};
        }

        std::vector<uint8_t> result;
        const bool neg = value < 0;
        // NB: -INT64_MIN in 2's complement is UB, so we must guard against it here.
        uint64_t absvalue = neg && valid64BitRange(value) ? -value : value;

        while (absvalue) {
            result.push_back(absvalue & 0xff);
            absvalue >>= 8;
        }

        // - If the most significant byte is >= 0x80 and the value is positive,
        // push a new zero-byte to make the significant byte < 0x80 again.
        // - If the most significant byte is >= 0x80 and the value is negative,
        // push a new 0x80 byte that will be popped off when converting to an
        // integral.
        // - If the most significant byte is < 0x80 and the value is negative,
        // add 0x80 to it, since it will be subtracted and interpreted as a
        // negative when converting to an integral.
        if (result.back() & 0x80) {
            result.push_back(neg ? 0x80 : 0);
        } else if (neg) {
            result.back() |= 0x80;
        }

        return result;
    }


private:
    static
    int64_t fromBytes(std::vector<uint8_t> const& vch, bool fRequireMinimal, size_t maxIntegerSize) {
        if (maxIntegerSize > MAXIMUM_ELEMENT_SIZE_64_BIT) {
            throw scriptnum_error("maxIntegerSize cannot be greater than 8");
        }
        if (vch.size() > maxIntegerSize) {
            throw scriptnum_error("script number overflow",
                                  maxIntegerSize == 8 ? ScriptError::INVALID_NUMBER_RANGE_64_BIT
                                                      : ScriptError::INVALID_NUMBER_RANGE);
        }
        if (fRequireMinimal && ! IsMinimallyEncoded(vch, maxIntegerSize)) {
            throw scriptnum_error("non-minimally encoded script number", ScriptError::MINIMALNUM);
        }
        return set_vch(vch);
    }

    ///! Precondition: vch.size() must be <= 8.
    static
    int64_t set_vch(const std::vector<uint8_t> &vch) {
        if (vch.empty()) {
            return 0;
        }

        int64_t result = 0;
        for (size_t i = 0; i != vch.size(); ++i) {
            result |= int64_t(vch[i]) << 8 * i;
        }

        // If the input vector's most significant byte is 0x80, remove it from
        // the result's msb and return a negative.
        if (vch.back() & 0x80) {
            return -int64_t(result & ~(0x80ULL << (8 * (vch.size() - 1))));
        }

        return result;
    }

};

/**
 * We use a prevector for the script to reduce the considerable memory overhead
 *  of vectors in cases where they normally contain a small number of small elements.
 * Tests in October 2015 showed use of this reduced dbcache memory usage by 23%
 *  and made an initial sync 13% faster.
 */
typedef prevector<28, uint8_t> CScriptBase;

bool GetScriptOp(CScriptBase::const_iterator& pc, CScriptBase::const_iterator end, opcodetype& opcodeRet, std::vector<uint8_t>* pvchRet);

/** Serialized script, used inside transaction inputs and outputs */
class CScript : public CScriptBase
{
protected:
    CScript& push_int64(int64_t n)
    {
        if (n == -1 || (n >= 1 && n <= 16))
        {
            push_back(n + (OP_1 - 1));
        }
        else if (n == 0)
        {
            push_back(OP_0);
        }
        else
        {
            *this << CScriptNum::serialize(n);
        }
        return *this;
    }
public:
    CScript() = default;
    CScript(const_iterator pbegin, const_iterator pend) : CScriptBase(pbegin, pend) { }
    CScript(std::vector<uint8_t>::const_iterator pbegin, std::vector<uint8_t>::const_iterator pend) : CScriptBase(pbegin, pend) { }
    CScript(const uint8_t* pbegin, const uint8_t* pend) : CScriptBase(pbegin, pend) { }

    CScript &operator+=(const CScript &b)
    {
        reserve(size() + b.size());
        insert(end(), b.begin(), b.end());
        return *this;
    }

    friend CScript operator+(const CScript &a, const CScript &b)
    {
        CScript ret = a;
        ret += b;
        return ret;
    }

    SERIALIZE_METHODS(CScript, obj) { READWRITE(AsBase<CScriptBase>(obj)); }

    explicit CScript(int64_t b) { operator<<(b); }
    explicit CScript(opcodetype b) { operator<<(b); }
    explicit CScript(CScriptNum& b) { operator<<(b); }
    // delete non-existent constructor to defend against future introduction
    // e.g. via prevector
    explicit CScript(const std::vector<uint8_t> &b) { operator<<(b); }

    /** Delete non-existent operator to defend against future introduction */
    CScript& operator<<(const CScript& b) = delete;

    CScript& operator<<(int64_t b) LIFETIMEBOUND { return push_int64(b); }

    CScript& operator<<(opcodetype opcode) LIFETIMEBOUND
    {
        if (opcode < 0 || opcode > 0xff)
            throw std::runtime_error("CScript::operator<<(): invalid opcode");
        insert(end(), (uint8_t)opcode);
        return *this;
    }

    CScript& operator<<(const CScriptNum& b) LIFETIMEBOUND
    {
        *this << b.getvch();
        return *this;
    }

    CScript& operator<<(ScriptInt const& x) LIFETIMEBOUND
    {
        return push_int64(x.getint64());
    }

    CScript& operator<<(const std::vector<uint8_t>& b) LIFETIMEBOUND
    {
        if (b.size() < OP_PUSHDATA1)
        {
            insert(end(), (uint8_t)b.size());
        }
        else if (b.size() <= 0xff)
        {
            insert(end(), OP_PUSHDATA1);
            insert(end(), (uint8_t)b.size());
        }
        else if (b.size() <= 0xffff)
        {
            insert(end(), OP_PUSHDATA2);
            uint8_t _data[2];
            WriteLE16(_data, b.size());
            insert(end(), _data, _data + sizeof(_data));
        }
        else
        {
            insert(end(), OP_PUSHDATA4);
            uint8_t _data[4];
            WriteLE32(_data, b.size());
            insert(end(), _data, _data + sizeof(_data));
        }
        insert(end(), b.begin(), b.end());
        return *this;
    }

    bool GetOp(const_iterator& pc, opcodetype& opcodeRet, std::vector<uint8_t>& vchRet) const
    {
        return GetScriptOp(pc, end(), opcodeRet, &vchRet);
    }

    bool GetOp(const_iterator& pc, opcodetype& opcodeRet) const
    {
        return GetScriptOp(pc, end(), opcodeRet, nullptr);
    }

    /** Encode/decode small integers: */
    static int DecodeOP_N(opcodetype opcode)
    {
        if (opcode == OP_0)
            return 0;
        assert(opcode >= OP_1 && opcode <= OP_16);
        return (int)opcode - (int)(OP_1 - 1);
    }
    static opcodetype EncodeOP_N(int n)
    {
        assert(n >= 0 && n <= 16);
        if (n == 0)
            return OP_0;
        return (opcodetype)(OP_1+n-1);
    }

    /**
     * Pre-version-0.6, Bitcoin always counted CHECKMULTISIGs
     * as 20 sigops. With pay-to-script-hash, that changed:
     * CHECKMULTISIGs serialized in scriptSigs are
     * counted more accurately, assuming they are of the form
     *  ... OP_N CHECKMULTISIG ...
     */
    unsigned int GetSigOpCount(bool fAccurate) const;

    /**
     * Accurately count sigOps, including sigOps in
     * pay-to-script-hash transactions:
     */
    unsigned int GetSigOpCount(const CScript& scriptSig) const;

    bool IsPayToBareDefaultCheckTemplateVerifyHash() const;

    /*
     * OP_1 <0x4e73>
     */
    bool IsPayToAnchor() const;
    /** Checks if output of IsWitnessProgram comes from a P2A output script
     */
    static bool IsPayToAnchor(int version, const std::vector<uint8_t>& program);


    bool IsPayToScriptHash() const;
    bool IsPayToWitnessScriptHash() const;
    bool IsWitnessProgram(int& version, std::vector<uint8_t>& program) const;

    /** Called by IsStandardTx and P2SH/BIP62 VerifyScript (which makes it consensus-critical). */
    bool IsPushOnly(const_iterator pc) const;
    bool IsPushOnly() const;

    /** Check if the script contains valid OP_CODES */
    bool HasValidOps() const;

    /**
     * Returns whether the script is guaranteed to fail at execution,
     * regardless of the initial stack. This allows outputs to be pruned
     * instantly when entering the UTXO set.
     */
    bool IsUnspendable() const
    {
        return (size() > 0 && *begin() == OP_RETURN) || (size() > MAX_SCRIPT_SIZE);
    }

    void clear()
    {
        // The default prevector::clear() does not release memory
        CScriptBase::clear();
        shrink_to_fit();
    }
};

struct CScriptWitness
{
    // Note that this encodes the data elements being pushed, rather than
    // encoding them as a CScript that pushes them.
    std::vector<std::vector<uint8_t> > stack;

    // Some compilers complain without a default constructor
    CScriptWitness() = default;

    bool IsNull() const { return stack.empty(); }

    void SetNull() { stack.clear(); stack.shrink_to_fit(); }

    std::string ToString() const;
};

/** A reference to a CScript: the Hash160 of its serialization */
class CScriptID : public BaseHash<uint160>
{
public:
    CScriptID() : BaseHash() {}
    explicit CScriptID(const CScript& in);
    explicit CScriptID(const uint160& in) : BaseHash(in) {}
};

/** Test for OP_SUCCESSx opcodes as defined by BIP342. */
bool IsOpSuccess(const opcodetype& opcode);

bool CheckMinimalPush(const std::vector<uint8_t>& data, opcodetype opcode);

/** Build a script by concatenating other scripts, or any argument accepted by CScript::operator<<. */
template<typename... Ts>
CScript BuildScript(Ts&&... inputs)
{
    CScript ret;
    int cnt{0};

    ([&ret, &cnt] (Ts&& input) {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<Ts>>, CScript>) {
            // If it is a CScript, extend ret with it. Move or copy the first element instead.
            if (cnt == 0) {
                ret = std::forward<Ts>(input);
            } else {
                ret.insert(ret.end(), input.begin(), input.end());
            }
        } else {
            // Otherwise invoke CScript::operator<<.
            ret << input;
        }
        cnt++;
    } (std::forward<Ts>(inputs)), ...);

    return ret;
}

#endif // BITCOIN_SCRIPT_SCRIPT_H
