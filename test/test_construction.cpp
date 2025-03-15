/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11/algorithm.hpp>
#include <new>
#include <utility>
#include <vector>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename Functor>
struct stateful:Functor
{
  stateful(int state_=0):state{state_}{}
  stateful(const stateful&)=default;
  stateful(stateful&& x)noexcept:
    Functor(std::move(x)),state{x.state}{x.state=-1;}

  stateful& operator=(stateful&& x)noexcept
  {
    if(this!=&x){
      Functor::operator=(std::move(x));
      state=x.state;
      x.state=-1;
    }
    return *this;
  }

  int state;
};

template<
  typename T,
  typename Propagate=std::false_type,typename AlwaysEqual=std::false_type
>
struct stateful_allocator
{
  using value_type=T;
  using propagate_on_container_copy_assignment=Propagate;
  using propagate_on_container_move_assignment=Propagate;
  using propagate_on_container_swap=Propagate;
  using is_always_equal=AlwaysEqual;

  stateful_allocator(int state_=0):state{state_}{}

  template<typename U>
  stateful_allocator(const stateful_allocator<U,Propagate,AlwaysEqual>& x):
    state{x.state},last_allocation{x.last_allocation}{}

  T* allocate(std::size_t n)
  {
    auto p=static_cast<T*>(::operator new(n*sizeof(T)));
    last_allocation=p;
    return p;
  }

  void deallocate(T* p,std::size_t)
  {
    ::operator delete(p);
    last_allocation=nullptr; /* avoids spurious use-after-free warnings */
  }

  bool operator==(const stateful_allocator& x)const
  {
    return AlwaysEqual::value||(state==x.state);
  }

  bool operator!=(const stateful_allocator& x)const{return !(*this==x);}

  int   state;
  void* last_allocation=nullptr;
};

template<
  typename Filter,typename ValueFactory,typename Propagate,typename AlwaysEqual
>
void test_pocxx()
{
  static constexpr auto propagate=Propagate::value;
  static constexpr auto always_equal=AlwaysEqual::value;
  using filter=realloc_filter<
    rehash_filter<Filter,stateful<typename Filter::hasher>>,
    stateful_allocator<typename Filter::value_type,Propagate,AlwaysEqual>
  >;
  using value_type=typename filter::value_type;
  using hasher=typename filter::hasher;
  using allocator_type=typename filter::allocator_type;

  std::vector<value_type> input;
  ValueFactory            fac;
  for(int i=0;i<10;++i)input.push_back(fac());

  {
    filter f1(input.begin(),input.end(),1000,hasher{42},allocator_type{2025});
    filter f2(allocator_type{1492});
    f2=f1;
    BOOST_TEST_GE(f1.capacity(),1000);
    BOOST_TEST_EQ(f1.hash_function().state,42);
    BOOST_TEST_EQ(f1.get_allocator().state,2025);
    BOOST_TEST_GE(f2.capacity(),1000);
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,propagate?2025:1492);
    BOOST_TEST(may_contain(f2,input));
  }
  {
    filter f1(input.begin(),input.end(),1000,hasher{42},allocator_type{2025});
    auto p1=f1.get_allocator().last_allocation;
    filter f2(0,hasher{24},allocator_type{1492});
    f2=std::move(f1);
    BOOST_TEST_EQ(f1.capacity(),0);
    BOOST_TEST_EQ(f1.hash_function().state,24);
    BOOST_TEST_EQ(f1.get_allocator().state,2025);
    BOOST_TEST_GE(f2.capacity(),1000);
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,propagate?2025:1492);
    if(propagate){
      BOOST_TEST_EQ(f2.get_allocator().last_allocation,p1);
    }
    else if(always_equal){
      BOOST_TEST_EQ(f2.get_allocator().last_allocation,nullptr);
    }
    else{
      BOOST_TEST_NE(f2.get_allocator().last_allocation,nullptr);
      BOOST_TEST_NE(f2.get_allocator().last_allocation,p1);
    }
    BOOST_TEST(may_contain(f2,input));
  }
  if(propagate||always_equal){
    filter f1(input.begin(),input.end(),1000,hasher{42},allocator_type{2025});
    auto p1=f1.get_allocator().last_allocation;
    filter f2(0,hasher{24},allocator_type{1492});
    if(propagate){ /* just a way to test member and non-member swap */
      f2.swap(f1);
    }
    else{
      using std::swap;
      swap(f1,f2);
    }
    BOOST_TEST_EQ(f1.capacity(),0);
    BOOST_TEST_EQ(f1.hash_function().state,24);
    BOOST_TEST_EQ(f1.get_allocator().state,propagate?1492:2025);
    BOOST_TEST_EQ(f1.get_allocator().last_allocation,propagate?nullptr:p1);
    BOOST_TEST_GE(f2.capacity(),1000);
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,propagate?2025:1492);
    BOOST_TEST_EQ(f2.get_allocator().last_allocation,propagate?p1:nullptr);
  }
}

