# Candidate Boost Bloom Library

[![Branch](https://img.shields.io/badge/branch-master-brightgreen.svg)](https://github.com/joaquintides/bloom/tree/master) [![CI](https://github.com/joaquintides/bloom/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/joaquintides/bloom/actions/workflows/ci.yml) [![Drone status](https://img.shields.io/drone/build/joaquintides/bloom/master?server=https%3A%2F%2Fdrone.cpp.al&logo=drone&logoColor=%23CCCCCC&label=CI)](https://drone.cpp.al/joaquintides/bloom) [![codecov](https://codecov.io/gh/joaquintides/bloom/branch/master/graph/badge.svg)](https://app.codecov.io/gh/joaquintides/bloom/tree/master) [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](https://master.bloom.cpp.al/) </br>
[![Branch](https://img.shields.io/badge/branch-develop-brightgreen.svg)](https://github.com/joaquintides/bloom/tree/develop) [![CI](https://github.com/joaquintides/bloom/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/joaquintides/bloom/actions/workflows/ci.yml) [![Drone status](https://img.shields.io/drone/build/joaquintides/bloom/develop?server=https%3A%2F%2Fdrone.cpp.al&logo=drone&logoColor=%23CCCCCC&label=CI)](https://drone.cpp.al/joaquintides/bloom) [![codecov](https://codecov.io/gh/joaquintides/bloom/branch/develop/graph/badge.svg)](https://app.codecov.io/gh/joaquintides/bloom/tree/develop) [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](https://develop.bloom.cpp.al/) </br>
[![BSL 1.0](https://img.shields.io/badge/license-BSL_1.0-blue.svg)](https://www.boost.org/users/license.html) <img alt="C++11 required" src="https://img.shields.io/badge/standard-C%2b%2b11-blue.svg"> <img alt="Header-only library" src="https://img.shields.io/badge/build-header--only-blue.svg">

(Candidate) Boost.Bloom provides the class template `boost::bloom::filter` that
can be configured to implement a classical [Bloom filter](https://en.wikipedia.org/wiki/Bloom_filter)
as well as variations discussed in the literature such as block filters, multiblock filters, and more.

```cpp
#include <boost/bloom/filter.hpp>
#include <cassert>
#include <string>

int main()
{
  // Bloom filter of strings with 5 bits set per insertion
  using filter = boost::bloom::filter<std::string, 5>;

  // create filter with a capacity of 1'000'000 **bits**
  filter f(1'000'000);

  // insert elements (they can't be erased, Bloom filters are insert-only)
  f.insert("hello");
  f.insert("Boost");
  //...

  // elements inserted are always correctly checked as such
  assert(f.may_contain("hello") == true);

  // elements not inserted may incorrectly be identified as such with a
  // false positive rate (FPR) which is a function of the array capacity,
  // the number of bits set per element and generally how the boost::bloom::filter
  // was specified
  if(f.may_contain("bye")) { // likely false
    //...
  }
}
```

## Learn about Boost.Bloom

* [Online documentation](https://develop.bloom.cpp.al/)
* [Some benchmarks](https://github.com/joaquintides/boost_bloom_benchmarks)

## Install Boost.Bloom

Clone this repo, adjust your include paths  and enjoy.
Boost.Bloom is header-only and requires no building. A recent version of Boost is required.

## Support

* Join the **#boost** discussion group at [cpplang.slack.com](https://cpplang.slack.com/)
([ask for an invite](https://cppalliance.org/slack/) if you’re not a member of this workspace yet)
* [File an issue](https://github.com/joaquintides/bloom/issues)

## Contribute

* [Pull requests](https://github.com/joaquintides/bloom/pulls) against **develop** branch are most welcome.
Note that by submitting patches you agree to license your modifications under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).
