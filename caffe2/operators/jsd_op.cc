/**
 * Copyright (c) 2016-present, Facebook, Inc.
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

#include "caffe2/operators/jsd_op.h"

namespace caffe2 {

namespace {

static constexpr float kLOG_THRESHOLD() {
  return 1e-20;
}

inline float logit(float p) {
  // it computes log(p / (1-p))
  // to avoid numeric issue, hard code p log(p) when p approaches 0
  float x = std::min(std::max(p, kLOG_THRESHOLD()), 1 - kLOG_THRESHOLD());
  return -log(1. / x - 1.);
}

inline float entropy(float p) {
  if (p < kLOG_THRESHOLD() || 1 - p < kLOG_THRESHOLD()) {
    return 0.;
  } else {
    float q = 1 - p;
    return -p * log(p) - q * log(q);
  }
}
} // namespace

template <>
bool BernoulliJSDOp<float, CPUContext>::RunOnDevice() {
  auto& X = Input(0); // predicted probabilities
  auto& T = Input(1); // target probabilities
  auto* L = Output(0); // JSD loss output
  int N = X.size();
  CAFFE_ENFORCE_EQ(T.size(), N);
  L->ResizeLike(X);
  auto* x_data = X.data<float>();
  auto* t_data = T.data<float>();
  auto* l_data = L->mutable_data<float>();
  for (int i = 0; i < N; i++) {
    auto p_mdl = x_data[i];
    auto p_emp = t_data[i];
    auto p_avg = (p_mdl + p_emp) / 2.;
    auto jsd = entropy(p_avg) - (entropy(p_mdl) + entropy(p_emp)) / 2.;
    l_data[i] = jsd;
  }
  return true;
}

template <>
bool BernoulliJSDGradientOp<float, CPUContext>::RunOnDevice() {
  auto& go = Input(0);
  auto& X = Input(1);
  auto& T = Input(2);
  auto* gi = Output(0);
  int N = X.size();
  gi->ResizeLike(X);
  auto* go_data = go.data<float>();
  auto* x_data = X.data<float>();
  auto* t_data = T.data<float>();
  auto* gi_data = gi->mutable_data<float>();
  for (int i = 0; i < N; i++) {
    auto p_mdl = x_data[i];
    auto p_emp = t_data[i];
    auto p_avg = (p_mdl + p_emp) / 2.;
    auto g_jsd = (logit(p_mdl) - logit(p_avg)) / 2.;
    gi_data[i] = go_data[i] * g_jsd;
  }
  return true;
}
REGISTER_CPU_OPERATOR(BernoulliJSD, BernoulliJSDOp<float, CPUContext>);
REGISTER_CPU_OPERATOR(
    BernoulliJSDGradient,
    BernoulliJSDGradientOp<float, CPUContext>);
OPERATOR_SCHEMA(BernoulliJSD)
    .NumInputs(2)
    .NumOutputs(1)
    .SetDoc(R"DOC(
Computes the Jensen-Shannon divergence (JSD) between two Bernoulli distributions
where each is parametrized by a single probability.
)DOC")
    .Input(0, "X", "array of probabilities for prediction")
    .Input(0, "T", "array of probabilities for target")
    .Output(0, "L", "array of JSD losses");
OPERATOR_SCHEMA(BernoulliJSDGradient).NumInputs(3).NumOutputs(1);

class GetBernoulliJSDGradient : public GradientMakerBase {
  using GradientMakerBase::GradientMakerBase;
  vector<OperatorDef> GetGradientDefs() override {
    return SingleGradientDef(
        "BernoulliJSDGradient",
        "",
        vector<string>{GO(0), I(0), I(1)},
        vector<string>{GI(0)});
  }
};
REGISTER_GRADIENT(BernoulliJSD, GetBernoulliJSDGradient);

} // namespace caffe2
