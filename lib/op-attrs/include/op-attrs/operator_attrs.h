#ifndef _OPERATOR_PARAMS_H
#define _OPERATOR_PARAMS_H

#include "ops/aggregate.h"
#include "ops/aggregate_spec.h"
#include "ops/attention.h"
#include "ops/batch_matmul.h"
#include "ops/cast.h"
#include "ops/concat.h"
#include "ops/conv_2d.h"
#include "ops/dropout.h"
#include "ops/element_binary.h"
#include "ops/element_unary.h"
#include "ops/embedding.h"
#include "ops/flat.h"
#include "ops/gather.h"
#include "ops/groupby.h"
#include "ops/layer_norm.h"
#include "ops/linear.h"
#include "ops/noop.h"
#include "ops/pool_2d.h"
#include "ops/reshape.h"
#include "ops/softmax.h"
#include "ops/split.h"
#include "ops/transpose.h"
#include "ops/combine.h"
#include "ops/fused_parallel_op.h"
#include "ops/repartition.h"
#include "ops/reduce.h"
#include "ops/reduction.h"
#include "ops/replicate.h"
#include "ops/topk.h"
#include "utils/variant.h"
#include "ops/broadcast.h"

namespace FlexFlow {

using SharedOperatorAttrs = variant<
                                       AggregateAttrs,
                                       AggregateSpecAttrs,
                                       BatchMatmulAttrs,
                                       CastAttrs,
                                       ConcatAttrs,
                                       Conv2DAttrs,
                                       DropoutAttrs,
                                       ElementBinaryAttrs,
                                       ElementScalarUnaryAttrs,
                                       ElementUnaryAttrs,
                                       EmbeddingAttrs,
                                       FlatAttrs,
                                       GatherAttrs,
                                       Group_byAttrs,
                                       InputAttrs,
                                       LayerNormAttrs,
                                       LinearAttrs,
                                       MultiHeadAttentionAttrs,
                                       NoopAttrs,
                                       Pool2DAttrs,
                                       ReduceAttrs,
                                       ReshapeAttrs,
                                       SplitAttrs,
                                       SoftmaxAttrs,
                                       TopKAttrs,
                                       TransposeAttrs>;

using ParallelOperatorAttrs = variant<CombineAttrs,
                                      ReductionAttrs,
                                      RepartitionAttrs,
                                      ReplicateAttrs,
                                      FusedParallelOpAttrs
>;


using ComputationGraphAttrs = variant_join<SharedOperatorAttrs, variant<BroadcastAttrs>>;
using CompGraphOperatorAttrs = ComputationGraphAttrs;

using PCGOperatorAttrs = variant_join<SharedOperatorAttrs, ParallelOperatorAttrs>;

static_assert(is_equal_comparable<ComputationGraphAttrs>::value, "ComputationGraphAttrs must support ==");
static_assert(elements_satisfy<is_valid_opattr, ComputationGraphAttrs>::value, "");
static_assert(is_neq_comparable<ComputationGraphAttrs>::value, "ComputationGraphAttrs must support !=");
static_assert(is_lt_comparable<ComputationGraphAttrs>::value, "ComputationGraphAttrs must support <");
static_assert(is_hashable<ComputationGraphAttrs>::value, "ComputationGraphAttrs must be hashable");

static_assert(is_equal_comparable<PCGOperatorAttrs>::value, "PCGOperatorAttrs must support ==");
static_assert(is_neq_comparable<PCGOperatorAttrs>::value, "PCGOperatorAttrs must support !=");
static_assert(is_lt_comparable<PCGOperatorAttrs>::value, "PCGOperatorAttrs must support <");
static_assert(is_hashable<PCGOperatorAttrs>::value, "PCGOperatorAttrs must be hashable");

/* OperatorType get_op_type(CompGraphOperatorAttrs const &); */
/* OperatorType get_op_type(PCGOperatorAttrs const &); */

RecordFormatter as_dot(CompGraphOperatorAttrs const &);
RecordFormatter as_dot(PCGOperatorAttrs const &);

std::vector<ParallelTensorShape> get_output_shapes(PCGOperatorAttrs const &op_params,
                                                   std::vector<ParallelTensorShape> const &input_tensor_shapes);

bool is_parallel_op(PCGOperatorAttrs const &);
bool is_valid(PCGOperatorAttrs const &, std::vector<ParallelTensorShape> const &);

}

#endif