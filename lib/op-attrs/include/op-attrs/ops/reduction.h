#ifndef _FLEXFLOW_REDUCTION_ATTRS_H
#define _FLEXFLOW_REDUCTION_ATTRS_H

#include "op-attrs/parallel_tensor_shape.h"
#include "op-attrs/ops/unary_op.h"
#include "utils/visitable.h"

namespace FlexFlow {

struct ReductionAttrs {
public:
  ReductionAttrs(int dim, int degree);
public:
  int reduction_legion_dim;
  int reduction_degree;
};
bool operator==(ReductionAttrs const &, ReductionAttrs const &);
bool operator<(ReductionAttrs const &, ReductionAttrs const &);

}

VISITABLE_STRUCT(::FlexFlow::ReductionAttrs, reduction_legion_dim, reduction_degree);

namespace std {
template <>
struct hash<::FlexFlow::ReductionAttrs> {
  size_t operator()(::FlexFlow::ReductionAttrs const &) const;
};
} 

#endif 