/* Common base for all boost::bloom::filter instantiations.
 *
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_CORE_HPP
#define BOOST_BLOOM_DETAIL_CORE_HPP

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/bloom/detail/sse2.hpp>
#include <boost/config.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/core/allocator_traits.hpp>
#include <boost/cstdint.hpp>
#include <boost/throw_exception.hpp>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

/* We use BOOST_BLOOM_PREFETCH[_WRITE] macros rather than proper
 * functions because of https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109985
 */

#if defined(BOOST_GCC)||defined(BOOST_CLANG)
#define BOOST_BLOOM_PREFETCH(p) __builtin_prefetch((const char*)(p))
#define BOOST_BLOOM_PREFETCH_WRITE(p) __builtin_prefetch((const char*)(p),1)
#elif defined(BOOST_BLOOM_SSE2)
#define BOOST_BLOOM_PREFETCH(p) _mm_prefetch((const char*)(p),_MM_HINT_T0)
#if defined(_MM_HINT_ET0)
#define BOOST_BLOOM_PREFETCH_WRITE(p) \
_mm_prefetch((const char*)(p),_MM_HINT_ET0)
#else
#define BOOST_BLOOM_PREFETCH_WRITE(p) \
_mm_prefetch((const char*)(p),_MM_HINT_T0)
#endif
#else
#define BOOST_BLOOM_PREFETCH(p) ((void)(p))
#define BOOST_BLOOM_PREFETCH_WRITE(p) ((void)(p))
#endif

namespace boost{
namespace bloom{
namespace detail{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

/*  mcg_and_fastrange produces (pos,hash') from hash, where
 *   - x=mulx64(hash,range), mulx64 denotes extended multiplication
 *   - pos=high(x)
 *   - hash'=low(x)
 *  pos is uniformly distributed in [0,range) (see
 *  https://arxiv.org/pdf/1805.10941), whereas hash'<-hash is a multiplicative
 *  congruential generator of the form hash'<-hash*rng mod 2^64. This MCG
 *  generates long cycles when the initial value of hash is odd and
 *  rng = +-3 (mod 8), which is why we adjust hash and rng as seen below. As a
 *  result, the low bits of hash' are of poor quality, and the least
 *  significant bit in particular is always one.
 */

struct mcg_and_fastrange
{
  constexpr mcg_and_fastrange(std::size_t m)noexcept:
    rng{
      m+(
        (m%8<=3)?3-(m%8):
        (m%8<=5)?5-(m%8):
                 8-(m%8)+3)
    }
    {}

  inline constexpr std::size_t range()const noexcept{return (std::size_t)rng;}

  inline void prepare_hash(boost::uint64_t& hash)const noexcept
  {
    hash|=1u;
  }

  inline std::size_t next_position(boost::uint64_t& hash)const noexcept
  {
    boost::uint64_t hi;
    hash=mulx64(hash,rng,hi);
    return (std::size_t)hi;
  }

