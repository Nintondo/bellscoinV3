// Copyright (c) 2012-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script.h>
#include <test/scriptnum10.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>
#include <limits.h>
#include <stdint.h>

BOOST_FIXTURE_TEST_SUITE(scriptnum_tests, BasicTestingSetup)

static constexpr int64_t int64_t_min = std::numeric_limits<int64_t>::min();
static constexpr int64_t int64_t_max = std::numeric_limits<int64_t>::max();
static constexpr int64_t int64_t_min_8_bytes = int64_t_min + 1;

/** A selection of numbers that do not trigger int64_t overflow
 *  when added/subtracted. */

static const int64_t values[] = {0,
                                 1,
                                 -1,
                                 -2,
                                 127,
                                 128,
                                 -255,
                                 256,
                                 (1LL << 15) - 1,
                                 -(1LL << 16),
                                 (1LL << 24) - 1,
                                 (1LL << 31),
                                 1 - (1LL << 32),
                                 1LL << 40,
                                 int64_t_min_8_bytes,
                                 int64_t_min,
                                 int64_t_max};

static const int64_t offsets[] = { 1, 0x79, 0x80, 0x81, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000};

static bool verify(const CScriptNum10& bignum, const CScriptNum& scriptnum)
{
    return bignum.getvch() == scriptnum.getvch() && bignum.getint() == scriptnum.getint32();
}

static void CheckCreateVch(int64_t x) {
    size_t const maxIntegerSize = CScriptNum::MAXIMUM_ELEMENT_SIZE_32_BIT;

    CScriptNum10 bigx(x);
    CScriptNum scriptx(CScriptNum::fromIntUnchecked(x));
    BOOST_CHECK(verify(bigx, scriptx));

    CScriptNum10 bigb(bigx.getvch(), false);
    CScriptNum scriptb(scriptx.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigb, scriptb));

    CScriptNum10 bigx3(scriptb.getvch(), false);
    CScriptNum scriptx3(bigb.getvch(), false, maxIntegerSize);
    BOOST_CHECK(verify(bigx3, scriptx3));
}

static void CheckCreateInt(const int64_t& num) {
    CScriptNum scriptx(CScriptNum::fromIntUnchecked(num));
    CScriptNum10 const bigx(num);
    BOOST_CHECK(verify(bigx, scriptx));
    BOOST_CHECK(verify(CScriptNum10(bigx.getint()), CScriptNum::fromIntUnchecked(scriptx.getint32())));
    BOOST_CHECK(verify(CScriptNum10(scriptx.getint32()), CScriptNum::fromIntUnchecked(bigx.getint())));
    BOOST_CHECK(verify(CScriptNum10(CScriptNum10(scriptx.getint32()).getint()),
                       CScriptNum::fromIntUnchecked(CScriptNum::fromIntUnchecked(bigx.getint()).getint32())));
}


static void CheckAdd(int64_t a, int64_t b) {
    if (a == int64_t_min || b == int64_t_min) {
        return;
    }

    CScriptNum10 const biga(a);
    CScriptNum10 const bigb(b);
    auto const scripta = CScriptNum::fromIntUnchecked(a);
    auto const scriptb = CScriptNum::fromIntUnchecked(b);

    // int64_t overflow is undefined.
    bool overflowing = (b > 0 && a > int64_t_max - b) ||
                       (b < 0 && a < int64_t_min_8_bytes - b);

    if ( ! overflowing) {
        auto res = scripta.safeAdd(scriptb);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(biga + bigb, *res));
        res = scripta.safeAdd(b);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(biga + bigb, *res));
        res = scriptb.safeAdd(scripta);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(biga + bigb, *res));
        res = scriptb.safeAdd(a);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(biga + bigb, *res));
    } else {
        BOOST_CHECK(!scripta.safeAdd(scriptb));
        BOOST_CHECK(!scripta.safeAdd(b));
        BOOST_CHECK(!scriptb.safeAdd(a));
    }
}

