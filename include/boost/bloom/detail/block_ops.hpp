/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_BLOCK_OPS_HPP
#define BOOST_BLOOM_DETAIL_BLOCK_OPS_HPP

#include <cstdint>

namespace boost{
namespace bloom{
namespace detail{

template<typename Block>
struct block_ops
{
  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void zero(Block& x)
  {
    x=0;
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void set(Block& x,std::uint64_t n)
  {
    x|=Block(1)<<n;
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void reduce(int& res,const Block& x,std::uint64_t n)
  {
    res&=static_cast<int>(x>>n);
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool testc(const Block& x,const Block& y)
  {
    return (x&y)==y;
  }
};

template<typename Block,std::size_t N>
struct block_ops<Block[N]>
{
  using value_type=Block[N];

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void zero(value_type& x)
  {
    for(std::size_t i=0;i<N;++i)x[i]=0;
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void set(value_type& x,std::uint64_t n)
  {
    x[n%N]|=Block(1)<<(n/N);
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void reduce(int& res,const value_type& x,std::uint64_t n)
  {
    res&=static_cast<int>(x[n%N]>>(n/N));
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool testc(const value_type& x,const value_type& y)
  {
#if 1
    for(std::size_t i=0;i<N;++i){
     if((x[i]&y[i])!=y[i])return false;
    }
    return true;
#else
    int res=1;
    for(std::size_t i=0;i<N;++i){
     res&=((x[i]&y[i])==y[i]);
    }
    return res;
#endif
  }
};

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */

#endif
