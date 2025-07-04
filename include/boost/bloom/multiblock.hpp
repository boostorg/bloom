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
#include <boost/config/workaround.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{

template<typename Block,std::size_t K,bool Branchless=false>
struct multiblock:
  public detail::multiblock_fpr_base<K>,
  private detail::block_base<Block,K>
{
  static constexpr std::size_t k=K;
  using value_type=Block[k];

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void mark(value_type& x,std::uint64_t hash)
  {
    std::size_t i=0;
    loop(hash,[&](std::uint64_t h){block_ops::set(x[i++],h&mask);});
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(const value_type& x,std::uint64_t hash)
  {
    return check(x,hash,std::integral_constant<bool,Branchless>{});
  }


private:
  using super=detail::block_base<Block,K>;
  using super::mask;
  using super::loop;
  using super::loop_while;
  using block_ops=detail::block_ops<Block>;

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
/* 'int': forcing value to bool 'true' or 'false' */
#pragma warning(push)
#pragma warning(disable:4800)
#endif

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(
    const value_type& x,std::uint64_t hash,std::false_type /* branchful */)
  {
    std::size_t i=0;
    return loop_while(hash,[&](std::uint64_t h){
      return block_ops::get_at_lsb(x[i++],h&mask)&1;
    });
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(
    const value_type& x,std::uint64_t hash,std::true_type /* branchless */)
  {
    int res=1;
    std::size_t i=0;
    loop(hash,[&](std::uint64_t h){block_ops::reduce(res,x[i++],h&mask);});
    return res;
  }

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
#pragma warning(pop) /* C4800 */
#endif
};

} /* namespace bloom */
} /* namespace boost */
#endif
