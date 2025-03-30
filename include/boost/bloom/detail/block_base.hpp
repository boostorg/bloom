/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_BLOCK_BASE_HPP
#define BOOST_BLOOM_DETAIL_BLOCK_BASE_HPP

#include <boost/config.hpp>
#include <boost/bloom/detail/constexpr_bit_width.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

namespace boost{
namespace bloom{
namespace detail{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

// TODO: describe

template<typename Block,std::size_t K>
struct block_base
{
  static constexpr std::size_t k=K;
  static constexpr std::size_t hash_width=sizeof(boost::uint64_t)*CHAR_BIT;
  static constexpr std::size_t block_width=sizeof(Block)*CHAR_BIT;
  static_assert(
    (block_width&(block_width-1))==0,
    "Block's size in bits must be a power of two");
  static constexpr std::size_t mask=block_width-1;
  static constexpr std::size_t shift=constexpr_bit_width(mask);
  static constexpr std::size_t rehash_k=(hash_width-shift)/shift;

  template<typename F>
  static BOOST_FORCEINLINE void loop(boost::uint64_t hash,F f)
  {
    for(std::size_t i=0;i<k/rehash_k;++i){
      auto h=hash;
      for(std::size_t j=0;j<rehash_k;++j){
        h>>=shift;
        f(h);
      }
      hash=detail::mulx64_mix(hash);
    }
    auto h=hash;
    for(std::size_t i=0;i<k%rehash_k;++i){
      h>>=shift;
      f(h);
    }
  }
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
