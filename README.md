# Candidate Boost Bloom Library

(Candidate) Boost.Bloom provides the class template `boost::bloom::filter` that
can be configured to implement a classical [Bloom filter](https://en.wikipedia.org/wiki/Bloom_filter)
as well as variations discussed in the literature such as _blocked_ filters, _split block_/_multi-block_
filters, and more.

## Example

```cpp
#include <boost/bloom/filter.hpp>
#include <cassert>
#include <string>

int main()
{
  // Bloom filter of strings with 5 bits set per insertion
  using filter = boost::bloom::filter<std::string, 5>;

  // create filter with a capacity of 1'000'000 **bits**
  filter f(1'000'000);

  // insert elements (they can't be erased, Bloom filters are insert-only)
  f.insert("hello");
  f.insert("Boost");
  //...

  // elements inserted are always correctly checked as such
  assert(f.may_contain("hello") == true);

  // elements not inserted may incorrectly be identified as such with a
  // false positive rate (FPR) which is a function of the array capacity,
  // the number of bits set per element and generally how the boost::bloom::filter
  // was specified
  if(f.may_contain("bye")) { // likely false
    //...
  }
}
```

## Filter definition

A `boost::bloom::filter` can be regarded as an array of _buckets_ selected pseudo-randomly
(based on a hash function) upon insertion: each of the buckets is passed to a _subfilter_
that marks one or more of its bits according to some associated strategy.

```cpp
template<
  typename T, std::size_t K,
  typename Subfilter = block<unsigned char, 1>, std::size_t BucketSize = 0,
  typename Hash = boost::hash<T>, typename Allocator = std::allocator<T>  
>
class filter;
```

* `T`: type of the elements inserted.
* `K` number of buckets marked per insertion.
* `Subfilter`: type of subfilter used (more on this later).
* `BucketSize`: the number of buckets is just the capacity of the underlying
array (in bytes), divided by `BucketSize`. When `BucketSize` is specified as zero,
the value `sizeof(Subfilter::value_type)` is used.

The default configuration with `block<unsigned char,1>` corresponds to a
classical Bloom filter setting `K` bits per elements uniformly distributed across
the array.

### Overlapping buckets

`BucketSize` can be any value other (and typically less) than `sizeof(Subfilter::value_type)`.
When this is the case, subfilters in adjacent buckets act on overlapping byte ranges.
Far from being a problem, this improves the resulting _false positive rate_ (FPR) of
the filter. The downside is that buckets won't be in general properly aligned in memory,
which may result in more cache misses.

## Provided subfilters

### `block<Block, K'>`

Sets `K'` bits in an underlying value of the unsigned integral type `Block`
(e.g. `unsigned char`, `uint32_t`, `uint64_t`). So,
a `filter<T, K, block<Block, K'>>` will set `K*K'` bits per element.
The tradeoff here is that insertion/lookup will be (much) faster than
with `filter<T, K*K'>` while the FPR will be worse (larger).
FPR is better the wider `Block` is.

### `multiblock<Block, K'>`

Instead of setting `K'` bits in a `Block` value, this subfilter sets
one bit on each of the elements of a `Block[K']` subarray. This improves FPR
but impacts performance with respect to `block<Block, K'>`, among other
things because cacheline boundaries can be crossed when accessing the subarray.

### `fast_multiblock32<K'>`

Statistically equivalent to `multiblock<uint32_t, K'>`, but uses
faster SIMD-based algorithms when SSE2, AVX2 or Neon are available. The approach is
similar to that of [Apache Kudu](https://kudu.apache.org/)
[`BlockBloomFilter`](https://github.com/apache/kudu/blob/master/src/kudu/util/block_bloom_filter_avx2.cc),
but that implementation is fixed to `K'` = 8 whereas we accept any value.

### `fast_multiblock64<K'>`

Statistically equivalent to `multiblock<uint64_t, K'>`, but uses a
faster SIMD-based algorithm when AVX2 is available.

## Estimating FPR

For a classical Bloom filter, the theoretical false positive rate, under some simplifying assumptions,
is given by

$$FPR(n,m,k)=\left(1 - \left(1 - \frac{1}{m}\right)^{kn}\right)^k \approx \left(1 - e^{-kn/m}\right)^k \text{   for large } m,$$

where $$n$$ is the number of elements inserted in the filter, $$m$$ its capacity in bits and $$k$$ the
number of bits set per insertion (see a [derivation](https://en.wikipedia.org/wiki/Bloom_filter#Probability_of_false_positives)
of this formula). For a given inverse load factor $$c=m/n$$, the optimum $$k$$ is
the integer closest to:

$$k_{\text{opt}}=c\cdot\ln2,$$

yielding a minimum attainable FPR of $$1/2^{k_{\text{opt}}} \approx 0.6185^{c}$$.

In the case of a Boost.Bloom block filter of the form `filter<T, K, block<Block, K'>>`, we can extend
the approach from [Putze et al.](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=f376ff09a64b388bfcde2f5353e9ddb44033aac8)
to derive the (approximate but very precise) formula:

$$FPR_{\text{block}}(n,m,b,k,k')=\left(\sum_{i=0}^{\infty} \text{Pois}(i,nbk/m) \cdot FPR(i,b,k')\right)^{k},$$

where

$$\text{Pois}(i,\lambda)=\frac{\lambda^i e^{-\lambda}}{i!}$$

is the probability mass function of a [Poisson distribution](https://en.wikipedia.org/wiki/Poisson_distribution)
with mean $$\lambda$$, and $$b$$ is the size of `Block` in bits. If we're using `multiblock<Block,K'>`, we have

$$FPR_\text{multiblock}(n,m,b,k,k')=\left(\sum_{i=0}^{\infty} \text{Pois}(i,nbkk'/m) \cdot FPR(i,b,1)^{k'}\right)^{k}.$$

As we have commented before, in general 

$$FPR_\text{block}(n,m,b,k,k') \geq FPR_\text{multiblock}(n,m,b,k,k') \geq FPR(n,m,kk'),$$

that is, block and multi-block filters have worse FPR than the classical filter for the same number of bits
set per insertion, but they will be much faster. We have the particular case

$$FPR_{\text{block}}(n,m,b,k,1)=FPR_{\text{multiblock}}(n,m,b,k,1)=FPR(n,m,k),$$

which follows simply from the observation that using `{block|multiblock}<Block, 1>` behaves exactly as
a classical Bloom filter.

We don't know of any closed, simple formula for the FPR of block and multiblock filters when
`Bucketsize` is not its "natural" size (`sizeof(Block)` for `block<Block, K'>`,
`K'*sizeof(Block)` for `multiblock<Block, K'>`), that is, when subfilter values overlap.
We can use the following appoximations ($$s$$ = `BucketSize` in bits):

$$FPR_{\text{block}}(n,m,b,s,k,k')=\left(\sum_{i=0}^{\infty} \text{Pois}\left(i,\frac{n(2b-s)k}{m}\right) \cdot FPR(i,2b-s,k')\right)^{k},$$
$$FPR_\text{multiblock}(n,m,b,s,k,k')=\left(\sum_{i=0}^{\infty} \text{Pois}\left(i,\frac{n(2bk'-s)k}{m}\right) \cdot FPR\left(i,\frac{2bk'-s}{k'},1\right)^{k'}\right)^{k},$$

where the replacement of $$b$$ with $$2b-s$$ (or $$bk'$$ with $$2bk'-s$$ for multiblock filters) accounts
for the fact that the window of hashing positions affecting a particular bit spreads due to
overlapping. Note that the formulas reduce to the non-ovelapping case when $$s$$ takes its
default value ($$b$$ for block, $$bk´$$ for multiblock). These approximations are acceptable for
low values of $$k'$$ but tend to underestimate the actual FPR as $$k'$$ grows.
In general, the use of overlapping improves (decreases) FPR by a factor ranging from
0.6 to 0.9 for typical filter configurations.

## Experimental results

All benchmarks run on a Windows 10 machine with 8GB RAM and an Intel Core i5-8265U CPU @1.60GHz
(base frequency) using Clang 13.0.1 for Visual Studio (clang-cl), `/arch:AVX2`,
release mode (see [benchmarking code](benchmark/comparison_table.cpp)).

The tables show the FPR and execution times in nanoseconds per operation 
for six different configurations of `boost::bloom::filter<int, ...>`
where `N` elements have been inserted. Filters are constructed with a capacity
`c*N` (bits), so `c` is the number of bits used per element. For each combination of `c` and
a given filter configuration, the optimum value of `K` (that yielding the minimum FPR)
has been used.

For reference, we provide also insertion, successful lookup and unsuccessful lookup times
for a `boost::unordered_flat_set<int>` with the same number of elements `N`.

### 64-bit mode

#### `N` = 1M elements

The `boost::unordered_flat_set` container uses 79.69 bits per element.


<table>
  <tr><th colspan="3"><code>boost::unordered_flat_set</code></tr>
  <tr>
    <th>insertion</th>
    <th>successful<br/>lookup</th>
    <th>unsuccessful<br/>lookup</th>
  </tr>
  <tr>
    <td align="right">29.97</td>
    <td align="right">16.25</td>
    <td align="right">4.99</td>
  </tr>
</table>
<table>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t,K>></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">6</td>
    <td align="right">2.1635</td>
    <td align="right">10.95</td>
    <td align="right">9.95</td>
    <td align="right">16.28</td>
    <td align="center">4</td>
    <td align="right">3.3525</td>
    <td align="right">3.43</td>
    <td align="right">3.17</td>
    <td align="right">3.18</td>
    <td align="center">5</td>
    <td align="right">2.4274</td>
    <td align="right">3.63</td>
    <td align="right">4.54</td>
    <td align="right">4.56</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">9</td>
    <td align="right">0.3151</td>
    <td align="right">18.58</td>
    <td align="right">14.25</td>
    <td align="right">16.22</td>
    <td align="center">5</td>
    <td align="right">1.0369</td>
    <td align="right">3.76</td>
    <td align="right">3.49</td>
    <td align="right">3.52</td>
    <td align="center">8</td>
    <td align="right">0.4244</td>
    <td align="right">3.59</td>
    <td align="right">4.80</td>
    <td align="right">4.85</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.0455</td>
    <td align="right">21.14</td>
    <td align="right">17.38</td>
    <td align="right">14.96</td>
    <td align="center">6</td>
    <td align="right">0.4138</td>
    <td align="right">4.13</td>
    <td align="right">3.74</td>
    <td align="right">3.73</td>
    <td align="center">11</td>
    <td align="right">0.0782</td>
    <td align="right">6.18</td>
    <td align="right">7.46</td>
    <td align="right">7.12</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0084</td>
    <td align="right">26.97</td>
    <td align="right">22.71</td>
    <td align="right">16.73</td>
    <td align="center">7</td>
    <td align="right">0.1936</td>
    <td align="right">4.56</td>
    <td align="right">3.99</td>
    <td align="right">3.96</td>
    <td align="center">14</td>
    <td align="right">0.0163</td>
    <td align="right">6.73</td>
    <td align="right">9.15</td>
    <td align="right">9.17</td>
  </tr>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K,fast_multiblock32&ltK>></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t, K>,1></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>,1></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">5</td>
    <td align="right">2.7392</td>
    <td align="right">7.95</td>
    <td align="right">7.19</td>
    <td align="right">7.26</td>
    <td align="center">5</td>
    <td align="right">3.0417</td>
    <td align="right">3.46</td>
    <td align="right">3.32</td>
    <td align="right">3.30</td>
    <td align="center">5</td>
    <td align="right">2.3163</td>
    <td align="right">3.43</td>
    <td align="right">4.49</td>
    <td align="right">4.57</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">8</td>
    <td align="right">0.5434</td>
    <td align="right">2.90</td>
    <td align="right">2.74</td>
    <td align="right">2.75</td>
    <td align="center">6</td>
    <td align="right">0.8174</td>
    <td align="right">3.78</td>
    <td align="right">3.50</td>
    <td align="right">3.48</td>
    <td align="center">8</td>
    <td align="right">0.3750</td>
    <td align="right">4.05</td>
    <td align="right">4.88</td>
    <td align="right">4.92</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.1212</td>
    <td align="right">8.58</td>
    <td align="right">7.68</td>
    <td align="right">4.08</td>
    <td align="center">7</td>
    <td align="right">0.2812</td>
    <td align="right">4.27</td>
    <td align="right">3.90</td>
    <td align="right">3.77</td>
    <td align="center">11</td>
    <td align="right">0.0681</td>
    <td align="right">6.15</td>
    <td align="right">7.35</td>
    <td align="right">7.35</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0287</td>
    <td align="right">5.15</td>
    <td align="right">6.03</td>
    <td align="right">3.73</td>
    <td align="center">8</td>
    <td align="right">0.1156</td>
    <td align="right">4.79</td>
    <td align="right">3.77</td>
    <td align="right">3.78</td>
    <td align="center">14</td>
    <td align="right">0.0100</td>
    <td align="right">6.67</td>
    <td align="right">8.86</td>
    <td align="right">8.90</td>
  </tr>
</table>

#### `N` = 10M elements

The `boost::unordered_flat_set` container uses 63.75 bits per element.

<table>
  <tr><th colspan="3"><code>boost::unordered_flat_set</code></tr>
  <tr>
    <th>insertion</th>
    <th>successful<br/>lookup</th>
    <th>unsuccessful<br/>lookup</th>
  </tr>
  <tr>
    <td align="right">76.08</td>
    <td align="right">32.90</td>
    <td align="right">16.31</td>
  </tr>
</table>
<table>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t,K>></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">6</td>
    <td align="right">2.1566</td>
    <td align="right">57.43</td>
    <td align="right">39.09</td>
    <td align="right">29.60</td>
    <td align="center">4</td>
    <td align="right">3.3462</td>
    <td align="right">13.27</td>
    <td align="right">10.46</td>
    <td align="right">10.42</td>
    <td align="center">5</td>
    <td align="right">2.4515</td>
    <td align="right">16.04</td>
    <td align="right">13.42</td>
    <td align="right">13.48</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">9</td>
    <td align="right">0.3146</td>
    <td align="right">113.52</td>
    <td align="right">68.16</td>
    <td align="right">36.52</td>
    <td align="center">5</td>
    <td align="right">1.0310</td>
    <td align="right">17.22</td>
    <td align="right">12.84</td>
    <td align="right">12.85</td>
    <td align="center">8</td>
    <td align="right">0.4244</td>
    <td align="right">16.77</td>
    <td align="right">14.61</td>
    <td align="right">14.75</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.0456</td>
    <td align="right">155.65</td>
    <td align="right">93.55</td>
    <td align="right">37.27</td>
    <td align="center">6</td>
    <td align="right">0.4035</td>
    <td align="right">19.37</td>
    <td align="right">14.41</td>
    <td align="right">14.40</td>
    <td align="center">11</td>
    <td align="right">0.0776</td>
    <td align="right">33.05</td>
    <td align="right">24.51</td>
    <td align="right">24.55</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0066</td>
    <td align="right">212.09</td>
    <td align="right">128.59</td>
    <td align="right">41.87</td>
    <td align="center">7</td>
    <td align="right">0.1879</td>
    <td align="right">20.85</td>
    <td align="right">15.03</td>
    <td align="right">15.30</td>
    <td align="center">14</td>
    <td align="right">0.0153</td>
    <td align="right">37.16</td>
    <td align="right">30.96</td>
    <td align="right">31.03</td>
  </tr>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K,fast_multiblock32&ltK>></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t, K>,1></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>,1></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">5</td>
    <td align="right">2.7367</td>
    <td align="right">15.95</td>
    <td align="right">12.30</td>
    <td align="right">12.23</td>
    <td align="center">5</td>
    <td align="right">3.0580</td>
    <td align="right">15.07</td>
    <td align="right">11.77</td>
    <td align="right">11.95</td>
    <td align="center">5</td>
    <td align="right">2.3256</td>
    <td align="right">15.88</td>
    <td align="right">13.14</td>
    <td align="right">13.29</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">8</td>
    <td align="right">0.5399</td>
    <td align="right">15.02</td>
    <td align="right">11.46</td>
    <td align="right">11.40</td>
    <td align="center">6</td>
    <td align="right">0.8253</td>
    <td align="right">19.80</td>
    <td align="right">14.82</td>
    <td align="right">14.85</td>
    <td align="center">8</td>
    <td align="right">0.3777</td>
    <td align="right">22.72</td>
    <td align="right">15.83</td>
    <td align="right">15.82</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.1170</td>
    <td align="right">26.48</td>
    <td align="right">20.64</td>
    <td align="right">17.34</td>
    <td align="center">7</td>
    <td align="right">0.2891</td>
    <td align="right">21.43</td>
    <td align="right">16.47</td>
    <td align="right">16.46</td>
    <td align="center">11</td>
    <td align="right">0.0643</td>
    <td align="right">34.25</td>
    <td align="right">25.10</td>
    <td align="right">24.72</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0276</td>
    <td align="right">27.59</td>
    <td align="right">21.11</td>
    <td align="right">15.72</td>
    <td align="center">8</td>
    <td align="right">0.1213</td>
    <td align="right">23.87</td>
    <td align="right">16.50</td>
    <td align="right">16.47</td>
    <td align="center">14</td>
    <td align="right">0.0119</td>
    <td align="right">38.08</td>
    <td align="right">31.57</td>
    <td align="right">31.83</td>
  </tr>
</table>

### 32-bit mode

#### `N` = 1M elements

The `boost::unordered_flat_set` container uses 79.69 bits per element.

<table>
  <tr><th colspan="3"><code>boost::unordered_flat_set</code></tr>
  <tr>
    <th>insertion</th>
    <th>successful<br/>lookup</th>
    <th>unsuccessful<br/>lookup</th>
  </tr>
  <tr>
    <td align="right">34.08</td>
    <td align="right">18.32</td>
    <td align="right">5.34</td>
  </tr>
</table>
<table>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t,K>></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">6</td>
    <td align="right">2.1546</td>
    <td align="right">23.20</td>
    <td align="right">22.80</td>
    <td align="right">25.90</td>
    <td align="center">4</td>
    <td align="right">3.3200</td>
    <td align="right">10.26</td>
    <td align="right">6.69</td>
    <td align="right">6.73</td>
    <td align="center">5</td>
    <td align="right">2.4144</td>
    <td align="right">6.98</td>
    <td align="right">8.85</td>
    <td align="right">8.76</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">9</td>
    <td align="right">0.3118</td>
    <td align="right">25.90</td>
    <td align="right">34.39</td>
    <td align="right">25.99</td>
    <td align="center">5</td>
    <td align="right">1.0423</td>
    <td align="right">11.53</td>
    <td align="right">8.35</td>
    <td align="right">8.47</td>
    <td align="center">8</td>
    <td align="right">0.4200</td>
    <td align="right">6.28</td>
    <td align="right">10.55</td>
    <td align="right">10.56</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.0453</td>
    <td align="right">31.76</td>
    <td align="right">41.61</td>
    <td align="right">24.98</td>
    <td align="center">6</td>
    <td align="right">0.3972</td>
    <td align="right">13.20</td>
    <td align="right">11.15</td>
    <td align="right">11.27</td>
    <td align="center">11</td>
    <td align="right">0.0783</td>
    <td align="right">13.68</td>
    <td align="right">17.24</td>
    <td align="right">17.20</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0062</td>
    <td align="right">40.04</td>
    <td align="right">50.53</td>
    <td align="right">24.49</td>
    <td align="center">7</td>
    <td align="right">0.1886</td>
    <td align="right">14.40</td>
    <td align="right">12.32</td>
    <td align="right">12.42</td>
    <td align="center">14</td>
    <td align="right">0.0149</td>
    <td align="right">13.00</td>
    <td align="right">20.51</td>
    <td align="right">20.41</td>
  </tr>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K,fast_multiblock32&ltK>></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t, K>,1></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>,1></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">5</td>
    <td align="right">2.7556</td>
    <td align="right">9.01</td>
    <td align="right">9.89</td>
    <td align="right">9.86</td>
    <td align="center">5</td>
    <td align="right">3.0416</td>
    <td align="right">11.54</td>
    <td align="right">8.48</td>
    <td align="right">8.39</td>
    <td align="center">5</td>
    <td align="right">2.3251</td>
    <td align="right">6.21</td>
    <td align="right">8.48</td>
    <td align="right">8.50</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">8</td>
    <td align="right">0.5414</td>
    <td align="right">4.48</td>
    <td align="right">6.20</td>
    <td align="right">6.13</td>
    <td align="center">6</td>
    <td align="right">0.8365</td>
    <td align="right">12.76</td>
    <td align="right">10.19</td>
    <td align="right">10.19</td>
    <td align="center">8</td>
    <td align="right">0.3823</td>
    <td align="right">6.40</td>
    <td align="right">9.11</td>
    <td align="right">9.22</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.1201</td>
    <td align="right">12.60</td>
    <td align="right">12.64</td>
    <td align="right">7.88</td>
    <td align="center">7</td>
    <td align="right">0.3024</td>
    <td align="right">13.65</td>
    <td align="right">12.33</td>
    <td align="right">12.34</td>
    <td align="center">11</td>
    <td align="right">0.0671</td>
    <td align="right">12.78</td>
    <td align="right">16.48</td>
    <td align="right">16.52</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0291</td>
    <td align="right">9.55</td>
    <td align="right">12.11</td>
    <td align="right">7.37</td>
    <td align="center">8</td>
    <td align="right">0.1199</td>
    <td align="right">15.36</td>
    <td align="right">8.73</td>
    <td align="right">8.78</td>
    <td align="center">14</td>
    <td align="right">0.0091</td>
    <td align="right">12.38</td>
    <td align="right">19.75</td>
    <td align="right">19.67</td>
  </tr>
</table>

#### `N` = 10M elements

The `boost::unordered_flat_set` container uses 63.75 bits per element.

<table>
  <tr><th colspan="3"><code>boost::unordered_flat_set</code></tr>
  <tr>
    <th>insertion</th>
    <th>successful<br/>lookup</th>
    <th>unsuccessful<br/>lookup</th>
  </tr>
  <tr>
    <td align="right">78.63</td>
    <td align="right">38.49</td>
    <td align="right">17.33</td>
  </tr>
</table>
<table>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t,K>></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">6</td>
    <td align="right">2.1489</td>
    <td align="right">85.90</td>
    <td align="right">81.33</td>
    <td align="right">48.20</td>
    <td align="center">4</td>
    <td align="right">3.3441</td>
    <td align="right">32.08</td>
    <td align="right">19.32</td>
    <td align="right">19.47</td>
    <td align="center">5</td>
    <td align="right">2.4525</td>
    <td align="right">24.20</td>
    <td align="right">22.54</td>
    <td align="right">22.56</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">9</td>
    <td align="right">0.3184</td>
    <td align="right">137.07</td>
    <td align="right">134.16</td>
    <td align="right">59.73</td>
    <td align="center">5</td>
    <td align="right">1.0317</td>
    <td align="right">38.20</td>
    <td align="right">27.28</td>
    <td align="right">27.27</td>
    <td align="center">8</td>
    <td align="right">0.4209</td>
    <td align="right">22.92</td>
    <td align="right">28.81</td>
    <td align="right">28.68</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.0455</td>
    <td align="right">183.98</td>
    <td align="right">177.12</td>
    <td align="right">60.84</td>
    <td align="center">6</td>
    <td align="right">0.4015</td>
    <td align="right">42.75</td>
    <td align="right">28.83</td>
    <td align="right">29.70</td>
    <td align="center">11</td>
    <td align="right">0.0783</td>
    <td align="right">52.50</td>
    <td align="right">49.11</td>
    <td align="right">48.99</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0068</td>
    <td align="right">246.14</td>
    <td align="right">230.24</td>
    <td align="right">64.08</td>
    <td align="center">7</td>
    <td align="right">0.1883</td>
    <td align="right">46.26</td>
    <td align="right">33.50</td>
    <td align="right">33.37</td>
    <td align="center">14</td>
    <td align="right">0.0158</td>
    <td align="right">52.51</td>
    <td align="right">54.27</td>
    <td align="right">54.89</td>
  </tr>
  <tr>
    <th></th>
    <th colspan="5"><code>filter&lt;K,fast_multiblock32&ltK>></code></th>
    <th colspan="5"><code>filter&lt;1,block&lt;uint64_t, K>,1></code></th>
    <th colspan="5"><code>filter&lt;1,multiblock&lt;uint64_t,K>,1></code></th>
  </tr>
  <tr>
    <th>c</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
    <th>K</th>
    <th>FPR<br/>[%]</th>
    <th>ins.</th>
    <th>succ.<br/>lkp.</th>
    <th>uns.<br/>lkp.</th>
  </tr>
  <tr>
    <td align="center">8</td>
    <td align="center">5</td>
    <td align="right">2.7383</td>
    <td align="right">21.18</td>
    <td align="right">21.74</td>
    <td align="right">21.38</td>
    <td align="center">5</td>
    <td align="right">3.0456</td>
    <td align="right">32.69</td>
    <td align="right">23.15</td>
    <td align="right">23.22</td>
    <td align="center">5</td>
    <td align="right">2.3196</td>
    <td align="right">21.79</td>
    <td align="right">23.25</td>
    <td align="right">23.14</td>
  </tr>
  <tr>
    <td align="center">12</td>
    <td align="center">8</td>
    <td align="right">0.5398</td>
    <td align="right">19.13</td>
    <td align="right">21.88</td>
    <td align="right">21.87</td>
    <td align="center">6</td>
    <td align="right">0.8314</td>
    <td align="right">40.98</td>
    <td align="right">28.54</td>
    <td align="right">28.38</td>
    <td align="center">8</td>
    <td align="right">0.3752</td>
    <td align="right">26.84</td>
    <td align="right">30.85</td>
    <td align="right">30.51</td>
  </tr>
  <tr>
    <td align="center">16</td>
    <td align="center">11</td>
    <td align="right">0.1192</td>
    <td align="right">39.34</td>
    <td align="right">36.91</td>
    <td align="right">26.92</td>
    <td align="center">7</td>
    <td align="right">0.2897</td>
    <td align="right">45.32</td>
    <td align="right">33.73</td>
    <td align="right">33.60</td>
    <td align="center">11</td>
    <td align="right">0.0629</td>
    <td align="right">52.77</td>
    <td align="right">48.75</td>
    <td align="right">48.19</td>
  </tr>
  <tr>
    <td align="center">20</td>
    <td align="center">14</td>
    <td align="right">0.0275</td>
    <td align="right">38.61</td>
    <td align="right">41.07</td>
    <td align="right">25.72</td>
    <td align="center">8</td>
    <td align="right">0.1216</td>
    <td align="right">49.14</td>
    <td align="right">31.42</td>
    <td align="right">31.29</td>
    <td align="center">14</td>
    <td align="right">0.0118</td>
    <td align="right">55.41</td>
    <td align="right">55.42</td>
    <td align="right">54.70</td>
  </tr>
</table>

