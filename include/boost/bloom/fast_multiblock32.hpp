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
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

namespace boost{
namespace bloom{

template<std::size_t K>
struct fast_multiblock32
{
  static constexpr std::size_t k=K;
  using value_type=uint32x4x2_t[(k+7)/8];
  static constexpr std::size_t used_value_size=sizeof(boost::uint32_t)*k;

  static BOOST_FORCEINLINE void mark(value_type& x,boost::uint64_t hash)
  {
    for(int i=0;i<k/8;++i){
      mark_uint32x4x2_t(x[i],hash,8);
      hash=detail::mulx64_mix(hash);
    }
    if(k%8){
      mark_uint32x4x2_t(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,boost::uint64_t hash)
  {
    for(int i=0;i<k/8;++i){
      if(!check_uint32x4x2_t(x[i],hash,8))return false;
      hash=detail::mulx64_mix(hash);
    }
    if(k%8){
      if(!check_uint32x4x2_t(x[k/8],hash,k%8))return false;
    }
    return true;
  }

private:
  static BOOST_FORCEINLINE uint32x4x2_t make_uint32x4x2_t(
    boost::uint64_t hash,std::size_t kp)
  {
    static const boost::uint32_t ones_data[8][8]={
      {1,0,0,0, 0,0,0,0},
      {1,1,0,0, 0,0,0,0},
      {1,1,1,0, 0,0,0,0},
      {1,1,1,1, 0,0,0,0},
      {1,1,1,1, 1,0,0,0},
      {1,1,1,1, 1,1,0,0},
      {1,1,1,1, 1,1,1,0},
      {1,1,1,1, 1,1,1,1}
    };

    uint32x4_t ones_lo=vld1q_u32(ones_data[kp-1]);
    uint32x4_t ones_hi=vld1q_u32(ones_data[kp-1]+4);

    /* Same constants as src/kudu/util/block_bloom_filter.h in
     * https://github.com/apache/kudu
     */

    static const uint32_t rehash_lo_data[4]=
      {0x5c6bfb31,0x9efc4947,0x2df1424b,0x705495c7};
    static const uint32_t rehash_hi_data[4]=
      {0xa2b7289d,0x8824ad5b,0x44974d91,0x47b6137b};

    uint32x4_t rehash_lo=vld1q_u32(rehash_lo_data);
    uint32x4_t rehash_hi=vld1q_u32(rehash_hi_data);

    uint32x4_t h_lo=vdupq_n_u32((boost::uint32_t)hash);
    uint32x4_t h_hi=vdupq_n_u32((boost::uint32_t)(hash>>32));

    uint32x4_t prod_lo=vmulq_u32(rehash_lo,h_lo);
    uint32x4_t prod_hi=vmulq_u32(rehash_hi,h_hi);

    uint32x4_t s_lo=vshrq_n_u32(prod_lo,27);
    uint32x4_t s_hi=vshrq_n_u32(prod_hi,27);

    int32x4_t s_lo_int=vreinterpretq_s32_u32(s_lo);
    int32x4_t s_hi_int=vreinterpretq_s32_u32(s_hi);

    uint32x4_t res_lo=vshlq_u32(ones_lo,s_lo_int);
    uint32x4_t res_hi=vshlq_u32(ones_hi,s_hi_int);

    return {res_lo,res_hi};
  }

  static BOOST_FORCEINLINE void mark_uint32x4x2_t(
    uint32x4x2_t& x,boost::uint64_t hash,std::size_t kp)
  {
    uint32x4x2_t h=make_uint32x4x2_t(hash,kp);
    x.val[0]=vorrq_u32(x.val[0],h.val[0]);
    x.val[1]=vorrq_u32(x.val[1],h.val[1]);
  }

  static BOOST_FORCEINLINE bool check_uint32x4x2_t(
    const uint32x4x2_t& x,boost::uint64_t hash,std::size_t kp)
  {
    uint32x4x2_t h=make_uint32x4x2_t(hash,kp);
    uint32x4_t   t0=vtstq_u32(x.val[0],h.val[0]);
    uint32x4_t   t1=vtstq_u32(x.val[1],h.val[1]);
    if(kp!=8){
      static const boost::uint32_t out=0xFFFFFFFFu;
      static const uint32x4x2_t masks[7]={
        {{{  0,out,out,out},{out,out,out,out}}},
        {{{  0,  0,out,out},{out,out,out,out}}},
        {{{  0,  0,  0,out},{out,out,out,out}}},
        {{{  0,  0,  0,  0},{out,out,out,out}}},
        {{{  0,  0,  0,  0},{  0,out,out,out}}},
        {{{  0,  0,  0,  0},{  0,  0,out,out}}},
        {{{  0,  0,  0,  0},{  0,  0,  0,out}}}
      };

      t0=vorrq_u32(t0,masks[kp-1].val[0]);
      t1=vorrq_u32(t1,masks[kp-1].val[1]);
    }
    int64x2_t t=vreinterpretq_s64_u32(vandq_u32(t0, t1));
    return (vgetq_lane_s64(t,0)&vgetq_lane_s64(t,1))==-1;
  }
};

} /* namespace bloom */
} /* namespace boost */
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
