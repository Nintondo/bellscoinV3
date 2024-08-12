#pragma once
/**
	@file
	@brief generic function for each N
	@author MITSUNARI Shigeo(@herumi)
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/
#include <mcl/op.hpp>
#include <mcl/util.hpp>
#include <cybozu/bit_operation.hpp>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4127)
#endif

namespace mcl { namespace fp {

template<size_t N>
static void shr1T(Unit *y, const Unit *x)
{
	bint::shrT<N>(y, x, 1);
}

template<size_t N>
void addModT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	if (bint::addT<N>(z, x, y)) {
		bint::subT<N>(z, z, p);
		return;
	}
	Unit tmp[N];
	if (bint::subT<N>(tmp, z, p) == 0) {
		bint::copyT<N>(z, tmp);
	}
}

template<size_t N>
void addModNFT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	bint::addNFT<N>(z, x, y);
	Unit tmp[N];
	if (bint::subNFT<N>(tmp, z, p) == 0) {
		bint::copyT<N>(z, tmp);
	}
}

template<size_t N>
void subModT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	if (bint::subT<N>(z, x, y)) {
		bint::addT<N>(z, z, p);
	}
}

template<size_t N>
void subModNFT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	if (bint::subNFT<N>(z, x, y)) {
		bint::addNFT<N>(z, z, p);
	}
}

// y[N] <- (-x[N]) % p[N]
template<size_t N>
static void negT(Unit *y, const Unit *x, const Unit *p)
{
	if (bint::isZeroT<N>(x)) {
		if (x != y) bint::clearT<N>(y);
		return;
	}
	bint::subT<N>(y, p, x);
}

// z[N + 1] <- x[N] * y
template<size_t N>
static void mulUnitPreT(Unit *z, const Unit *x, Unit y)
{
	z[N] = bint::mulUnitT<N>(z, x, y);
}

// z[N] <- (x[N] * y) % p[N]
template<size_t N>
static void mulUnitModT(Unit *z, const Unit *x, Unit y, const Unit *p)
{
	Unit xy[N + 1];
	mulUnitPreT<N>(xy, x, y);
	size_t n = bint::div(0, 0, xy, N + 1, p, N);
	bint::copyN(z, xy, n);
	bint::clearN(z + n, N - n);
}

// z[N] <- x[N * 2] % p[N]
template<size_t N>
static void fpDblModT(Unit *y, const Unit *x, const Unit *p)
{
	Unit t[N * 2];
	bint::copyN(t, x, N * 2);
	size_t n = bint::div(0, 0, t, N * 2, p, N);
	bint::copyN(y, t, n);
	bint::clearN(y + n, N - n);
}

//	z[N * 2] <- (x[N * 2] + y[N * 2]) mod p[N] << (N * UnitBitSize)
template<size_t N>
static void fpDblAddModT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	if (bint::addT<N * 2>(z, x, y)) {
		bint::subT<N>(z + N, z + N, p);
		return;
	}
	Unit tmp[N];
	if (bint::subT<N>(tmp, z + N, p) == 0) {
		bint::copyN(z + N, tmp, N);
	}
}

//	z[N * 2] <- (x[N * 2] - y[N * 2]) mod p[N] << (N * UnitBitSize)
template<size_t N>
static void fpDblSubModT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	if (bint::subT<N * 2>(z, x, y)) {
		bint::addT<N>(z + N, z + N, p);
	}
}

// [return:z[N+1]] = z[N+1] + x[N] * y + (CF << (N * UnitBitSize))
template<size_t N>
Unit mulUnitAddFullWithCF(Unit z[N + 1], const Unit x[N], Unit y, Unit CF)
{
	Unit H = bint::mulUnitAddT<N>(z, x, y);
	Unit v = z[N];
	v += H;
	Unit CF2 = v < H;
	v += CF;
	CF2 += v < CF;
	z[N] = v;
	return CF2;
}

/*
	z[N] <- montRed(xy[N * 2], p[N])
	REMARK : assume p[-1] = rp
*/
template<size_t N>
static void modRedT(Unit *z, const Unit *xy, const Unit *p)
{
	const Unit rp = p[-1];
	Unit buf[N * 2];
	bint::copyT<N * 2>(buf, xy);
	Unit CF = 0;
	for (size_t i = 0; i < N; i++) {
		Unit q = buf[i] * rp;
		CF = mulUnitAddFullWithCF<N>(buf + i, p, q, CF);
	}
	if (CF) {
		CF = bint::subT<N>(z, buf + N, p);
		assert(CF == 1);
		(void)CF;
	} else {
		if (bint::subT<N>(z, buf + N, p)) {
			bint::copyT<N>(z, buf + N);
		}
	}
}