static void CheckNegate(int64_t x) {
    const CScriptNum10 bigx(x);
    auto const scriptx = CScriptNum::fromIntUnchecked(x);

    // -INT64_MIN is undefined
    if (x != int64_t_min) {
        BOOST_CHECK(verify(-bigx, -scriptx));
    }
}

static
void CheckMultiply(int64_t a, int64_t b) {
    auto res = CScriptNum::fromInt(a);
    if ( ! res) {
        BOOST_CHECK(a == int64_t_min);
        return;
    }
    auto const scripta = *res;

    res = CScriptNum::fromInt(b);
    if ( ! res) {
        BOOST_CHECK(b == int64_t_min);
        return;
    }
    auto const scriptb = *res;

    res = scripta.safeMul(scriptb);
    BOOST_CHECK( ! res || a * b == res->getint64());
    res = scripta.safeMul(b);
    BOOST_CHECK( ! res || a * b == res->getint64());
    res = scriptb.safeMul(scripta);
    BOOST_CHECK( ! res || b * a == res->getint64());
    res = scriptb.safeMul(a);
    BOOST_CHECK( ! res || b * a == res->getint64());
}

static void CheckSubtract(int64_t a, int64_t b) {
    if (a == int64_t_min || b == int64_t_min) {
        return;
    }

    CScriptNum10 const biga(a);
    CScriptNum10 const bigb(b);
    auto const scripta = CScriptNum::fromIntUnchecked(a);
    auto const scriptb = CScriptNum::fromIntUnchecked(b);

    // int64_t overflow is undefined.
    bool overflowing = (b > 0 && a < int64_t_min_8_bytes + b) ||
                       (b < 0 && a > int64_t_max + b);

    if ( ! overflowing) {
        auto res = scripta.safeSub(scriptb);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(biga - bigb, *res));
        res = scripta.safeSub(b);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(biga - bigb, *res));
    } else {
        BOOST_CHECK(!scripta.safeSub(scriptb));
        BOOST_CHECK(!scripta.safeSub(b));
    }

    overflowing = (a > 0 && b < int64_t_min_8_bytes + a) ||
                  (a < 0 && b > int64_t_max + a);

    if ( ! overflowing) {
        auto res = scriptb.safeSub(scripta);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(bigb - biga, *res));
        res = scriptb.safeSub(a);
        BOOST_CHECK(res);
        BOOST_CHECK(verify(bigb - biga, *res));
    } else {
        BOOST_CHECK(!scriptb.safeSub(scripta));
        BOOST_CHECK(!scriptb.safeSub(a));
    }
}