  boost::uint64_t rng;
};

/* used_block_size<Subfilter>::value is Subfilter::used_value_size if it
 * exists, or sizeof(Subfilter::value_type) otherwise. This covers the
 * case where a subfilter only operates on the first bytes of its entire
 * value_type (e.g. fast_multiblock32<K> with K<8).
 */

template<typename Subfilter,typename=void>
struct used_block_size
{
  static constexpr std::size_t value=sizeof(typename Subfilter::value_type);
};

template<typename Subfilter>
struct used_block_size<
  Subfilter,
  typename std::enable_if<Subfilter::used_value_size!=0>::type
>
{
  static constexpr std::size_t value=Subfilter::used_value_size;
};

/* GCD with x,p > 1, p a power of two */

inline constexpr std::size_t gcd_pow2(std::size_t x,std::size_t p)
{
  /* x&-x: maximum power of two dividing x */
  return (x&(0-x))<p?(x&(0-x)):p;
}

/* std::ldexp is not constexpr in C++11 */

inline constexpr double constexpr_ldexp_1_positive(int exp)
{
  return exp==0?1.0:2.0*constexpr_ldexp_1_positive(exp-1);
}

struct filter_array
{
  unsigned char* data;
  unsigned char* buckets; /* adjusted from data for proper alignment */
};

struct if_constexpr_void_else{void operator()()const{}};

template<bool B,typename F,typename G=if_constexpr_void_else>
void if_constexpr(F f,G g={})
{
  std::get<B?0:1>(std::forward_as_tuple(f,g))();
}

template<bool B,typename T,typename std::enable_if<B>::type* =nullptr>
void copy_assign_if(T& x,const T& y){x=y;}

template<bool B,typename T,typename std::enable_if<!B>::type* =nullptr>
void copy_assign_if(T&,const T&){}

template<bool B,typename T,typename std::enable_if<B>::type* =nullptr>
void move_assign_if(T& x,T& y){x=std::move(y);}

template<bool B,typename T,typename std::enable_if<!B>::type* =nullptr>
void move_assign_if(T&,T&){}

template<bool B,typename T,typename std::enable_if<B>::type* =nullptr>
void swap_if(T& x,T& y){using std::swap; swap(x,y);}

template<bool B,typename T,typename std::enable_if<!B>::type* =nullptr>
void swap_if(T&,T&){}

template<
  std::size_t K,typename Subfilter,std::size_t BucketSize,typename Allocator
>
class filter_core:empty_value<Allocator,0>
{
  static_assert(K>0,"K must be >= 1");
  static_assert(
    std::is_same<allocator_value_type_t<Allocator>,unsigned char>::value,
    "Allocator value_type must be unsigned char");

public:
  static constexpr std::size_t k=K;
  using subfilter=Subfilter;

private:
  static constexpr std::size_t kp=subfilter::k;
  static constexpr std::size_t k_total=k*kp;
  using block_type=typename subfilter::value_type;
  static constexpr std::size_t block_size=sizeof(block_type);
  static constexpr std::size_t used_block_size=
    detail::used_block_size<subfilter>::value;

public:
  static constexpr std::size_t bucket_size=
    BucketSize?BucketSize:used_block_size;
  static_assert(
    bucket_size<=used_block_size,"BucketSize can't exceed the block size");

private:
  static constexpr std::size_t tail_size=sizeof(block_type)-bucket_size;
  static constexpr bool are_blocks_aligned=
    (bucket_size%alignof(block_type)==0);
  static constexpr std::size_t cacheline=64; /* unknown at compile time */
  static constexpr std::size_t initial_alignment=
    are_blocks_aligned?
      alignof(block_type)>cacheline?alignof(block_type):cacheline:
      1;
  static constexpr std::size_t prefetched_cachelines=
    1+(block_size+cacheline-1-gcd_pow2(bucket_size,cacheline))/cacheline;
  using hash_strategy=detail::mcg_and_fastrange;

public:
  using allocator_type=Allocator;
  using size_type=std::size_t;
  using difference_type=std::ptrdiff_t;
  using pointer=unsigned char*;
  using const_pointer=const unsigned char*;

  explicit filter_core(std::size_t m=0):filter_core{m,allocator_type{}}{}

  filter_core(std::size_t m,const allocator_type& al_):
    allocator_base{empty_init,al_},
    hs{requested_range(m)},
    ar(new_array(al(),m?hs.range():0))
  {
    clear_bytes();
  }

  filter_core(std::size_t n,double fpr,const allocator_type& al_):
    filter_core(capacity_for(n,fpr),al_){}

  filter_core(const filter_core& x):
    filter_core{x,allocator_select_on_container_copy_construction(x.al())}{}

  filter_core(filter_core&& x)noexcept:
    filter_core{std::move(x),allocator_type(std::move(x.al()))}{}

  filter_core(const filter_core& x,const allocator_type& al_):
    allocator_base{empty_init,al_},
    hs{x.hs},
    ar(new_array(al(),x.range()))
  {
    copy_bytes(x);
  }

  filter_core(filter_core&& x,const allocator_type& al_):
    allocator_base{empty_init,al_},
    hs{x.hs}
  {
    auto empty_ar=new_array(x.al(),0); /* we're relying on this not throwing */
    if(al()==x.al()){
      ar=x.ar;
    }
    else{
      ar=new_array(al(),x.range());
      copy_bytes(x);
      x.delete_array();
    }
    x.hs=hash_strategy{0};
    x.ar=empty_ar;
  }

  ~filter_core()noexcept
  {
    delete_array();
  }

