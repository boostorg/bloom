/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_MULTIBLOCK_HPP
#define BOOST_BLOOM_MULTIBLOCK_HPP

#include <boost/bloom/detail/block_base.hpp>
#include <boost/bloom/detail/block_ops.hpp>
#include <boost/bloom/detail/multiblock_fpr_base.hpp>
#include <boost/config.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

template<typename Block,std::size_t K>
struct multiblock:
  public detail::multiblock_fpr_base<K>,
  private detail::block_base<Block,K>
{
  static constexpr std::size_t k=K;
  using value_type=Block[k];

  static BOOST_FORCEINLINE void mark(value_type& x,std::uint64_t hash)
  {
    std::size_t i=0;
    loop(hash,[&](std::uint64_t h){block_ops::set(x[i++],h&mask);});
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,std::uint64_t hash)
  {
    int res=1;
    std::size_t i=0;
    loop(hash,[&](std::uint64_t h){block_ops::reduce(res,x[i++],h&mask);});
    return res;
  }

private:
  using super=detail::block_base<Block,K>;
  using super::mask;
  using super::loop;
  using block_ops=detail::block_ops<Block>;
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace bloom */
} /* namespace boost */
#endif