static void CheckCompare(const int64_t& num1, const int64_t& num2)
{
    const CScriptNum10 bignum1(num1);
    const CScriptNum10 bignum2(num2);
    const CScriptNum scriptnum1(CScriptNum::fromIntUnchecked(num1));
    const CScriptNum scriptnum2(CScriptNum::fromIntUnchecked(num2));

    BOOST_CHECK((bignum1 == bignum1) == (scriptnum1 == scriptnum1));
    BOOST_CHECK((bignum1 != bignum1) ==  (scriptnum1 != scriptnum1));
    BOOST_CHECK((bignum1 < bignum1) ==  (scriptnum1 < scriptnum1));
    BOOST_CHECK((bignum1 > bignum1) ==  (scriptnum1 > scriptnum1));
    BOOST_CHECK((bignum1 >= bignum1) ==  (scriptnum1 >= scriptnum1));
    BOOST_CHECK((bignum1 <= bignum1) ==  (scriptnum1 <= scriptnum1));

    BOOST_CHECK((bignum1 == bignum1) == (scriptnum1 == num1));
    BOOST_CHECK((bignum1 != bignum1) ==  (scriptnum1 != num1));
    BOOST_CHECK((bignum1 < bignum1) ==  (scriptnum1 < num1));
    BOOST_CHECK((bignum1 > bignum1) ==  (scriptnum1 > num1));
    BOOST_CHECK((bignum1 >= bignum1) ==  (scriptnum1 >= num1));
    BOOST_CHECK((bignum1 <= bignum1) ==  (scriptnum1 <= num1));

    BOOST_CHECK((bignum1 == bignum2) ==  (scriptnum1 == scriptnum2));
    BOOST_CHECK((bignum1 != bignum2) ==  (scriptnum1 != scriptnum2));
    BOOST_CHECK((bignum1 < bignum2) ==  (scriptnum1 < scriptnum2));
    BOOST_CHECK((bignum1 > bignum2) ==  (scriptnum1 > scriptnum2));
    BOOST_CHECK((bignum1 >= bignum2) ==  (scriptnum1 >= scriptnum2));
    BOOST_CHECK((bignum1 <= bignum2) ==  (scriptnum1 <= scriptnum2));

    BOOST_CHECK((bignum1 == bignum2) ==  (scriptnum1 == num2));
    BOOST_CHECK((bignum1 != bignum2) ==  (scriptnum1 != num2));
    BOOST_CHECK((bignum1 < bignum2) ==  (scriptnum1 < num2));
    BOOST_CHECK((bignum1 > bignum2) ==  (scriptnum1 > num2));
    BOOST_CHECK((bignum1 >= bignum2) ==  (scriptnum1 >= num2));
    BOOST_CHECK((bignum1 <= bignum2) ==  (scriptnum1 <= num2));
}

static void RunCreate(CScriptNum const& scriptx)
{
    size_t const maxIntegerSize = CScriptNum::MAXIMUM_ELEMENT_SIZE_32_BIT;
    int64_t const x = scriptx.getint64();
    CheckCreateInt(x);
    if (scriptx.getvch().size() <= maxIntegerSize) {
        CheckCreateVch(x);
    } else {
        BOOST_CHECK_THROW(CheckCreateVch(x), scriptnum10_error);
    }
}

static
void RunCreateSet(int64_t v, int64_t o) {
    auto const value = CScriptNum::fromIntUnchecked(v);
    auto const offset = CScriptNum::fromIntUnchecked(o);

    RunCreate(value);

    auto res = value.safeAdd(offset);
    if (res) {
        RunCreate(*res);
    }

    res = value.safeSub(offset);
    if (res) {
        RunCreate(*res);
    }
}

static void RunOperators(int64_t a, int64_t b)
{
    CheckAdd(a, b);
    CheckSubtract(a, b);
    CheckMultiply(a, b);
    CheckNegate(a);
    CheckCompare(a, b);
}

BOOST_AUTO_TEST_CASE(creation) {
    for (auto value : values) {
        for (auto offset : offsets) {
            RunCreateSet(value, offset);
        }
    }
}

BOOST_AUTO_TEST_CASE(operators) {
    // Prevent potential UB below
    auto negate = [](int64_t x) { return x != int64_t_min ? -x : int64_t_min; };

    for (auto a : values) {
        RunOperators(a, a);
        RunOperators(a, negate(a));
        for (auto b : values) {
            RunOperators(a, b);
            RunOperators(a, negate(b));
            if (a != int64_t_max && a != int64_t_min && a != int64_t_min_8_bytes &&
                b != int64_t_max && b != int64_t_min && b != int64_t_min_8_bytes) {
                RunOperators(a + b, a);
                RunOperators(a + b, b);
                RunOperators(a - b, a);
                RunOperators(a - b, b);
                RunOperators(a + b, a + b);
                RunOperators(a + b, a - b);
                RunOperators(a - b, a + b);
                RunOperators(a - b, a - b);
                RunOperators(a + b, negate(a));
                RunOperators(a + b, negate(b));
                RunOperators(a - b, negate(a));
                RunOperators(a - b, negate(b));
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