  filter_core& operator=(const filter_core& x)
  {
    static constexpr auto pocca=
      allocator_propagate_on_container_copy_assignment_t<allocator_type>::
        value;

    if(this!=&x){
      if_constexpr<pocca>([&,this]{
        if(al()!=x.al()||range()!=x.range()){
          auto x_al=x.al();
          auto new_ar=new_array(x_al,x.range());
          delete_array();
          hs=x.hs;
          ar=new_ar;
        }
        copy_assign_if<pocca>(al(),x.al());
      },
      [&,this]{ /* else */
        if(range()!=x.range()){
          auto new_ar=new_array(al(),x.range());
          delete_array();
          hs=x.hs;
          ar=new_ar;
        }
      });
      copy_bytes(x);
    }
    return *this;
  }

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4127) /* conditional expression is constant */
#endif

  filter_core& operator=(filter_core&& x)noexcept(
    allocator_propagate_on_container_move_assignment_t<allocator_type>::value||
    allocator_is_always_equal_t<allocator_type>::value)
  {
    static constexpr auto pocma=
      allocator_propagate_on_container_move_assignment_t<allocator_type>::
        value;

    if(this!=&x){
      auto empty_ar=new_array(x.al(),0); /* relying on this not throwing */
      if(pocma||al()==x.al()){
        delete_array();
        move_assign_if<pocma>(al(),x.al());
        hs=x.hs;
        ar=x.ar;
      }
      else{
        if(range()!=x.range()){
          auto new_ar=new_array(al(),x.range());
          delete_array();
          hs=x.hs;
          ar=new_ar;
        }
        copy_bytes(x);
        x.delete_array();
      }
      x.hs=hash_strategy{0};
      x.ar=empty_ar;
    }
    return *this;
  }

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4127 */
#endif

  allocator_type get_allocator()const noexcept
  {
    return al();
  }

  std::size_t capacity()const noexcept
  {
    return used_array_size()*CHAR_BIT;
  }

  BOOST_FORCEINLINE void insert(boost::uint64_t hash)
  {
    hs.prepare_hash(hash);
    for(auto n=k;n--;){
      auto p=next_element(hash); /* modifies h */
      /* We do the unhappy-path null check here rather than at the beginning
       * of the function because prefetch completion wait gives us free CPU
       * cycles to spare.
       */
      if(BOOST_UNLIKELY(n==k-1&&ar.data==nullptr))return;

      set(p,hash);
    }
  }

  void swap(filter_core& x)noexcept(
    allocator_propagate_on_container_swap_t<allocator_type>::value||
    allocator_is_always_equal_t<allocator_type>::value)
  {
    static constexpr auto pocs=
      allocator_propagate_on_container_swap_t<allocator_type>::value;

    if_constexpr<pocs>([&,this]{
      swap_if<pocs>(al(),x.al());
    },
    [&,this]{ /* else */
      BOOST_ASSERT(al()==x.al());
      (void)this; /* makes sure captured this is used */
    });
    std::swap(hs,x.hs);
    std::swap(ar,x.ar);
  }

  void clear()noexcept
  {
    clear_bytes();
  }

  void reset(std::size_t m=0)
  {
    hash_strategy new_hs{requested_range(m)};
    std::size_t   rng=m?new_hs.range():0;
    if(rng!=range()){
      auto new_ar=new_array(al(),rng);
      delete_array();
      hs=new_hs;
      ar=new_ar;
    }
    clear_bytes();
  }

  filter_core& operator&=(const filter_core& x)
  {
    combine(x,[](unsigned char& a,unsigned char b){a&=b;});
    return *this;
  }

  filter_core& operator|=(const filter_core& x)
  {
    combine(x,[](unsigned char& a,unsigned char b){a|=b;});
    return *this;
  }

  BOOST_FORCEINLINE bool may_contain(boost::uint64_t hash)const
  {
    hs.prepare_hash(hash);
#if 1
    auto p0=next_element(hash);
    for(std::size_t n=k-1;n--;){
      auto p=p0;
      auto hash0=hash;
      p0=next_element(hash);
      if(!get(p,hash0))return false;
    }
    if(!get(p0,hash))return false;
    return true;
#else
    for(auto n=k;n--;){
      auto p=next_element(hash); /* modifies hash */
      if(!get(p,hash))return false;
    }
    return true;
#endif
  }

