/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_FAST_MULTIBLOCK32_HPP
#define BOOST_BLOOM_FAST_MULTIBLOCK32_HPP

#include <boost/bloom/detail/avx2.hpp>
#include <boost/bloom/detail/neon.hpp>

#if defined(BOOST_BLOOM_AVX2)
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

namespace boost{
namespace bloom{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

template<std::size_t K>
struct fast_multiblock32
{
  static constexpr std::size_t k=K;
  using value_type=__m256i[(k+7)/8];
  static constexpr std::size_t used_value_size=sizeof(boost::uint32_t)*k;

  static BOOST_FORCEINLINE void mark(value_type& x,boost::uint64_t hash)
  {
    for(int i=0;i<k/8;++i){
      mark_m256i(x[i],hash,8);
      hash=detail::mulx64_mix(hash);
    }
    if(k%8){
      mark_m256i(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,boost::uint64_t hash)
  {
    for(int i=0;i<k/8;++i){
      if(!check_m256i(x[i],hash,8))return false;
      hash=detail::mulx64_mix(hash);
    }
    if(k%8){
      if(!check_m256i(x[k/8],hash,k%8))return false;
    }
    return true;
  }

private:
  static BOOST_FORCEINLINE __m256i make_m256i(
    boost::uint64_t hash,std::size_t kp)
  {
    const __m256i ones[8]={
      _mm256_set_epi32(0,0,0,0,0,0,0,1),
      _mm256_set_epi32(0,0,0,0,0,0,1,1),
      _mm256_set_epi32(0,0,0,0,0,1,1,1),
      _mm256_set_epi32(0,0,0,0,1,1,1,1),
      _mm256_set_epi32(0,0,0,1,1,1,1,1),
      _mm256_set_epi32(0,0,1,1,1,1,1,1),
      _mm256_set_epi32(0,1,1,1,1,1,1,1),
      _mm256_set_epi32(1,1,1,1,1,1,1,1),
    };

    /* Same constants as src/kudu/util/block_bloom_filter.h in
     * https://github.com/apache/kudu
     */

    const __m256i rehash=_mm256_set_epi64x(
      0x47b6137b44974d91ull,0x8824ad5ba2b7289dull,
      0x705495c72df1424bull,0x9efc49475c6bfb31ull);
    
    __m256i h=_mm256_set1_epi64x(hash);
    h=_mm256_mullo_epi32(rehash,h);
    h=_mm256_srli_epi32(h,32-5);
    return _mm256_sllv_epi32(ones[kp-1],h);
  }

  static BOOST_FORCEINLINE void mark_m256i(
    __m256i& x,boost::uint64_t hash,std::size_t kp)
  {
    __m256i h=make_m256i(hash,kp);
    x=_mm256_or_si256(x,h);
  }

  static BOOST_FORCEINLINE bool check_m256i(
    const __m256i& x,boost::uint64_t hash,std::size_t kp)
  {
    if(kp==8){
      __m256i h=make_m256i(hash,8);
      return _mm256_testc_si256(x,h);
    }
    else{
      const __m256i mask[7]={
        _mm256_set_epi32(-1,-1,-1,-1,-1,-1,-1, 0),
        _mm256_set_epi32(-1,-1,-1,-1,-1,-1, 0, 0),
        _mm256_set_epi32(-1,-1,-1,-1,-1, 0, 0, 0),
        _mm256_set_epi32(-1,-1,-1,-1, 0, 0, 0, 0),
        _mm256_set_epi32(-1,-1,-1, 0, 0, 0, 0, 0),
        _mm256_set_epi32(-1,-1, 0, 0, 0, 0, 0, 0),
        _mm256_set_epi32(-1, 0, 0, 0, 0, 0, 0, 0)
      };

      __m256i h=make_m256i(hash,kp);
      __m256i y=_mm256_or_si256(mask[kp-1],x);
      return _mm256_testc_si256(y,h);
    }
  }
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace bloom */
} /* namespace boost */
#elif defined(BOOST_BLOOM_LITTLE_ENDIAN_NEON)

#error Neon detected

#else /* fallback */
#include <boost/bloom/multiblock.hpp>

namespace boost{
namespace bloom{

template<std::size_t K>
using fast_multiblock32=multiblock<boost::uint32_t,K>;

} /* namespace bloom */
} /* namespace boost */
#endif

#endif
