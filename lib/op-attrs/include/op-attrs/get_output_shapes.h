#ifndef _FLEXFLOW_INCLUDE_OP_ATTRS_GET_OUTPUT_SHAPES_H
#define _FLEXFLOW_INCLUDE_OP_ATTRS_GET_OUTPUT_SHAPES_H

#include "op-attrs/operator_attrs.h"
#include "utils/containers.h"
#include "op-attrs/parallel_tensor_shape.h"
#include "utils/optional.h"
#include "tensor_shape.h"

namespace FlexFlow {

template <typename T, typename Enable = void> struct has_unary_output_t : std::false_type { };
template <typename T, typename Enable = void> struct has_unary_input_t : std::false_type { };
template <typename T, typename Enable = void> struct has_binary_input_t : std::false_type { };

template <typename T, typename Enable = void> struct has_multi_output_t : std::true_type { };
template <typename T, typename Enable = void> struct has_multi_input_t : std::true_type { };

template <typename T>
struct has_multi_output_t<T, typename std::enable_if<has_unary_output_t<T>::value>::type> : std::false_type { };

template <typename T>
struct has_multi_input_t<T, typename std::enable_if<(has_unary_input_t<T>::value || has_binary_input_t<T>::value)>::type> : std::false_type { };

/* template <typename T, typename Enable = void> struct output_type_t { using type = std::vector<ParallelTensorShape>; }; */


template <> struct has_unary_output_t<AggregateAttrs> : std::true_type { };
template <> struct has_unary_output_t<AggregateSpecAttrs> : std::true_type { };

template <> struct has_unary_input_t<AggregateSpecAttrs> : std::true_type { };

template <typename T>
typename std::enable_if<has_unary_input_t<T>::value, bool>::type
is_valid(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  if (shapes.size() != 1) {
    return false;
  }

  return is_valid(t, get_only(shapes));
}

template <typename T>
typename std::enable_if<has_binary_input_t<T>::value, bool>::type
is_valid(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  if (shapes.size() != 2) {
    return false;
  }

  return is_valid(t, shapes.at(0), shapes.at(1));
}

template <typename T> 
typename std::enable_if<(has_unary_input_t<T>::value && has_unary_output_t<T>::value), ParallelTensorShape>::type
output_shapes(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  return output_shape(t, get_only(shapes));
}

template <typename T>
typename std::enable_if<(has_binary_input_t<T>::value && has_unary_output_t<T>::value), std::vector<ParallelTensorShape>>::type
output_shapes(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  assert (shapes.size() == 2);

  return {output_shape(t, shapes.at(0), shapes.at(1))};
}

TensorShape get_tensor_shape_unsafe(ParallelTensorShape const &);
std::vector<TensorShape> get_tensor_shapes_unsafe(std::vector<ParallelTensorShape> const &);

template <typename Attrs> TensorShape get_output_shape(Attrs const &attrs, TensorShape const &);
template <typename Attrs> TensorShape get_output_shape(Attrs const &attrs, TensorShape const &, TensorShape const &);
template <typename Attrs> TensorShape get_output_shape(Attrs const &attrs, std::vector<TensorShape> const &);
template <typename Attrs> std::vector<TensorShape> get_output_shapes(Attrs const &attrs, TensorShape const &);
template <typename Attrs> std::vector<TensorShape> get_output_shapes(Attrs const &attrs, TensorShape const &, TensorShape const &);
template <typename Attrs> std::vector<TensorShape> get_output_shapes(Attrs const &attrs, std::vector<TensorShape> const &);

TensorShape get_output_shape(AggregateAttrs const &,
                             TensorShape const &, 
                             TensorShape const &,
                             TensorShape const &,
                             TensorShape const &,
                             std::vector<TensorShape> const &);
ParallelTensorShape get_output_shape(AggregateAttrs const &, 
                                     ParallelTensorShape const &gate_preds,
                                     ParallelTensorShape const &gate_assign, 
                                     ParallelTensorShape const &true_gate_assign,
                                     ParallelTensorShape const &full_gate_gradients,
                                     std::vector<ParallelTensorShape> const &exp_preds);
ParallelTensorShape get_output_shape(AggregateSpecAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(MultiHeadAttentionAttrs const &, std::vector<ParallelTensorShape> const &);
ParallelTensorShape get_output_shape(BatchMatmulAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(CastAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(CombineAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(ConcatAttrs const &, std::vector<ParallelTensorShape> const &);
ParallelTensorShape get_output_shape(Conv2DAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(DropoutAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(ElementBinaryAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(ElementUnaryAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(EmbeddingAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(FlatAttrs const &, ParallelTensorShape const &);
std::vector<ParallelTensorShape> get_output_shapes(GatherAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(Group_byAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(LayerNormAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(LinearAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(Pool2DAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(ReduceAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(ReductionAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(RepartitionAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(ReplicateAttrs const &, ParallelTensorShape const &);
std::vector<ParallelTensorShape> get_output_shapes(SplitAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(SoftmaxAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(TopKAttrs const &, ParallelTensorShape const &);
ParallelTensorShape get_output_shape(TransposeAttrs const &, ParallelTensorShape const &);

struct GetOutputShapesFunctor { 
  GetOutputShapesFunctor(std::vector<ParallelTensorShape> const &s)
    : s(s) { }

  std::vector<ParallelTensorShape> const &s;

  template <typename T>
  std::vector<ParallelTensorShape> operator()(T const &t)  {
    return get_output_shapes(t, s);
  }
};

template <typename ...Ts> 
std::vector<ParallelTensorShape> get_output_shapes(variant<Ts...> const &t, std::vector<ParallelTensorShape> const &s) {
  return get_output_shape(GetOutputShapesFunctor{s}, t);
}


template <typename T>
typename std::enable_if<!has_unary_output_t<T>::value, optional<int>>::type get_num_outputs(T const &) { return nullopt; }

template <typename T>
typename std::enable_if<has_unary_output_t<T>::value, optional<int>>::type get_num_outputs(T const &) { return 1; }

int get_num_outputs(SplitAttrs const &attrs);

template <typename T>
bool is_valid(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  auto num_outputs = get_num_outputs(t);
  if (num_outputs.has_value() && shapes.size() != num_outputs.value()) {
    return false;
  }

  for (ParallelTensorShape const &shape : shapes) {
    if (!is_valid(shape)) {
      return false;
    }
  }

  return is_valid_internal(t, shapes);
}

template <typename T>
typename std::enable_if<has_unary_input_t<T>::value, bool>::type 
is_valid_internal(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  return is_valid_internal(t, get_only(shapes));
}

template <typename T>
typename std::enable_if<has_binary_input_t<T>::value, bool>::type 
is_valid_internal(T const &t, std::vector<ParallelTensorShape> const &shapes) {
  return is_valid_internal(t, shapes.at(0), shapes.at(1));
}

bool is_valid_internal(AggregateAttrs const &, std::vector<ParallelTensorShape> const &);
bool is_valid_internal(AggregateSpecAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(MultiHeadAttentionAttrs const &, std::vector<ParallelTensorShape> const &);
bool is_valid_internal(BatchMatmulAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
bool is_valid_internal(CastAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(ConcatAttrs const &, std::vector<ParallelTensorShape> const &);
bool is_valid_internal(Conv2DAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(DropoutAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(ElementBinaryAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
bool is_valid_internal(ElementUnaryAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(EmbeddingAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(FlatAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(GatherAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
bool is_valid_internal(Group_byAttrs const &, ParallelTensorShape const &, ParallelTensorShape const &);
bool is_valid_internal(LayerNormAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(LinearAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(Pool2DAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(ReduceAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(ReductionAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(RepartitionAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(ReplicateAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(ReshapeAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(SoftmaxAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(SplitAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(TopKAttrs const &, ParallelTensorShape const &);
bool is_valid_internal(TransposeAttrs const &, ParallelTensorShape const &);

}

#endif