  friend bool operator==(const filter_core& x,const filter_core& y)
  {
    if(x.range()!=y.range())return false;
    else if(!x.ar.data)return true;
    else return std::memcmp(x.ar.buckets,y.ar.buckets,x.used_array_size())==0;
  }

private:
  using allocator_base=empty_value<Allocator,0>;

  const Allocator& al()const{return allocator_base::get();}
  Allocator& al(){return allocator_base::get();}

  static std::size_t requested_range(std::size_t m)
  {
    if(m>(used_block_size-bucket_size)*CHAR_BIT){
      /* ensures filter_core{f.capacity()}.capacity()==f.capacity() */
      m-=(used_block_size-bucket_size)*CHAR_BIT;
    }
    return
      (std::numeric_limits<std::size_t>::max)()-m>=bucket_size*CHAR_BIT-1?
      (m+bucket_size*CHAR_BIT-1)/(bucket_size*CHAR_BIT):
      m/(bucket_size*CHAR_BIT);
  }

  static filter_array new_array(allocator_type& al,std::size_t rng)
  {
    if(rng){
      auto p=allocator_allocate(al,space_for(rng));
      return {p,buckets_for(p)};
    }
    else{
      /* To avoid dynamic allocation for zero capacity or moved-from filters,
       * we point buckets to a statically allocated dummy array with all bits
       * set to one. This is good for read operations but not so for write
       * operations, where we need to resort to a null check on
       * filter_array::data.
       */

      static struct {unsigned char x=-1;}
      dummy[space_for(hash_strategy{0}.range())];

      return {nullptr,buckets_for(reinterpret_cast<unsigned char*>(&dummy))};
    }
  }

  void delete_array()noexcept
  {
    if(ar.data)allocator_deallocate(al(),ar.data,space_for(range()));
  }

  void clear_bytes()noexcept
  {
    std::memset(ar.buckets,0,used_array_size());
  }

  void copy_bytes(const filter_core& x)
  {
    BOOST_ASSERT(range()==x.range());
    std::memcpy(ar.buckets,x.ar.buckets,used_array_size());
  }

  std::size_t range()const noexcept
  {
    return ar.data?hs.range():0;
  }

  static constexpr std::size_t space_for(std::size_t rng)noexcept
  {
    return (initial_alignment-1)+rng*bucket_size+tail_size;
  }

  static unsigned char* buckets_for(unsigned char* p)noexcept
  {
    return p+
      (boost::uintptr_t(initial_alignment)-
       boost::uintptr_t(p))%initial_alignment;
  }

  std::size_t used_array_size()const noexcept
  {
    return range()?range()*bucket_size+(used_block_size-bucket_size):0;
  }

  static std::size_t capacity_for(std::size_t n,double fpr)
  {
    using size_t_limits=std::numeric_limits<std::size_t>;
    using double_limits=std::numeric_limits<double>;

    BOOST_ASSERT(fpr>=0.0&&fpr<=1.0);
    if(n==0)return 0;

    constexpr double eps=1.0/(double)(std::numeric_limits<std::size_t>::max)();
    constexpr double max_size_t_as_double=
      size_t_limits::digits<=double_limits::digits?
      (double)(size_t_limits::max)():
      (double)(size_t_limits::max)()
        /* ensure value is portably castable back to std::size_t */
        -constexpr_ldexp_1_positive(
          size_t_limits::digits-double_limits::digits);

    const double c_max=max_size_t_as_double/n;

    /* Capacity of a classical Bloom filter as a lower bound:
     * c = k / -log(1 - fpr^(1/k)).
     */
    
    double d=1.0-std::pow(fpr,1.0/k_total);
    if(std::fpclassify(d)==FP_ZERO)return 0; /* fpr ~ 1 */
    double l=std::log(d);
    if(std::fpclassify(l)==FP_ZERO)return (std::size_t)(c_max*n); /* fpr ~ 0 */
    double c0=(std::min)(k_total/-l,c_max);

    /* bracket target fpr between c0 and c1 */

    double c1=c0;
    if(fpr_for(c1)>fpr){ /* expected case */
      do{
        double cn=c1*1.5;
        if(cn>c_max)return (std::size_t)(c_max*n);
        c0=c1;
        c1=cn;
      }while(fpr_for(c1)>fpr);
    }
    else{ /* c0 shouldn't overshoot ever, just in case */
      do{
        double cn=c0/1.5;
        c1=c0;
        c0=cn;
      }while(fpr_for(c0)<fpr);
    }

    /* bisect */

    double cm;
    while((cm=c0+(c1-c0)/2)>c0 && cm<c1 && c1-c0>=eps){
      if(fpr_for(cm)>fpr)c0=cm;
      else               c1=cm;
    }
    return (std::size_t)(cm*n);
  }