// [return:z[N+1]] = z[N+1] + x[N] * y + (CF << (N * UnitBitSize))
template<size_t N>
Unit mulUnitAddWithCF(Unit z[N + 1], const Unit x[N], Unit y, Unit CF)
{
	Unit H = bint::mulUnitAddT<N>(z, x, y);
	H += CF;
	Unit v = z[N];
	v += H;
	z[N] = v;
	return v < H;
}

template<size_t N>
static void modRedNFT(Unit *z, const Unit *xy, const Unit *p)
{
	const Unit rp = p[-1];
	Unit buf[N * 2];
	bint::copyT<N * 2>(buf, xy);
	Unit CF = 0;
	for (size_t i = 0; i < N; i++) {
		Unit q = buf[i] * rp;
		CF = mulUnitAddWithCF<N>(buf + i, p, q, CF);
	}
	if (bint::subT<N>(z, buf + N, p)) {
		bint::copyT<N>(z, buf + N);
	}
}

// update z[N + 1]
template<size_t N>
static Unit mulUnitAddFull(Unit *z, const Unit *x, Unit y)
{
	Unit v1 = z[N];
	Unit v2 = v1 + bint::mulUnitAddT<N>(z, x, y);
	z[N] = v2;
	return v2 < v1;
}
/*
	z[N] <- Montgomery(x[N], y[N], p[N])
	REMARK : assume p[-1] = rp
*/
template<size_t N>
static void mulMontT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	const Unit rp = p[-1];
	Unit buf[N * 2 + 1];
	buf[N] = bint::mulUnitT<N>(buf, x, y[0]);
	Unit q = buf[0] * rp;
	buf[N + 1] = mulUnitAddFull<N>(buf, p, q);
	for (size_t i = 1; i < N; i++) {
		buf[N + 1 + i] = mulUnitAddFull<N>(buf + i, x, y[i]);
		q = buf[i] * rp;
		buf[N + 1 + i] += mulUnitAddFull<N>(buf + i, p, q);
	}
	if (buf[N + N]) {
		bint::subT<N>(z, buf + N, p);
	} else {
		if (bint::subT<N>(z, buf + N, p)) {
			bint::copyT<N>(z, buf + N);
		}
	}
}

template<size_t N>
static void mulMontNFT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	const Unit rp = p[-1];
	/*
		R = 1 << 64
		L % 64 = 63 ; not full bit
		F = 1 << (L + 1)
		max p = (1 << L) - 1
		x, y <= p - 1
		max x * y[0], p * q <= ((1 << L) - 1)(R - 1)
		t = x * y[i] + p * q <= 2((1 << L) - 1)(R - 1) = (F - 2)(R - 1)
		t >> 64 <= (F - 2)(R - 1)/R = (F - 2) - (F - 2)/R
			t + (t >> 64) = (F - 2)R - (F - 2)/R < FR
	*/
	Unit buf[N * 2];
	buf[N] = bint::mulUnitT<N>(buf, x, y[0]);
	Unit q = buf[0] * rp;
	buf[N] += bint::mulUnitAddT<N>(buf, p, q);
	for (size_t i = 1; i < N; i++) {
		buf[N + i] = bint::mulUnitAddT<N>(buf + i, x, y[i]);
		q = buf[i] * rp;
		buf[N + i] += bint::mulUnitAddT<N>(buf + i, p, q);
	}
	if (bint::subT<N>(z, buf + N, p)) {
		bint::copyT<N>(z, buf + N);
	}
}

// z[N] <- Montgomery(x[N], x[N], p[N])
template<size_t N>
static void sqrMontT(Unit *y, const Unit *x, const Unit *p)
{
	mulMontT<N>(y, x, x, p);
}

template<size_t N>
static void sqrMontNFT(Unit *y, const Unit *x, const Unit *p)
{
	mulMontNFT<N>(y, x, x, p);
}

// z[N] <- (x[N] * y[N]) % p[N]
template<size_t N>
static void mulModT(Unit *z, const Unit *x, const Unit *y, const Unit *p)
{
	Unit xy[N * 2];
	bint::mulT<N>(xy, x, y);
	fpDblModT<N>(z, xy, p);
}

// y[N] <- (x[N] * x[N]) % p[N]
template<size_t N>
static void sqrModT(Unit *y, const Unit *x, const Unit *p)
{
	Unit xx[N * 2];
	bint::sqrT<N>(xx, x);
	fpDblModT<N>(y, xx, p);
}

} } // mcl::fp

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
