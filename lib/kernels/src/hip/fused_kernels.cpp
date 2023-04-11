/* Copyright 2023 CMU, Facebook, LANL, MIT, NVIDIA, and Stanford (alphabetical)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kernels/fused_kernels.h"
#include "kernels/batch_matmul_kernels.h"
#include "kernels/concat_kernels.h"
#include "kernels/conv_2d_kernels.h"
#include "kernels/element_binary_kernels.h"
#include "kernels/element_unary_kernels.h"
#include "kernels/linear_kernels.h"
#include "kernels/pool_2d_kernels.h"
#include "kernels/reshape_kernels.h"
#include "kernels/transpose_kernels.h"
#include "kernels/batch_norm_kernels.h"
#include "kernels/dropout_kernels.h"
#include "kernels/flat_kernels.h"
#include "kernels/linear_kernels.h"
#include "kernels/hip_helper.h"
#include <hip/hip_runtime.h>

namespace FlexFlow {
// declare Legion names
using Legion::Context;
using Legion::coord_t;
using Legion::Domain;
using Legion::LogicalPartition;
using Legion::LogicalRegion;
using Legion::PhysicalRegion;
using Legion::PointInRectIterator;
using Legion::Rect;
using Legion::Runtime;
using Legion::Task;

OpPerDeviceState *FusedOp::init_task(Task const *task,
                           std::vector<PhysicalRegion> const &regions,
                           Context ctx,
                           Runtime *runtime) {
  FusedOp const *fused = (FusedOp *)task->args;
  FusedOpPerDeviceState const *metas = (FusedOpPerDeviceState *)task->local_args;
  FusedOpPerDeviceState *local_meta = new FusedOpPerDeviceState();
  memcpy(local_meta, metas, sizeof(FusedOpPerDeviceState));
  local_meta->fused_op = (FusedOp *)malloc(sizeof(FusedOp));
  memcpy(static_cast<void *>(local_meta->fused_op),
         static_cast<void const *>(fused),
         sizeof(FusedOp));
  return ((OpPerDeviceState *)local_meta);
}

/*
  regions[...](I): inputs
  regions[...](I): weights
  regions[...](I): outputs
*/
__host__ void FusedOp::forward_task(Task const *task,
                                    std::vector<PhysicalRegion> const &regions,
                                    Context ctx,
                                    Runtime *runtime) {
  // const FusedOp* fused = (FusedOp*) task->args;
  FusedOpPerDeviceState const *metas = *((FusedOpPerDeviceState **)task->local_args);
  FusedOp const *fused = metas->fused_op;
  assert(metas->numOperators == fused->numOperators);
  assert(regions.size() == task->regions.size());
  assert((int)regions.size() ==
         fused->numInputs + fused->numWeights + fused->numOutputs);
  GenericTensorAccessorR input_accessor[MAX_NUM_INPUTS];
  GenericTensorAccessorR weight_accessor[MAX_NUM_WEIGHTS];
  GenericTensorAccessorW output_accessor[MAX_NUM_OUTPUTS];
  assert(fused->numInputs <= MAX_NUM_INPUTS);
  for (int i = 0; i < fused->numInputs; i++) {
    input_accessor[i] =
        helperGetGenericTensorAccessorRO(fused->input_data_types[i],
                                         regions[i],
                                         task->regions[i],
                                         FID_DATA,
                                         ctx,
                                         runtime);
  }
  int roff = fused->numInputs;
  assert(fused->numWeights <= MAX_NUM_WEIGHTS);
  for (int i = 0; i < fused->numWeights; i++) {
    weight_accessor[i] =
        helperGetGenericTensorAccessorRO(fused->weight_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
  }
  roff += fused->numWeights;
  assert(fused->numOutputs <= MAX_NUM_OUTPUTS);
  for (int i = 0; i < fused->numOutputs; i++) {
    output_accessor[i] =
        helperGetGenericTensorAccessorWO(fused->output_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
  }
  // Assert that all meta share the same dnn/blas handler
  int start = 0;
  for (start = 0; start < fused->numOperators; start++) {
    if (metas->meta[start] != NULL) {
      break;
    }
  }
  for (int op = start + 1; op < fused->numOperators; op++) {
    if (metas->meta[op] != NULL) {
      assert(metas->meta[start]->handle.blas == metas->meta[op]->handle.blas);
      assert(metas->meta[start]->handle.dnn == metas->meta[op]->handle.dnn);
    }
  }

  hipStream_t stream;
  if (start < fused->numOperators) {
    
  }

  int ioff = 0, woff = 0, ooff = 0;
  for (int op = 0; op < fused->numOperators; op++) {
    GenericTensorAccessorR my_input_accessor[MAX_NUM_INPUTS];
    GenericTensorAccessorR my_weight_accessor[MAX_NUM_WEIGHTS];
    GenericTensorAccessorW my_output_accessor[MAX_NUM_OUTPUTS];
    for (int i = 0; i < fused->op_num_inputs[op]; i++) {
      int my_off = fused->op_input_idx[i + ioff];
      if (fused->op_input_source[i + ioff] == SOURCE_INPUT) {
        my_input_accessor[i] = input_accessor[my_off];
      } else if (fused->op_input_source[i + ioff] == SOURCE_OUTPUT) {
        my_input_accessor[i] = output_accessor[my_off];
      } else {
        assert(false);
      }
    }
    for (int i = 0; i < fused->op_num_weights[op]; i++) {
      assert(fused->op_weight_source[i + woff] == SOURCE_WEIGHT);
      my_weight_accessor[i] = weight_accessor[fused->op_weight_idx[i + woff]];
    }
    for (int i = 0; i < fused->op_num_outputs[op]; i++) {
      assert(fused->op_output_source[i + ooff] == SOURCE_OUTPUT);
      my_output_accessor[i] = output_accessor[i + ooff];
    }
    switch (fused->op_op_type[op]) {
      case OP_CONCAT: {
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        ConcatPerDeviceState *m = (ConcatPerDeviceState *)metas->meta[op];
        int num_inputs = fused->op_num_inputs[op];
        Kernels::Concat::forward_kernel(m,
                                                my_output_accessor[0],
                                                my_input_accessor,
                                                num_inputs,
                                                m->legion_axis);
        break;
      }
      case OP_CONV2D: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_dim() == 5);
        assert(my_weight_accessor[0].domain.get_dim() == 5);
        assert(my_output_accessor[0].domain.get_dim() == 5);
        Conv2DPerDeviceState *m = (Conv2DPerDeviceState *)metas->meta[op];
        Kernels::Conv2D::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_weight_accessor[0].get_float_ptr(),
            my_weight_accessor[1].get_float_ptr());
        break;
      }
      case OP_BATCHNORM: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_dim() == 5);
        assert(my_output_accessor[0].domain.get_dim() == 5);
        assert(my_weight_accessor[0].domain.get_dim() == 2);
        assert(my_weight_accessor[1].domain.get_dim() == 2);
        BatchNormPerDeviceState *m = (BatchNormPerDeviceState *)metas->meta[op];
        BatchNorm::forward_kernel(m,
                                  my_input_accessor[0].get_float_ptr(),
                                  my_output_accessor[0].get_float_ptr(),
                                  my_weight_accessor[0].get_float_ptr(),
                                  my_weight_accessor[1].get_float_ptr());
        break;
      }
      case OP_DROPOUT: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        DropoutPerDeviceState *m = (DropoutPerDeviceState *)metas->meta[op];
        Kernels::Dropout::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr());
        break;
      }
      case OP_LINEAR: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        Domain kernel_domain = my_weight_accessor[0].domain;
        int in_dim = kernel_domain.hi()[0] - kernel_domain.lo()[0] + 1;
        int out_dim = kernel_domain.hi()[1] - kernel_domain.lo()[1] + 1;
        int batch_size = my_input_accessor[0].domain.get_volume() / in_dim;
        assert(my_output_accessor[0].domain.get_volume() ==
               out_dim * batch_size);
        assert(my_input_accessor[0].domain.get_volume() == in_dim * batch_size);
        float const *bias_ptr = nullptr;
        if (fused->op_num_weights[op] == 2) {
          assert(my_weight_accessor[1].domain.get_volume() == out_dim);
          bias_ptr = my_weight_accessor[1].get_float_ptr();
        } else {
          assert(fused->op_num_weights[op] == 1);
        }
        LinearPerDeviceState *m = (LinearPerDeviceState *)metas->meta[op];
        Kernels::Linear::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_weight_accessor[0].get_float_ptr(),
            bias_ptr,
            in_dim,
            out_dim,
            batch_size);
        break;
      }
      case OP_BATCHMATMUL: {
        assert(fused->op_num_inputs[op] == 2);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        Domain out_domain = my_output_accessor[0].domain;
        Domain a_domain = my_input_accessor[0].domain;
        Domain b_domain = my_input_accessor[1].domain;
        int m = b_domain.hi()[0] - b_domain.lo()[0] + 1;
        assert(m == out_domain.hi()[0] - out_domain.lo()[0] + 1);
        int n = a_domain.hi()[1] - a_domain.lo()[1] + 1;
        assert(n == out_domain.hi()[1] - out_domain.lo()[1] + 1);
        int k = a_domain.hi()[0] - a_domain.lo()[0] + 1;
        assert(k == b_domain.hi()[1] - b_domain.lo()[1] + 1);
        assert(a_domain.get_dim() == b_domain.get_dim());
        assert(a_domain.get_dim() == out_domain.get_dim());
        int batch = 1;
        for (int i = 2; i < a_domain.get_dim(); i++) {
          int dim_size = a_domain.hi()[i] - a_domain.lo()[i] + 1;
          assert(dim_size == b_domain.hi()[i] - b_domain.lo()[i] + 1);
          assert(dim_size == out_domain.hi()[i] - out_domain.lo()[i] + 1);
          batch *= dim_size;
        }
        BatchMatmulPerDeviceState *meta = (BatchMatmulPerDeviceState *)metas->meta[op];
        Kernels::BatchMatmul::forward_kernel(
            meta,
            my_output_accessor[0].get_float_ptr(),
            my_input_accessor[0].get_float_ptr(),
            my_input_accessor[1].get_float_ptr(),
            (float const *)nullptr,
            m,
            n,
            k,
            batch,
            meta->a_seq_length_dim,
            meta->b_seq_length_dim,
            fused->iter_config.seq_length);
        break;
      }
      case OP_EW_ADD:
      case OP_EW_SUB:
      case OP_EW_MUL:
      case OP_EW_DIV:
      case OP_EW_MAX:
      case OP_EW_MIN: {
        assert(fused->op_num_inputs[op] == 2);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain == my_input_accessor[1].domain);
        assert(my_input_accessor[0].domain == my_output_accessor[0].domain);
        ElementBinaryPerDeviceState *m = (ElementBinaryPerDeviceState *)metas->meta[op];
        Kernels::ElementBinary::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_input_accessor[1].get_float_ptr(),
            my_output_accessor[0].get_float_ptr());
        break;
        break;
      }
      case OP_RELU:
      case OP_SIGMOID:
      case OP_TANH:
      case OP_ELU: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain == my_output_accessor[0].domain);
        ElementUnaryPerDeviceState *m = (ElementUnaryPerDeviceState *)metas->meta[op];
        Kernels::ElementUnary::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_input_accessor[0].domain.get_volume());
        break;
      }
      case OP_POOL2D: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        Pool2DPerDeviceState *m = (Pool2DPerDeviceState *)metas->meta[op];
        Kernels::Pool2D::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr());
        break;
      }
      case OP_FLAT: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_volume() ==
               my_output_accessor[0].domain.get_volume());
        Kernels::Flat::forward_kernel(
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_input_accessor[0].domain.get_volume());
        break;
      }
      case OP_RESHAPE: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_volume() ==
               my_output_accessor[0].domain.get_volume());
        Kernels::Reshape::forward_kernel(
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_input_accessor[0].domain.get_volume());
        break;
      }
      case OP_TRANSPOSE: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_volume() ==
               my_output_accessor[0].domain.get_volume());
        TransposePerDeviceState *m = (TransposePerDeviceState *)metas->meta[op];
        Kernels::Transpose::forward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_input_accessor[0].domain,
            my_output_accessor[0].domain);
        break;
      }
      default: {
        fprintf(stderr,
                "Fusion currently does not support type = %d\n",
                fused->op_op_type[op]);
        assert(false && "Fusion currently does not support type");
      }
    }
    ioff += fused->op_num_inputs[op];
    woff += fused->op_num_weights[op];
    ooff += fused->op_num_outputs[op];
  }
  // for (int i = 0; i < fused->numOutputs; i++)
  //   print_tensor<float>(output_ptr[i], output_domain[i].get_volume(),
  //   "[Fused:forward:output]");
}