  static double fpr_for(double c)
  {
    constexpr std::size_t w=(2*used_block_size-bucket_size)*CHAR_BIT;
    const double          lambda=w*k/c;
    const double          loglambda=std::log(lambda);
    double                res=0.0;
    double                deltap=0.0;
    for(int i=0;i<1000;++i){
      double poisson=std::exp(i*loglambda-lambda-std::lgamma(i+1));
      double delta=poisson*subfilter::fpr(i,w);
      double resn=res+delta;

      /* The terms of this summation are unimodal, so we check we're on the
       * descending slope before stopping.
       */

      if(delta<deltap&&resn==res)break;
      deltap=delta;
      res=resn;
    }

    /* For small values of c (high values of lambda), truncation errors,loop
     * exhaustion and the use of Poisson instead of binomial may result in a
     * calculated value less than the classical Bloom filter formula, which we
     * know is always the minimum attainable.
     */

    return (std::max)(
      std::pow((double)res,(double)k),
      std::pow(1.0-std::exp(-(double)k_total/c),(double)k_total));
  }

  BOOST_FORCEINLINE bool get(const unsigned char* p,boost::uint64_t hash)const
  {
    return get(p,hash,std::integral_constant<bool,are_blocks_aligned>{});
  }

  BOOST_FORCEINLINE bool get(
    const unsigned char* p,boost::uint64_t hash,
    std::true_type /* blocks aligned */)const
  {
    return subfilter::check(*reinterpret_cast<const block_type*>(p),hash);
  }

  BOOST_FORCEINLINE bool get(
    const unsigned char* p,boost::uint64_t hash,
    std::false_type /* blocks not aligned */)const
  {
    block_type x;
    std::memcpy(&x,p,block_size);
    return subfilter::check(x,hash);
  }

  BOOST_FORCEINLINE void set(unsigned char* p,boost::uint64_t hash)
  {
    return set(p,hash,std::integral_constant<bool,are_blocks_aligned>{});
  }

  BOOST_FORCEINLINE void set(
    unsigned char* p,boost::uint64_t hash,
    std::true_type /* blocks aligned */)
  {
    subfilter::mark(*reinterpret_cast<block_type*>(p),hash);
  }

  BOOST_FORCEINLINE void set(
    unsigned char* p,boost::uint64_t hash,
    std::false_type /* blocks not aligned */)
  {
    block_type x;
    std::memcpy(&x,p,block_size);
    subfilter::mark(x,hash);
    std::memcpy(p,&x,block_size);
  }

  BOOST_FORCEINLINE 
  unsigned char* next_element(boost::uint64_t& h)noexcept
  {
    auto p=ar.buckets+hs.next_position(h)*bucket_size;
    for(std::size_t i=0;i<prefetched_cachelines;++i){
      BOOST_BLOOM_PREFETCH_WRITE((unsigned char*)p+i*cacheline);
    }
    return p;
  }

  BOOST_FORCEINLINE
  const unsigned char* next_element(boost::uint64_t& h)const noexcept
  {
    auto p=ar.buckets+hs.next_position(h)*bucket_size;
    for(std::size_t i=0;i<prefetched_cachelines;++i){
      BOOST_BLOOM_PREFETCH((unsigned char*)p+i*cacheline);
    }
    return p;
  }

  template<typename F>
  void combine(const filter_core& x,F f)
  {
    if(range()!=x.range()){
      BOOST_THROW_EXCEPTION(std::invalid_argument("incompatible filters"));
    }
    auto first0=ar.buckets,
         last0=first0+used_array_size(),
         first1=x.ar.buckets;
    while(first0!=last0)f(*first0++,*first1++);
  }

  hash_strategy hs;
  filter_array  ar;
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