template<typename Filter,typename ValueFactory>
void test_construction()
{
  using filter=realloc_filter<
    rehash_filter<Filter,stateful<typename Filter::hasher>>,
    stateful_allocator<typename Filter::value_type>
  >;
  using value_type=typename filter::value_type;
  using hasher=typename filter::hasher;
  using allocator_type=typename filter::allocator_type;

  std::vector<value_type> input;
  ValueFactory            fac;
  for(int i=0;i<10;++i)input.push_back(fac());

  std::initializer_list<value_type> il={input[0],input[1],input[2],input[3]};

  {
    filter f;
    BOOST_TEST_EQ(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
  }
  {
    filter f(1000);
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
  }
  {
    filter f(100,0.01);
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
  }
  {
    filter f(1000,hasher{42});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,0);
  }
  {
    filter f(100,0.01,hasher{42});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,0);
  }
  {
    filter f(1000,hasher{42},allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
  }
  {
    filter f(100,0.01,hasher{42},allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
  }
  {
    filter f(input.begin(),input.end(),1000);
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(input.begin(),input.end(),100,0.01);
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(input.begin(),input.end(),1000,hasher{42});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(input.begin(),input.end(),100,0.01,hasher{42});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(input.begin(),input.end(),1000,hasher{42},allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(
      input.begin(),input.end(),100,0.01,hasher{42},allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f1(1000,hasher{42},allocator_type{2025});
    f1.insert(input.begin(),input.end());
    filter f2(f1);
    BOOST_TEST_GE(f1.capacity(),1000);
    BOOST_TEST_EQ(f1.hash_function().state,42);
    BOOST_TEST_EQ(f1.get_allocator().state,2025);
    BOOST_TEST_EQ(f2.capacity(),f1.capacity());
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,2025);
    BOOST_TEST(may_contain(f2,input));
  }
  {
    filter f1(1000,hasher{42},allocator_type{2025});
    f1.insert(input.begin(),input.end());
    auto p1=f1.get_allocator().last_allocation;
    filter f2(std::move(f1));
    BOOST_TEST_EQ(f1.capacity(),0);
    BOOST_TEST_EQ(f1.hash_function().state,-1);
    BOOST_TEST_GE(f2.capacity(),1000);
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,2025);
    BOOST_TEST_EQ(f2.get_allocator().last_allocation,p1);
    BOOST_TEST(may_contain(f2,input));
  }
  {
    filter f(input.begin(),input.end(),1000,allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(input.begin(),input.end(),100,0.01,allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,input));
  }
  {
    filter f(allocator_type{2025});
    BOOST_TEST_EQ(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
  }
  {
    filter f1(1000,hasher{42},allocator_type{2025});
    f1.insert(input.begin(),input.end());
    filter f2(f1,allocator_type{1492});
    BOOST_TEST_GE(f1.capacity(),1000);
    BOOST_TEST_EQ(f1.hash_function().state,42);
    BOOST_TEST_EQ(f1.get_allocator().state,2025);
    BOOST_TEST(may_contain(f1,input));
    BOOST_TEST_EQ(f2.capacity(),f1.capacity());
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,1492);
    BOOST_TEST(may_contain(f2,input));
  }
  {
    filter f1(1000,hasher{42},allocator_type{2025});
    f1.insert(input.begin(),input.end());
    auto p1=f1.get_allocator().last_allocation;
    filter f2(std::move(f1),allocator_type{1492});
    BOOST_TEST_EQ(f1.capacity(),0);
    BOOST_TEST_EQ(f1.hash_function().state,-1);
    BOOST_TEST_EQ(f1.get_allocator().state,2025);
    BOOST_TEST_GE(f2.capacity(),1000);
    BOOST_TEST_EQ(f2.hash_function().state,42);
    BOOST_TEST_EQ(f2.get_allocator().state,1492);
    BOOST_TEST_NE(f2.get_allocator().last_allocation,nullptr);
    BOOST_TEST_NE(f2.get_allocator().last_allocation,p1);
    BOOST_TEST(may_contain(f2,input));

    filter f3(1000,hasher{42},allocator_type{2025});
    f3.insert(input.begin(),input.end());
    filter f4(std::move(f3),allocator_type{2025});
    BOOST_TEST_EQ(f3.capacity(),0);
    BOOST_TEST_EQ(f3.hash_function().state,-1);
    BOOST_TEST_EQ(f3.get_allocator().state,2025);
    BOOST_TEST_GE(f4.capacity(),1000);
    BOOST_TEST_EQ(f4.hash_function().state,42);
    BOOST_TEST_EQ(f4.get_allocator().state,2025);
    BOOST_TEST_EQ(f4.get_allocator().last_allocation,nullptr);
    BOOST_TEST(may_contain(f4,input));
  }
  {
    filter f(il,1000);
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(il,100,0.01);
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(il,1000,hasher{42});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(il,100,0.01,hasher{42});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,0);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(il,1000,hasher{42},allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(il,100,0.01,hasher{42},allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,42);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(1000,allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
  }
  {
    filter f(100,0.01,allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
  }
  {
    filter f(il,1000,allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),1000);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,il));
  }
  {
    filter f(il,100,0.01,allocator_type{2025});
    BOOST_TEST_GE(f.capacity(),0);
    BOOST_TEST_EQ(f.hash_function().state,0);
    BOOST_TEST_EQ(f.get_allocator().state,2025);
    BOOST_TEST(may_contain(f,il));
  }
  test_pocxx<Filter,ValueFactory,std::false_type,std::false_type>();
  test_pocxx<Filter,ValueFactory,std::false_type,std::true_type >();
  test_pocxx<Filter,ValueFactory,std::true_type, std::false_type>();
  test_pocxx<Filter,ValueFactory,std::true_type, std::true_type >();
  {
    std::vector<value_type> partial_input(input.begin()+il.size(),input.end());
    filter f(partial_input.begin(),partial_input.end(),1000);
    BOOST_TEST(may_contain(f,partial_input));
    f=il;
    BOOST_TEST(may_contain(f,il));
    BOOST_TEST(may_not_contain(f,partial_input));
  }
}

