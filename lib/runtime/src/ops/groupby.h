#ifndef _FLEXFLOW_GROUPBY_H_
#define _FLEXFLOW_GROUPBY_H_

#include "operator.h"
#include "layer.h"

namespace FlexFlow {

class Group_by : public Op {
public:
  Group_by(FFModel &model,
           const ParallelTensor _input,
           const ParallelTensor _assign,
           int _n,
           float _alpha,
           char const *name);
  Group_by(FFModel &model,
           Group_by const &other,
           const ParallelTensor input,
           const ParallelTensor assign);
  Group_by(FFModel &model,
           Params const &params,
           Input const &inputs,
           char const *name = nullptr);
  void init(FFModel const &) override;
  void forward(FFModel const &) override;
  void backward(FFModel const &) override;
  void print_layer(FFModel const &model) override {
    assert(0);
  }
  static Op *
      create_operator_from_layer(FFModel &model,
                                 Layer const *layer,
                                 std::vector<ParallelTensor> const &inputs);
  static PerDeviceOpState *init_task(Legion::Task const *task,
                           std::vector<Legion::PhysicalRegion> const &regions,
                           Legion::Context ctx,
                           Legion::Runtime *runtime);
  static void forward_task(Legion::Task const *task,
                           std::vector<Legion::PhysicalRegion> const &regions,
                           Legion::Context ctx,
                           Legion::Runtime *runtime);
  static void backward_task(Legion::Task const *task,
                            std::vector<Legion::PhysicalRegion> const &regions,
                            Legion::Context ctx,
                            Legion::Runtime *runtime);
  void serialize(Legion::Serializer &s) const override;
  static PCG::Node deserialize(FFModel &ff,
                               Legion::Deserializer &d,
                               ParallelTensor inputs[],
                               int num_inputs);
  Op *materialize(FFModel &ff,
                  ParallelTensor inputs[],
                  int num_inputs) const override;
  static void
      forward_kernel_wrapper(GroupByMeta const *m,
                             float const *input,
                             int const *exp_assign,
                             float **outputs,
                             int n,       // num experts
                             int k,       // chosen experts
                             float alpha, // factor additional memory assigned
                             int batch_size,
                             int data_dim);
  static void
      backward_kernel_wrapper(GroupByMeta const *m,
                              float *input_grad,
                              int const *exp_assign,
                              float **output_grads,
                              int n,       // num experts
                              int k,       // chosen experts
                              float alpha, // factor additional memory assigned
                              int batch_size,
                              int data_dim);
  bool measure_operator_cost(Simulator *sim,
                             MachineView const &pc,
                             CostMetrics &cost_metrics) const override;

public:
  int n;
  float alpha;
};

}

#endif