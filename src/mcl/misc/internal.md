# Internal Algorithm of mulVec with AVX-512

# bencmark

```
make bin/mt_test.exe CFLAGS_USER=-DCYBOZU_BENCH_USE_GETTIMEOFDAY
bin/mt_test.exe -g1 -n num
```

mulVec on Xeon w9-3495X (turbo boost on) with AVX-512 IFMA

unit : msec
n|8192|16384|32768|65536
-|-|-|-|-
w/o IFMA|66.498|122.666|227.042|426.498
w IFMA|46.411|87.002|153.958|300.331
speed up rate|1.43|1.41|1.47|1.42

G1 mulEach
- w/o IFMA : 42.166Mclk
- w IFMA : 16.643Mclk

# GLV method

## Split function for BLS12-381

### Definition of parameters

```python
M = 1<<256
H = 1<<128
z = -0xd201000000010000
L = z*z - 1
r = L*L + L + 1
s = r.bit_length()
S = 1<<s
v = S // L
r0 = S % L

adj = False
def split(x):
    b = (x * v) >> s
    a = x - b * L
    if adj:
      if a >= L:
        a -= L
        b += 1
    return (a, b)
```

variables|z|L|r|S|v
-|-|-|-|-|-
bit_length|64|128|255|255|128

- x in [0, r-1]
- a + b L = x for (a, b) = split(x).

### Theorem
0 <= a < 1.11 L < H and 0 <= b < L+1 for x in [0, r-1].

### Proof

```
Let r0 := L S % r, then S=v L + r0 and r0 in [0, L-1]. In fact, r0 ~ 0.11 L.
Let r1 := x v % S, then x v = b S + r1 and r1 in [0, S-1].
```

```
b <=  xv / S < (r-1) (S/L)/S = (r-1)/L = L+1.
```

```
aS = (x - bL)S = xS - bSL = xS - (xv - r1)L = x(S - vL) + r1 L = r0 x + r1 L
     <= r0 (r-1) + (S-1)L = S L + (r-1)r0 - L.
a <= L + ((r-1)r0 - L)/S
((r-1)r0 - L)/S ~ 0.10016 L < 0.11 L.
```
### Remark
If adj is true, then a is in [0, L-1].


## window size
- 128-bit (Fr is 256 bit and use GLV method)
- w-bit window size

```python
def f(w):
  return 2**w+(128+w-1)//w
```

w|1|2|3|4|5|6
-|-|-|-|-|-|-
f(w)|130|68|51|48|58|86

argmin f(w) = 4

## Selection of coordinates

### psuedo code of GLV method

```python
def mul(P, x):
  assert(0 <= x < r)
  (a, b) = split(x) # x = a + b L
  # a, b < H=1<<128
  w = 4
  for i in range(1<<w):
    tbl1[i] = P * i
    tbl2[i] = mulLamba(tbl1[i])

  mask = (1<<w)-1
  Q = 0
  for i in range(128//w):
    for j in range(w):
      Q = dbl(Q)         ### AAA
    j1 = (a >> (w*i)) & mask
    j2 = (b >> (w*i)) & mask
    Q = add(Q, tbl2[j2]) # ADD1
    Q = add(Q, tbl1[j1]) # ADD2
  return Q
```
Note that the value of tbl2 is added first.

### Theorem
We can use Jacobi additive formula add(P, Q) assuming P != Q and P+Q != 0.

Proof.

During the calculation, Q is monotonic increase and always in [0, P, ..., (r-1)P].

- ADD1 : tbl2[] is in [0, L P, ..., 15 L P] and L is odd.
After computing AAA, Q is a multiple of 16 P, so Q != tbl2[j2].
- ADD2 : tbl1[] is in [0, P, ..., 15 P].
After computing ADD1, if the immediately preceding tbl2[j2] is 0, then then Q is a multiple of 16 P, so Q != tbl1[j1].
Otherwise, Q is bigger than L P, so Q != tbl1[j1].

## Jacobi and Proj
`sqr` is equal to `mul` on AVX-512.

-|add|dbl
-|-|-
Proj|12M+27A|8M+13A
Jacobi|16M+7A|7M+12A

## NAF (Non-Adjacent Form)

```
def naf(x, w=3):
  tbl = []
  H=2**(w-1)
  W=H*2
  mask = W-1
  while x >= 1:
    if x & 1:
      t = x & mask
      if t >= H:
        t -= W
      x = x - t
    else:
      t = 0
    x = x >> 1
    tbl.append(t)
    tbl.reverse()
  return tbl
```

Consider to apply `w=5` to `(a, b)=split(x)`.
The max value of `a` is `1.1 L = 0b101...` of 128-bit length.
`0b101` is less than `(1<<(w-1))-1` and so negativity and CF operation are unnecessary.
