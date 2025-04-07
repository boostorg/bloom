/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11/algorithm.hpp>
#include <climits>
#include <cstring>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename Filter,typename ValueFactory>
void test_data()
{
  using filter=Filter;

  ValueFactory fac;

  {
    filter        f;
    const filter& cf=f;
    BOOST_TEST_EQ(f.data(),cf.data());
  }
  {
    filter        f(1000);
    const filter& cf=f;
    BOOST_TEST_NE(f.data(),nullptr);
    BOOST_TEST_EQ(f.data(),cf.data());
  }
  {
    filter f1(1000),f2(1000);
    for(int i=0;i<10;++i)f1.insert(fac());
    std::memcpy(f2.data(),f1.data(),f1.capacity()/CHAR_BIT);
    BOOST_TEST_NE(f1.data(),f2.data());
    BOOST_TEST(f1==f2);
  }
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;
    using value_type=typename filter::value_type;

    test_data<filter,value_factory<value_type>>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  return boost::report_errors();
}
