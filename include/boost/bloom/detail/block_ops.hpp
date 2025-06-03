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
  static inline Block zero()
  {
    return Block(0);
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

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */

#endif
