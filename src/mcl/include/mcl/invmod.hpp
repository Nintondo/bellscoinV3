#pragma once
/**
	@file
	@brief non constant time invMod by safegcd
	@author MITSUNARI Shigeo(@herumi)
	cf. The original code is https://github.com/bitcoin-core/secp256k1/blob/master/doc/safegcd_implementation.md
	It is offered under the MIT license.
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/

#include <mcl/gmp_util.hpp>
#include <mcl/bint.hpp>
#include <cybozu/bit_operation.hpp>
#include <mcl/invmod_fwd.hpp>
#include <mcl/util.hpp>

namespace mcl {

namespace inv {

struct Quad {
	Unit u, v, q, r;
};

template<int N>
void _add(SintT<N>& z, const SintT<N>& x, const Unit *y, bool ySign)
{
	if (x.sign == ySign) {
		Unit ret = mcl::bint::addT<N>(z.v, x.v, y);
		(void)ret;
		assert(ret == 0);
		z.sign = x.sign;
		return;
	}
	int r = mcl::bint::cmpT<N>(x.v, y);
	if (r >= 0) {
		mcl::bint::subT<N>(z.v, x.v, y);
		z.sign = x.sign;
		return;
	}
	mcl::bint::subT<N>(z.v, y, x.v);
	z.sign = ySign;
}

template<int N>
void set(SintT<N>& y, const Unit *x, bool sign)
{
	mcl::bint::copyT<N>(y.v, x);
	y.sign = sign;
}

template<int N>
void clear(SintT<N>& x)
{
	x.sign = false;
	mcl::bint::clearT<N>(x.v);
}

template<int N>
bool isZero(const SintT<N>& x)
{
	Unit r = x.v[0];
	for (int i = 1; i < N; i++) r |= x.v[i];
	return r == 0;
}

template<int N>
void add(SintT<N>& z, const SintT<N>& x, const SintT<N>& y)
{
	_add(z, x, y.v, y.sign);
}

template<int N>
void sub(SintT<N>& z, const SintT<N>& x, const SintT<N>& y)
{
	_add(z, x, y.v, !y.sign);
}

template<int N>
void mulUnit(SintT<N+1>&z, const SintT<N>& x, INT y)
{
	Unit abs_y = y < 0 ? -y : y;
	z.v[N] = mcl::bint::mulUnitT<N>(z.v, x.v, abs_y);
	z.sign = x.sign ^ (y < 0);
}

template<int N>
void shr(SintT<N>& y, int x)
{
	mcl::bint::shrT<N>(y.v, y.v, x);
}

template<int N>
Unit getLow(const SintT<N>& x)
{
	Unit r = x.v[0];
	if (x.sign) r = -r;
	return r;
}

template<int N>
Unit getLowMask(const SintT<N>& x)
{
	Unit r = getLow(x);
	return r & MASK;
}

template<int N2>
void toSint(SintT<N2>& y, const mpz_class& x)
{
	const size_t n = mcl::gmp::getUnitSize(x);
	const Unit *p = mcl::gmp::getUnit(x);
	for (size_t i = 0; i < n; i++) {
		y.v[i] = p[i];
	}
	for (size_t i = n; i < N2; i++) y.v[i] = 0;
	y.sign = x < 0;
}
template<int N2>
void toMpz(mpz_class& y, const SintT<N2>& x)
{
	mcl::gmp::setArray(y, x.v, N2);
	if (x.sign) y = -y;
}

static inline INT divsteps_n_matrix(Quad& t, INT eta, Unit f, Unit g)
{
	static const uint32_t tbl[] = { 15, 5, 3, 9, 7, 13, 11, 1 };
	Unit u = 1, v = 0, q = 0, r = 1;
	int i = modL;
	for (;;) {
		INT zeros = g == 0 ? i : cybozu::bsf(g);
		if (i < zeros) zeros = i;
		eta -= zeros;
		i -= zeros;
		g >>= zeros;
		u <<= zeros;
		v <<= zeros;
		if (i == 0) break;
		if (eta < 0) {
			Unit u0 = u;
			Unit v0 = v;
			Unit f0 = f;
			eta = -eta;
			f = g;
			u = q;
			v = r;
			g = -f0;
			q = -u0;
			r = -v0;
		}
		int limit = mcl::fp::min_<INT>(mcl::fp::min_<INT>(eta + 1, i), 4);
		Unit w = (g * tbl[(f & 15)>>1]) & ((1u<<limit)-1);
		g += w * f;
		q += w * u;
		r += w * v;
	}
	t.u = u;
	t.v = v;
	t.q = q;
	t.r = r;
	return eta;
}

template<int N>
void update_fg(SintT<N>& f, SintT<N>& g, const Quad& t)
{
	SintT<N+1> f1, f2, g1, g2;
	mulUnit(f1, f, t.u);
	mulUnit(f2, f, t.q);
	mulUnit(g1, g, t.v);
	mulUnit(g2, g, t.r);
	add(f1, f1, g1);
	add(g1, f2, g2);
	shr(f1, modL);
	shr(g1, modL);
	assert(f1.v[N] == 0);
	assert(g1.v[N] == 0);
	set(f, f1.v, f1.sign);
	set(g, g1.v, g1.sign);
}

template<int N>
void update_de(const InvModT<N>& im, SintT<N>& d, SintT<N>& e, const Quad& t)
{
	const SintT<N>& M = im.M;
	const INT Mi = im.Mi;
	Unit ud = 0;
	Unit ue = 0;
	if (d.sign) {
		ud = t.u;
		ue = t.q;
	}
	if (e.sign) {
		ud += t.v;
		ue += t.r;
	}
	SintT<N+1> d1, d2, e1, e2;
	// d = d * u + e * v
	// e = d * q + e * r
	mulUnit(d1, d, t.u);
	mulUnit(d2, d, t.q);
	mulUnit(e1, e, t.v);
	mulUnit(e2, e, t.r);
	add(d1, d1, e1);
	add(e1, d2, e2);
	Unit di = getLow(d1) + im.lowM * ud;
	Unit ei = getLow(e1) + im.lowM * ue;
	ud -= Mi * di;
	ue -= Mi * ei;
	INT sd = ud & MASK;
	INT se = ue & MASK;
	if (sd >= half) sd -= modN;
	if (se >= half) se -= modN;
	// d = (d + M * sd) >> modL
	// e = (e + M * se) >> modL
	mulUnit(d2, M, sd);
	mulUnit(e2, M, se);
	add(d1, d1, d2);
	add(e1, e1, e2);
	shr(d1, modL);
	shr(e1, modL);
	assert(d1.v[N] == 0);
	assert(e1.v[N] == 0);
	set(d, d1.v, d1.sign);
	set(e, e1.v, e1.sign);
}

template<int N>
void normalize(const InvModT<N>& im, SintT<N>& v, bool minus)
{
	const SintT<N>& M = im.M;
	if (v.sign) {
		add(v, v, M);
	}
	if (minus) {
		sub(v, M, v);
	}
	if (v.sign) {
		add(v, v, M);
	}
}

template<int N>
void exec(const InvModT<N>& im, Unit *py, const Unit *px)
{
	INT eta = -1;
	SintT<N> f = im.M, g, d, e;
	set(g, px, false);

	clear(d);
	clear(e); e.v[0] = 1;
	Quad t;
	while (!isZero(g)) {
		Unit fLow = getLowMask(f);
		Unit gLow = getLowMask(g);
		eta = divsteps_n_matrix(t, eta, fLow, gLow);
		update_fg(f, g, t);
		update_de(im, d, e, t);
	}
	normalize(im, d, f.sign);
	mcl::bint::copyT<N>(py, d.v);
}

template<int N>
void exec(const InvModT<N>& im, mpz_class& y, const mpz_class& x)
{
	Unit ux[N], uy[N];
	mcl::gmp::getArray(ux, N, x);
	exec<N>(im, uy, ux);
	mcl::gmp::setArray(y, uy, N);
}

template<int N>
void init(InvModT<N>& invMod, const mpz_class& mM)
{
	toSint(invMod.M, mM);
	invMod.lowM = getLow(invMod.M);
	mpz_class inv;
	mpz_class mod = mpz_class(1) << modL;
	mcl::gmp::invMod(inv, mM, mod);
	invMod.Mi = mcl::gmp::getUnit(inv)[0] & MASK;
}

} // mcl::inv

} // mcl