/*
  regions[...](I): input
  regions[...](I): weight
  regions[...](I): output
  regions[...](I/O): input_grad
  regions[...](I/O): weight_grad
  regions[...](I/O): output_grad
*/

__host__ void FusedOp::backward_task(Task const *task,
                                     std::vector<PhysicalRegion> const &regions,
                                     Context ctx,
                                     Runtime *runtime) {
  // const FusedOp* fused = (FusedOp*) task->args;
  FusedOpPerDeviceState const *metas = *((FusedOpPerDeviceState **)task->local_args);
  FusedOp const *fused = metas->fused_op;

  assert(metas->numOperators == fused->numOperators);
  assert(regions.size() == task->regions.size());
  {
    int sum = fused->numInputs + fused->numWeights + fused->numOutputs;
    assert(sum * 2 == (int)regions.size());
  }
  GenericTensorAccessorR input_accessor[MAX_NUM_INPUTS];
  GenericTensorAccessorW input_grad_accessor[MAX_NUM_INPUTS];
  GenericTensorAccessorR weight_accessor[MAX_NUM_WEIGHTS];
  GenericTensorAccessorW weight_grad_accessor[MAX_NUM_WEIGHTS];
  GenericTensorAccessorR output_accessor[MAX_NUM_OUTPUTS];
  GenericTensorAccessorW output_grad_accessor[MAX_NUM_OUTPUTS];
  int roff = 0;
  assert(fused->numInputs <= MAX_NUM_INPUTS);
  for (int i = 0; i < fused->numInputs; i++) {
    input_accessor[i] =
        helperGetGenericTensorAccessorRO(fused->input_data_types[i],
                                         regions[i],
                                         task->regions[i],
                                         FID_DATA,
                                         ctx,
                                         runtime);
  }
  roff += fused->numInputs;
  assert(fused->numWeights <= MAX_NUM_WEIGHTS);
  for (int i = 0; i < fused->numWeights; i++) {
    weight_accessor[i] =
        helperGetGenericTensorAccessorRO(fused->weight_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
  }
  roff += fused->numWeights;
  assert(fused->numOutputs <= MAX_NUM_OUTPUTS);
  for (int i = 0; i < fused->numOutputs; i++) {
    output_accessor[i] =
        helperGetGenericTensorAccessorRO(fused->output_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
  }
  roff += fused->numOutputs;
  for (int i = 0; i < fused->numInputs; i++) {
    input_grad_accessor[i] =
        helperGetGenericTensorAccessorRW(fused->input_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
    assert(input_grad_accessor[i].domain == input_accessor[i].domain);
  }
  roff += fused->numInputs;
  for (int i = 0; i < fused->numWeights; i++) {
    weight_grad_accessor[i] =
        helperGetGenericTensorAccessorRW(fused->weight_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
    assert(weight_grad_accessor[i].domain.get_volume() ==
           weight_accessor[i].domain.get_volume());
  }
  roff += fused->numWeights;
  for (int i = 0; i < fused->numOutputs; i++) {
    output_grad_accessor[i] =
        helperGetGenericTensorAccessorRW(fused->output_data_types[i],
                                         regions[i + roff],
                                         task->regions[i + roff],
                                         FID_DATA,
                                         ctx,
                                         runtime);
    assert(output_grad_accessor[i].domain == output_accessor[i].domain);
  }
  roff += fused->numOutputs;
  // Assert that all meta share the same dnn/blas handler
  int start = 0;
  for (start = 0; start < fused->numOperators; start++) {
    if (metas->meta[start] != NULL) {
      break;
    }
  }
  for (int op = start + 1; op < fused->numOperators; op++) {
    if (metas->meta[op] != NULL) {
      assert(metas->meta[start]->handle.blas == metas->meta[op]->handle.blas);
      assert(metas->meta[start]->handle.dnn == metas->meta[op]->handle.dnn);
    }
  }

  hipStream_t stream;
  

  int ioff = 0, woff = 0, ooff = 0;
  GenericTensorAccessorR my_input_accessor[MAX_NUM_INPUTS];
  GenericTensorAccessorR my_weight_accessor[MAX_NUM_WEIGHTS];
  GenericTensorAccessorR my_output_accessor[MAX_NUM_OUTPUTS];
  GenericTensorAccessorW my_input_grad_accessor[MAX_NUM_INPUTS];
  GenericTensorAccessorW my_weight_grad_accessor[MAX_NUM_WEIGHTS];
  GenericTensorAccessorW my_output_grad_accessor[MAX_NUM_OUTPUTS];
  // Do backpropagation in the reverse ordering
  for (int op = 0; op < fused->numOperators; op++) {
    ioff += fused->op_num_inputs[op];
    woff += fused->op_num_weights[op];
    ooff += fused->op_num_outputs[op];
  }

  for (int op = fused->numOperators - 1; op >= 0; op--) {
    ioff -= fused->op_num_inputs[op];
    woff -= fused->op_num_weights[op];
    ooff -= fused->op_num_outputs[op];
    for (int i = 0; i < fused->op_num_inputs[op]; i++) {
      int my_off = fused->op_input_idx[i + ioff];
      if (fused->op_input_source[i + ioff] == SOURCE_INPUT) {
        my_input_accessor[i] = input_accessor[my_off];
        my_input_grad_accessor[i] = input_grad_accessor[my_off];
      } else if (fused->op_input_source[i + ioff] == SOURCE_OUTPUT) {
        my_input_accessor[i] = output_accessor[my_off];
        my_input_grad_accessor[i] = output_grad_accessor[my_off];
        assert(my_input_grad_accessor[i].domain == my_input_accessor[i].domain);
      } else {
        assert(false);
      }
    }
    for (int i = 0; i < fused->op_num_weights[op]; i++) {
      assert(fused->op_weight_source[i + woff] == SOURCE_WEIGHT);
      my_weight_accessor[i] = weight_accessor[fused->op_weight_idx[i + woff]];
      my_weight_grad_accessor[i] =
          weight_grad_accessor[fused->op_weight_idx[i + woff]];
      assert(my_weight_grad_accessor[i].domain.get_volume() ==
             my_weight_accessor[i].domain.get_volume());
    }
    for (int i = 0; i < fused->op_num_outputs[op]; i++) {
      assert(fused->op_output_source[i + ooff] == SOURCE_OUTPUT);
      my_output_accessor[i] = output_accessor[fused->op_output_idx[i + ooff]];
      my_output_grad_accessor[i] =
          output_grad_accessor[fused->op_output_idx[i + ooff]];
      assert(my_output_grad_accessor[i].domain == my_output_accessor[i].domain);
    }
    switch (fused->op_op_type[op]) {
      case OP_CONCAT: {
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        ConcatPerDeviceState *m = (ConcatPerDeviceState *)metas->meta[op];
        int num_inputs = fused->op_num_inputs[op];
        Kernels::Concat::backward_kernel(m,
                                                 my_output_grad_accessor[0],
                                                 my_input_grad_accessor,
                                                 num_inputs,
                                                 m->legion_axis);
        break;
      }
      case OP_CONV2D: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_dim() == 5);
        assert(my_weight_accessor[0].domain.get_dim() == 5);
        assert(my_output_accessor[0].domain.get_dim() == 5);
        Conv2DPerDeviceState *m = (Conv2DPerDeviceState *)metas->meta[op];
        Kernels::Conv2D::backward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr(),
            my_weight_accessor[0].get_float_ptr(),
            my_weight_grad_accessor[0].get_float_ptr(),
            my_weight_grad_accessor[1].get_float_ptr());
        break;
      }
      case OP_BATCHNORM: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain.get_dim() == 5);
        assert(my_weight_accessor[0].domain.get_dim() == 2);
        assert(my_weight_accessor[1].domain.get_dim() == 2);
        assert(my_output_accessor[0].domain.get_dim() == 5);
        BatchNormPerDeviceState *m = (BatchNormPerDeviceState *)metas->meta[op];
        BatchNorm::backward_kernel(
            m,
            (float const *)my_input_accessor[0].get_float_ptr(),
            (float *)my_output_grad_accessor[0].get_float_ptr(),
            (float const *)my_output_accessor[0].get_float_ptr(),
            (float *)my_input_grad_accessor[0].get_float_ptr(),
            (float const *)my_weight_accessor[0].get_float_ptr(),
            (float *)my_weight_grad_accessor[0].get_float_ptr(),
            (float *)my_weight_grad_accessor[1].get_float_ptr(),
            my_output_accessor[0].domain.get_volume());
        break;
      }
      case OP_DROPOUT: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        DropoutPerDeviceState *m = (DropoutPerDeviceState *)metas->meta[op];
        Kernels::Dropout::backward_kernel(
            m,
            my_output_grad_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].get_float_ptr());
        break;
      }
      case OP_LINEAR: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_outputs[op] == 1);
        Domain kernel_domain = my_weight_accessor[0].domain;
        int in_dim = kernel_domain.hi()[0] - kernel_domain.lo()[0] + 1;
        int out_dim = kernel_domain.hi()[1] - kernel_domain.lo()[1] + 1;
        int batch_size = my_input_accessor[0].domain.get_volume() / in_dim;
        assert(my_output_accessor[0].domain.get_volume() ==
               out_dim * batch_size);
        assert(my_input_accessor[0].domain.get_volume() == in_dim * batch_size);
        float *bias_grad_ptr = nullptr;
        if (fused->op_num_weights[op] == 2) {
          assert(my_weight_accessor[1].domain.get_volume() == out_dim);
          bias_grad_ptr = my_weight_grad_accessor[1].get_float_ptr();
        } else {
          assert(fused->op_num_weights[op] == 1);
        }
        LinearPerDeviceState *m = (LinearPerDeviceState *)metas->meta[op];
        Kernels::Linear::backward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr(),
            my_weight_accessor[0].get_float_ptr(),
            my_weight_grad_accessor[0].get_float_ptr(),
            bias_grad_ptr,
            in_dim,
            out_dim,
            batch_size);
        break;
      }
      case OP_BATCHMATMUL: {
        assert(fused->op_num_inputs[op] == 2);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        Domain out_domain = my_output_accessor[0].domain;
        Domain a_domain = my_input_accessor[0].domain;
        Domain b_domain = my_input_accessor[1].domain;
        // check dims
        int m = b_domain.hi()[0] - b_domain.lo()[0] + 1;
        assert(m == out_domain.hi()[0] - out_domain.lo()[0] + 1);
        int n = a_domain.hi()[1] - a_domain.lo()[1] + 1;
        assert(n == out_domain.hi()[1] - out_domain.lo()[1] + 1);
        int k = a_domain.hi()[0] - a_domain.lo()[0] + 1;
        assert(k == b_domain.hi()[1] - b_domain.lo()[1] + 1);
        assert(a_domain.get_dim() == b_domain.get_dim());
        assert(a_domain.get_dim() == out_domain.get_dim());
        int batch = 1;
        for (int i = 2; i < a_domain.get_dim(); i++) {
          int dim_size = a_domain.hi()[i] - a_domain.lo()[i] + 1;
          assert(dim_size == b_domain.hi()[i] - b_domain.lo()[i] + 1);
          assert(dim_size == out_domain.hi()[i] - out_domain.lo()[i] + 1);
          batch *= dim_size;
        }
        BatchMatmulPerDeviceState *meta = (BatchMatmulPerDeviceState *)metas->meta[op];
        Kernels::BatchMatmul::backward_kernel(
            meta,
            (float const *)my_output_accessor[0].get_float_ptr(),
            (float const *)my_output_grad_accessor[0].get_float_ptr(),
            (float const *)my_input_accessor[0].get_float_ptr(),
            (float *)my_input_grad_accessor[0].get_float_ptr(),
            (float const *)my_input_accessor[1].get_float_ptr(),
            (float *)my_input_grad_accessor[1].get_float_ptr(),
            (float *)nullptr,
            m,
            n,
            k,
            batch);
        break;
      }
      case OP_EW_ADD:
      case OP_EW_SUB:
      case OP_EW_MUL:
      case OP_EW_DIV:
      case OP_EW_MAX:
      case OP_EW_MIN: {
        assert(fused->op_num_inputs[op] == 2);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain == my_input_accessor[1].domain);
        assert(my_input_accessor[0].domain == my_output_accessor[0].domain);
        ElementBinaryPerDeviceState *m = (ElementBinaryPerDeviceState *)metas->meta[op];
        Kernels::ElementBinary::backward_kernel(
            m,
            my_output_grad_accessor[0].get_float_ptr(),
            my_input_accessor[0].get_float_ptr(),
            my_input_accessor[1].get_float_ptr(),
            my_input_grad_accessor[0].get_float_ptr(),
            my_input_grad_accessor[1].get_float_ptr());
        break;
      }
      case OP_RELU:
      case OP_SIGMOID:
      case OP_TANH:
      case OP_ELU: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_accessor[0].domain == my_output_accessor[0].domain);
        ElementUnaryPerDeviceState *m = (ElementUnaryPerDeviceState *)metas->meta[op];
        Kernels::ElementUnary::backward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr(),
            my_input_accessor[0].domain.get_volume());
        break;
      }
      case OP_POOL2D: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        // assert(my_input_accessor[0].domain == my_output_accessor[0].domain);
        Pool2DPerDeviceState *m = (Pool2DPerDeviceState *)metas->meta[op];
        Kernels::Pool2D::backward_kernel(
            m,
            my_input_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr());
        break;
      }
      case OP_FLAT: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_grad_accessor[0].domain.get_volume() ==
               my_output_grad_accessor[0].domain.get_volume());
        Kernels::Flat::backward_kernel(
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].domain.get_volume());
        break;
      }
      case OP_RESHAPE: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_grad_accessor[0].domain.get_volume() ==
               my_output_grad_accessor[0].domain.get_volume());
        Kernels::Reshape::backward_kernel(
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].domain.get_volume());
        break;
      }
      case OP_TRANSPOSE: {
        assert(fused->op_num_inputs[op] == 1);
        assert(fused->op_num_weights[op] == 0);
        assert(fused->op_num_outputs[op] == 1);
        assert(my_input_grad_accessor[0].domain.get_volume() ==
               my_output_grad_accessor[0].domain.get_volume());
        TransposePerDeviceState *m = (TransposePerDeviceState *)metas->meta[op];
        Kernels::Transpose::backward_kernel(
            m,
            my_input_grad_accessor[0].get_float_ptr(),
            my_output_grad_accessor[0].get_float_ptr(),
            my_input_grad_accessor[0].domain,
            my_output_grad_accessor[0].domain);
        break;
      }
      default:
        assert(false && "Fusion currently does not support type");
    }
  }
  assert(ioff == 0);
  assert(woff == 0);
  assert(ooff == 0);
  // for (int i = 0; i < fused->numWeights; i++)
  //   print_tensor<float>(weight_grad_ptr[i],
  //   weight_grad_domain[i].get_volume(), "[Fused:backward:weight_grad]");
  // for (int i = 0; i < fused->numInputs; i++)
  //   print_tensor<float>(input_grad_ptr[i], input_grad_domain[i].get_volume(),
  //   "[Fused:backward:input_grad]");
  // for (int i = 0; i < fused->numOutputs; i++)
  //   print_tensor<float>(output_grad_ptr[i],
  //   output_grad_domain[i].get_volume(), "[Fused:backward:output_grad]");
}

}; // namespace FlexFlow