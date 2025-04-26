/* Copyright 2022 Peter Dimov.
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_MULX64_HPP
#define BOOST_BLOOM_DETAIL_MULX64_HPP

#include <boost/cstdint.hpp>
#include <climits>
#include <cstddef>

#if defined(_MSC_VER)&&!defined(__clang__)
#include <intrin.h>
#endif

namespace boost{
namespace bloom{
namespace detail{

#if defined(_MSC_VER)&&defined(_M_X64)&&!defined(__clang__)

__forceinline boost::uint64_t umul128(
  boost::uint64_t x,boost::uint64_t y,boost::uint64_t& hi)
{
  return _umul128(x,y,&hi);
}

#elif defined(_MSC_VER)&&defined(_M_ARM64)&&!defined(__clang__)

__forceinline boost::uint64_t umul128(
  boost::uint64_t x,boost::uint64_t y,boost::uint64_t& hi)
{
  hi=__umulh(x,y);
  return x*y;
}

#elif defined(__SIZEOF_INT128__)

inline boost::uint64_t umul128(
  boost::uint64_t x,boost::uint64_t y,boost::uint64_t& hi)
{
  __uint128_t r=(__uint128_t)x*y;
  hi=(boost::uint64_t)(r>>64);
  return (boost::uint64_t)r;
}

#else

inline boost::uint64_t umul128(
  boost::uint64_t x,boost::uint64_t y,boost::uint64_t& hi)
{
  boost::uint64_t x1=(boost::uint32_t)x;
  boost::uint64_t x2=x >> 32;

  boost::uint64_t y1=(boost::uint32_t)y;
  boost::uint64_t y2=y >> 32;

  boost::uint64_t r3=x2*y2;

  boost::uint64_t r2a=x1*y2;

  r3+=r2a>>32;

  boost::uint64_t r2b=x2*y1;

  r3+=r2b>>32;

  boost::uint64_t r1=x1*y1;

  boost::uint64_t r2=(r1>>32)+(boost::uint32_t)r2a+(boost::uint32_t)r2b;

  r1=(r2<<32)+(boost::uint32_t)r1;
  r3+=r2>>32;

  hi=r3;
  return r1;
}

#endif

inline boost::uint64_t mulx64(boost::uint64_t x)noexcept
{
  /* multiplier is 2^64/phi */
  boost::uint64_t hi;
  boost::uint64_t lo=umul128(x,0x9E3779B97F4A7C15ull,hi);
  return hi^lo;
}

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