struct allocator_only_constructible
{
  allocator_only_constructible()=delete;
  ~allocator_only_constructible()=delete;

  int n;
};

struct allocator_only_constructible_hash
{
  using is_transparent=void;

  std::size_t operator()(const allocator_only_constructible& x)const
  {
    return (*this)(x.n);
  }

  std::size_t operator()(int n)const
  {
    return boost::hash<int>{}(n);
  }
};

template<typename T>
struct constructing_allocator
{
  using value_type=T;

  constructing_allocator()=default;
  template<typename U>
  constructing_allocator(const constructing_allocator<U>&){}

  T* allocate(std::size_t n)
  {
    return static_cast<T*>(::operator new(n*sizeof(T)));
  }

  void deallocate(T* p,std::size_t){::operator delete(p);}

  void construct(allocator_only_constructible* p,int n){p->n=n;}
  void destroy(allocator_only_constructible* p){}

  bool operator==(const constructing_allocator& x)const{return true;}
  bool operator!=(const constructing_allocator& x)const{return false;}
};

void test_allocator_aware_construction()
{
  using value_type=allocator_only_constructible;
  using filter=boost::bloom::filter<
    value_type,5,boost::bloom::block<unsigned char,1>,0,
    allocator_only_constructible_hash,constructing_allocator<value_type>
  >;

  filter f(1000);
  f.emplace(42);
  BOOST_TEST(f.may_contain(42));
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;
    using value_type=typename filter::value_type;

    test_construction<filter,value_factory<value_type>>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  test_allocator_aware_construction();
  return boost::report_errors();
}
