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
    for(std::size_t i=0;i<k/8;++i){
      mark_m256i(x[i],hash,8);
      hash=detail::mulx64_mix(hash);
    }
    if(k%8){
      mark_m256i(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,boost::uint64_t hash)
  {
    for(std::size_t i=0;i<k/8;++i){
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

    __m256i h=_mm256_set1_epi64x(hash);
    h=_mm256_sllv_epi64(h,_mm256_set_epi64x(15,10,5,0));
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
    __m256i h=make_m256i(hash,kp);
    return _mm256_testc_si256(x,h);
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

/* https://stackoverflow.com/a/54018882/213114 */

#ifdef _MSC_VER
#define BOOST_BLOOM_INIT_U32X4(w,x,y,z) \
{((w)+(unsigned long long(x)<<32)),((y)+(unsigned long long(z)<<32))}
#else
#define BOOST_BLOOM_INIT_U32X4(w,x,y,z) {(w),(x),(y),(z)}
#endif

#define BOOST_BLOOM_INIT_U32X4X2(w0,x0,y0,z0,w1,x1,y1,z1) \
{{BOOST_BLOOM_INIT_U32X4(w0,x0,y0,z0),BOOST_BLOOM_INIT_U32X4(w1,x1,y1,z1)}}

template<std::size_t K>
struct fast_multiblock32
{
  static constexpr std::size_t k=K;
  using value_type=uint32x4x2_t[(k+7)/8];
  static constexpr std::size_t used_value_size=sizeof(boost::uint32_t)*k;

  static BOOST_FORCEINLINE void mark(value_type& x,boost::uint64_t hash)
  {
    for(std::size_t i=0;i<k/8;++i){
      mark_uint32x4x2_t(x[i],hash,8);
      hash=detail::mulx64_mix(hash);
    }
    if(k%8){
      mark_uint32x4x2_t(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,boost::uint64_t hash)
  {
    for(std::size_t i=0;i<k/8;++i){
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
    static const uint32x4x2_t ones[8]={
      BOOST_BLOOM_INIT_U32X4X2(1,0,0,0,0,0,0,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,0,0,0,0,0,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,1,0,0,0,0,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,1,1,0,0,0,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,1,1,1,0,0,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,1,1,1,1,0,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,1,1,1,1,1,0),
      BOOST_BLOOM_INIT_U32X4X2(1,1,1,1,1,1,1,1)
    };

    uint32x4_t h_lo=vreinterpretq_u32_u64(vdupq_n_u64(hash)),
               h_hi=h_lo;

    h_lo=vreinterpretq_u32_u64(
      vshlq_u64(vreinterpretq_u64_u32(h_lo),(int64x2_t{0,5})));
    h_hi=vreinterpretq_u32_u64(
      vshlq_u64(vreinterpretq_u64_u32(h_lo),(int64x2_t{10,15})));

    h_lo=vshrq_n_u32(h_lo,32-5);
    h_hi=vshrq_n_u32(h_hi,32-5);

    return {
      vshlq_u32(ones[kp-1].val[0],vreinterpretq_s32_u32(h_lo)),
      vshlq_u32(ones[kp-1].val[1],vreinterpretq_s32_u32(h_hi))
    };
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
    uint32x4_t   lo=vtstq_u32(x.val[0],h.val[0]);
    uint32x4_t   hi=vtstq_u32(x.val[1],h.val[1]);
    if(kp!=8){
      static const boost::uint32_t out=0xFFFFFFFFu;
      static const uint32x4x2_t masks[7]={
        BOOST_BLOOM_INIT_U32X4X2( 0 ,out,out,out,out,out,out,out),
        BOOST_BLOOM_INIT_U32X4X2( 0 , 0 ,out,out,out,out,out,out),
        BOOST_BLOOM_INIT_U32X4X2( 0 , 0 , 0 ,out,out,out,out,out),
        BOOST_BLOOM_INIT_U32X4X2( 0 , 0 , 0 , 0 ,out,out,out,out),
        BOOST_BLOOM_INIT_U32X4X2( 0 , 0 , 0 , 0 , 0 ,out,out,out),
        BOOST_BLOOM_INIT_U32X4X2( 0 , 0 , 0 , 0 , 0 , 0 ,out,out),
        BOOST_BLOOM_INIT_U32X4X2( 0 , 0 , 0 , 0 , 0 , 0 , 0 ,out)
      };

      lo=vorrq_u32(lo,masks[kp-1].val[0]);
      hi=vorrq_u32(hi,masks[kp-1].val[1]);
    }
    int64x2_t res=vreinterpretq_s64_u32(vandq_u32(lo,hi));
    return (vgetq_lane_s64(res,0)&vgetq_lane_s64(res,1))==-1;
  }
};

#undef BOOST_BLOOM_INIT_U32X4X2
#undef BOOST_BLOOM_INIT_U32X4

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
