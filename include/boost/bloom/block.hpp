/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_BLOCK_HPP
#define BOOST_BLOOM_BLOCK_HPP

#include <boost/bloom/detail/block_base.hpp>
#include <boost/bloom/detail/block_ops.hpp>
#include <boost/bloom/detail/block_fpr_base.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{

template<typename Block,std::size_t K>
struct block:
  public detail::block_fpr_base<K>,
  private detail::block_base<Block,K>,private detail::block_ops<Block>
{
  static constexpr std::size_t k=K;
  using value_type=Block;

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void mark(value_type& x,std::uint64_t hash)
  {
    loop(hash,[&](std::uint64_t h){set(x,h&mask);});
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(const value_type& x,std::uint64_t hash)
  {
    Block fp;
    zero(fp);
    mark(fp,hash);
    return testc(x,fp);
  }

private:
  using super=detail::block_base<Block,K>;
  using super::mask;
  using super::loop;
  using block_ops=detail::block_ops<Block>;
  using block_ops::set;
  using block_ops::zero;
  using block_ops::testc;
};

} /* namespace bloom */
} /* namespace boost */
#endif
