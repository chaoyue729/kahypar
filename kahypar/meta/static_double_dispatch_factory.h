/***************************************************************************
 *  Copyright (C) 2015-2016 Sebastian Schlag <sebastian.schlag@kit.edu>
 **************************************************************************/

#pragma once

#include "kahypar/meta/mandatory.h"
#include "kahypar/meta/typelist.h"

namespace kahypar {
namespace meta {
template <
  class Executor = Mandatory,
  class TypesLhs = Mandatory,
  class TypesRhs = TypesLhs,
  typename ResultType = void
  >
class StaticDoubleDispatchFactory {
  using Head = typename TypesLhs::Head;
  using Tail = typename TypesLhs::Tail;

 public:
  template <typename BaseTypeLhs,
            typename BaseTypeRhs,
            typename ... Parameters>
  static ResultType go(BaseTypeLhs&& lhs, BaseTypeRhs&& rhs, Executor exec,
                       Parameters&& ... parameters) {
    if (Head* p1 = dynamic_cast<Head*>(&lhs)) {
      return StaticDoubleDispatchFactory<Executor, TypesLhs, TypesRhs,
                                         ResultType>::dispatchRhs(
        *p1, rhs, exec,
        std::forward<Parameters>(parameters) ...);
    } else {
      return StaticDoubleDispatchFactory<Executor, Tail, TypesRhs,
                                         ResultType>::go(
        lhs, rhs, exec,
        std::forward<Parameters>(parameters) ...);
    }
  }

  template <class SomeLhs,
            typename BaseTypeRhs,
            typename ... Parameters>
  static ResultType dispatchRhs(SomeLhs&& lhs, BaseTypeRhs&& rhs, Executor exec,
                                Parameters&& ... parameters) {
    using Head = typename TypesRhs::Head;
    using Tail = typename TypesRhs::Tail;
    if (Head* p2 = dynamic_cast<Head*>(&rhs)) {
      return exec.fire(lhs, *p2, std::forward<Parameters>(parameters) ...);
    } else {
      return StaticDoubleDispatchFactory<Executor, TypesLhs, Tail,
                                         ResultType>::dispatchRhs(
        lhs, rhs, exec,
        std::forward<Parameters>(parameters) ...);
    }
  }
};

template <
  class Executor,
  class TypesRhs,
  typename ResultType>
class StaticDoubleDispatchFactory<Executor, NullType, TypesRhs, ResultType>{
 public:
  template <typename BaseTypeLhs,
            typename BaseTypeRhs,
            typename ... Parameters>
  static ResultType go(BaseTypeLhs&& lhs, BaseTypeRhs&& rhs, Executor exec,
                       Parameters&& ... parameters) {
    return exec.onError(lhs, rhs, std::forward<Parameters>(parameters) ...);
  }
};

template <
  class Executor,
  class TypesLhs,
  typename ResultType>
class StaticDoubleDispatchFactory<Executor, TypesLhs, NullType, ResultType>{
 public:
  template <typename BaseTypeLhs,
            typename BaseTypeRhs,
            typename ... Parameters>
  static ResultType dispatchRhs(BaseTypeLhs& lhs, BaseTypeRhs& rhs, Executor exec,
                                Parameters&& ... parameters) {
    return exec.onError(lhs, rhs, std::forward<Parameters>(parameters) ...);
  }
};
}  // namespace meta
}  // namespace kahypar
