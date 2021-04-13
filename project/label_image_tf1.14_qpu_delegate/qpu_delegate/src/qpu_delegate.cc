/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "simple_delegate.h"
#include "qpu_delegate.h"
#include "tensorflow/lite/builtin_ops.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include <vector>
#include <utility>

namespace tflite
{
    namespace qpu_test
    {

        // QPU delegate kernel.
        class QPUDelegateKernel : public SimpleDelegateKernelInterface
        {
        public:
            explicit QPUDelegateKernel(const QPUDelegateOptions &options)
                : options_(options) {}

            TfLiteStatus Init(TfLiteContext *context,
                              const TfLiteDelegateParams *params) override
            {
                // Save index to all nodes which are part of this delegate.
                inputs_.resize(params->nodes_to_replace->size);
                outputs_.resize(params->nodes_to_replace->size);
                builtin_code_.resize(params->nodes_to_replace->size);
                for (int i = 0; i < params->nodes_to_replace->size; ++i)
                {
                    const int node_index = params->nodes_to_replace->data[i];
                    // Get this node information.
                    TfLiteNode *delegated_node = nullptr;
                    TfLiteRegistration *delegated_node_registration = nullptr;
                    TF_LITE_ENSURE_EQ(
                        context,
                        context->GetNodeAndRegistration(context, node_index, &delegated_node,
                                                        &delegated_node_registration),
                        kTfLiteOk);
                    inputs_[i].push_back(delegated_node->inputs->data[0]);
                    inputs_[i].push_back(delegated_node->inputs->data[1]);
                    outputs_[i].push_back(delegated_node->outputs->data[0]);
                    builtin_code_[i] = delegated_node_registration->builtin_code;
                }
                return kTfLiteOk;
            }

            TfLiteStatus Prepare(TfLiteContext *context, TfLiteNode *node) override
            {
                return kTfLiteOk;
            }

            TfLiteStatus Eval(TfLiteContext *context, TfLiteNode *node) override
            {
                // Evaluate the delegated graph.
                // Here we loop over all the delegated nodes.
                // We know that all the nodes are either ADD or SUB operations and the
                // number of nodes equals ''inputs_.size()'' and inputs[i] is a list of
                // tensor indices for inputs to node ''i'', while outputs_[i] is the list of
                // outputs for node
                // ''i''. Note, that it is intentional we have simple implementation as this
                // is for demonstration.

                for (int i = 0; i < inputs_.size(); ++i)
                {
                    // Get the node input tensors.
                    // Add/Sub operation accepts 2 inputs.
                    auto &input_tensor_1 = context->tensors[inputs_[i][0]];
                    auto &input_tensor_2 = context->tensors[inputs_[i][1]];
                    auto &output_tensor = context->tensors[outputs_[i][1]];
                    TF_LITE_ENSURE_EQ(
                        context,
                        ComputeResult(context, builtin_code_[i], &input_tensor_1,
                                      &input_tensor_2, &output_tensor),
                        kTfLiteOk);
                }
                return kTfLiteOk;
            }

        private:
            // Computes the result of addition of 'input_tensor_1' and 'input_tensor_2'
            // and store the result in 'output_tensor'.
            TfLiteStatus ComputeResult(TfLiteContext *context, int builtin_code,
                                       const TfLiteTensor *input_tensor_1,
                                       const TfLiteTensor *input_tensor_2,
                                       TfLiteTensor *output_tensor)
            {
                if (NumElements(input_tensor_1) != NumElements(input_tensor_2) ||
                    NumElements(input_tensor_1) != NumElements(output_tensor))
                {
                    return kTfLiteDelegateError;
                }
                // This code assumes no activation, and no broadcasting needed (both inputs
                // have the same size).
                auto *input_1 = GetTensorData<float>(input_tensor_1);
                auto *input_2 = GetTensorData<float>(input_tensor_2);
                auto *output = GetTensorData<float>(output_tensor);
                for (int i = 0; i < NumElements(input_tensor_1); ++i)
                {
                    if (builtin_code == kTfLiteBuiltinAdd)
                        output[i] = input_1[i] + input_2[i];
                    else
                        output[i] = input_1[i] - input_2[i];
                }
                return kTfLiteOk;
            }

            // Holds the indices of the input/output tensors.
            // inputs_[i] is list of all input tensors to node at index 'i'.
            // outputs_[i] is list of all output tensors to node at index 'i'.
            std::vector<std::vector<int>> inputs_, outputs_;
            // Holds the builtin code of the ops.
            // builtin_code_[i] is the type of node at index 'i'
            std::vector<int> builtin_code_;

        private:
            const QPUDelegateOptions options_;
        };

        // QPUDelegate implements the interface of SimpleDelegateInterface.
        // This holds the Delegate capabilities.
        class QPUDelegate : public SimpleDelegateInterface
        {
        public:
            explicit QPUDelegate(const QPUDelegateOptions &options)
                : options_(options) {}
            bool IsNodeSupportedByDelegate(const TfLiteRegistration *registration,
                                           const TfLiteNode *node,
                                           TfLiteContext *context) const override
            {
                return options_.allowed_builtin_code == registration->builtin_code;
            }

            TfLiteStatus Initialize(TfLiteContext *context) override { return kTfLiteOk; }

            const char *Name() const override
            {
                static constexpr char kName[] = "QPUDelegate";
                return kName;
            }

            std::unique_ptr<SimpleDelegateKernelInterface> CreateDelegateKernelInterface()
                override
            {
                return std::make_unique<QPUDelegateKernel>(options_);
            }

            SimpleDelegateInterface::Options DelegateOptions() const override
            {
                // Use default options.
                return SimpleDelegateInterface::Options();
            }

        private:
            const QPUDelegateOptions options_;
        };

    } // namespace qpu_test
} // namespace tflite

QPUDelegateOptions TfLiteQPUDelegateOptionsDefault()
{
    QPUDelegateOptions options = {0};

    // Operation(s) allowed to delegate
    options.allowed_builtin_code = kTfLiteBuiltinAdd;
    return options;
}

// Creates a new delegate instance that need to be destroyed with
// `TfLiteQPUDelegateDelete` when delegate is no longer used by TFLite.
// When `options` is set to `nullptr`, the above default values are used:
TfLiteDelegate *TfLiteQPUDelegateCreate(const QPUDelegateOptions *options)
{
    std::unique_ptr<tflite::qpu_test::QPUDelegate> qpu(
        new tflite::qpu_test::QPUDelegate(
            options ? *options : TfLiteQPUDelegateOptionsDefault()));
    return tflite::TfLiteDelegateFactory::CreateSimpleDelegate(std::move(qpu));
}

// Destroys a delegate created with `TfLiteQPUDelegateCreate` call.
void TfLiteQPUDelegateDelete(TfLiteDelegate *delegate)
{
    tflite::TfLiteDelegateFactory::DeleteSimpleDelegate(delegate);
}