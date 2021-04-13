/* Copyright 2017 The TensorFlow Authors.
    Modifications copyright 2021 Wanghao Xu.

   Portions copyright 2019 clouwdwise consulting. 
   All Rights Reserved.

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

#include "label_image.h" // file path changed by Xu

#include <fcntl.h>     // NOLINT(build/include_order)
#include <getopt.h>    // NOLINT(build/include_order)
#include <sys/time.h>  // NOLINT(build/include_order)
#include <sys/types.h> // NOLINT(build/include_order)
#include <sys/uio.h>   // NOLINT(build/include_order)
#include <unistd.h>    // NOLINT(build/include_order)

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "dummy_delegate.h" // added
#include "absl/memory/memory.h"
#include "bitmap_helpers.h" // file path changed by Xu
#include "get_top_n.h"      // file path changed by Xu
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "tensorflow/lite/profiling/profiler.h"
#include "tensorflow/lite/string_util.h"

// added
#include "tensorflow/lite/tools/command_line_flags.h"
#include "tensorflow/lite/tools/delegates/delegate_provider.h"
#include "tensorflow/lite/tools/evaluation/utils.h"


#define LOG(x) std::cerr

#define EXPECTED_NUM_INPUTS 1
#define EXPECTED_NUM_OUTPUTS 4
#define IMAGE_WIDTH 300
#define IMAGE_HEIGHT 300
#define IMAGE_CHANNELS 3
#define THRESHOLD 0.5f

namespace tflite
{
  namespace label_image
  {

    std::vector<std::array<unsigned char, 4>> ColorPalette{
        {255, 0, 0, 255},   // red
        {0, 255, 0, 255},   // lime
        {0, 0, 255, 255},   // blue
        {255, 128, 0, 255}, // orange
        {255, 0, 255, 255}, // magenta
        {0, 255, 255, 255}, // cyan
        {255, 255, 0, 255}, // yellow
        {255, 0, 127, 255}, // pink
        {128, 255, 0, 255}, // light green
        {0, 191, 255, 255}  // deep sky blue
    };

    template <typename T>
    T *TensorData(TfLiteTensor *tensor);

    template <>
    float *TensorData(TfLiteTensor *tensor)
    {
      switch (tensor->type)
      {
      case kTfLiteFloat32:
        return tensor->data.f;
      default:
        LOG(FATAL) << "Unknown or mismatched tensor type\n";
      }
      return nullptr;
    }

    template <>
    uint8_t *TensorData(TfLiteTensor *tensor)
    {
      switch (tensor->type)
      {
      case kTfLiteUInt8:
        return tensor->data.uint8;
      default:
        LOG(FATAL) << "Unknown or mistmatched tensor type\n";
      }
      return nullptr;
    }

    double get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

    using TfLiteDelegatePtr = tflite::Interpreter::TfLiteDelegatePtr;
    using TfLiteDelegatePtrMap = std::map<std::string, TfLiteDelegatePtr>;

    class DelegateProviders
    {
    public:
      DelegateProviders()
          : delegates_list_(tflite::tools::GetRegisteredDelegateProviders())
      {
        for (const auto &delegate : delegates_list_)
        {
          params_.Merge(delegate->DefaultParams());
        }
      }

      // Initialize delegate-related parameters from parsing command line arguments,
      // and remove the matching arguments from (*argc, argv). Returns true if all
      // recognized arg values are parsed correctly.
      bool InitFromCmdlineArgs(int *argc, const char **argv)
      {
        std::vector<tflite::Flag> flags;
        for (const auto &delegate : delegates_list_)
        {
          auto delegate_flags = delegate->CreateFlags(&params_);
          flags.insert(flags.end(), delegate_flags.begin(), delegate_flags.end());
        }

        const bool parse_result = Flags::Parse(argc, argv, flags);
        if (!parse_result)
        {
          std::string usage = Flags::Usage(argv[0], flags);
          LOG(ERROR) << usage;
        }
        return parse_result;
      }

      // Create a list of TfLite delegates based on what have been initialized (i.e.
      // 'params_').
      TfLiteDelegatePtrMap CreateAllDelegates() const
      {
        TfLiteDelegatePtrMap delegates_map;
        for (const auto &delegate : delegates_list_)
        {
          auto ptr = delegate->CreateTfLiteDelegate(params_);
          // It's possible that a delegate of certain type won't be created as
          // user-specified benchmark params tells not to.
          if (ptr == nullptr)
            continue;
          LOG(INFO) << delegate->GetName() << " delegate created.";
          delegates_map.emplace(delegate->GetName(), std::move(ptr));
        }
        return delegates_map;
      }

    private:
      // Contain delegate-related parameters that are initialized from command-line
      // flags.
      tflite::tools::ToolParams params_;

      const tflite::tools::DelegateProviderList &delegates_list_;
    };

    TfLiteDelegatePtrMap GetDelegates(Settings *s,
                                      const DelegateProviders &delegate_providers)
    {
      // TODO(b/169681115): deprecate delegate creation path based on "Settings" by
      // mapping settings to DelegateProvider's parameters.
      TfLiteDelegatePtrMap delegates;

      // our delegate
      if (s->dummy_delegate)
      {
        auto delegate = TfLiteDummyDelegateCreateUnique(nullptr);
        if (!delegate)
        {
          LOG(INFO) << "Dummy delegate init failed.";
        }
        else
        {
          LOG(INFO) << "Use dummy delegate acceleration.";
          delegates.emplace("DUMMY", std::move(delegate));
        }
      }

      // Independent of above delegate creation options that are specific to this
      // binary, we use delegate providers to create TFLite delegates. Delegate
      // providers have been used in TFLite benchmark/evaluation tools and testing
      // so that we have a single and more comprehensive set of command line
      // arguments for delegate creation.
      TfLiteDelegatePtrMap delegates_from_providers =
          delegate_providers.CreateAllDelegates();
      for (auto &name_and_delegate : delegates_from_providers)
      {
        delegates.emplace("Delegate_Provider_" + name_and_delegate.first,
                          std::move(name_and_delegate.second));
      }

      return delegates;
    }

    // Takes a file name, and loads a list of labels from it, one per line, and
    // returns a vector of the strings. It pads with empty strings so the length
    // of the result is a multiple of 16, because our model expects that.
    TfLiteStatus ReadLabelsFile(const string &file_name,
                                std::vector<string> *result,
                                size_t *found_label_count)
    {
      std::ifstream file(file_name);
      if (!file)
      {
        LOG(FATAL) << "Labels file: " << file_name << " not found\n";
        return kTfLiteError;
      }
      result->clear();
      string line;
      while (std::getline(file, line))
      {
        result->push_back(line);
      }
      *found_label_count = result->size();
      const int padding = 16;
      while (result->size() % padding)
      {
        result->emplace_back();
      }
      return kTfLiteOk;
    }

    void PrintProfilingInfo(const profiling::ProfileEvent *e, uint32_t op_index,
                            TfLiteRegistration registration)
    {
      // output something like
      // time (ms) , Node xxx, OpCode xxx, symblic name
      //      5.352, Node   5, OpCode   4, DEPTHWISE_CONV_2D
      LOG(INFO) << std::fixed << std::setw(10) << std::setprecision(3)
                << (e->end_timestamp_us - e->begin_timestamp_us) / 1000.0
                << ", Node " << std::setw(3) << std::setprecision(3) << op_index
                << ", OpCode " << std::setw(3) << std::setprecision(3)
                << registration.builtin_code << ", "
                << EnumNameBuiltinOperator(
                       static_cast<BuiltinOperator>(registration.builtin_code))
                << "\n";
    }

    void RunInference(Settings *s,
                      const DelegateProviders &delegate_providers)
    {

      // Check we have a model file in the first place...
      if (!s->model_name.c_str())
      {
        LOG(ERROR) << "No model file specified\n";
        exit(-1);
      }
      // ...if so, load the model from file
      std::unique_ptr<tflite::FlatBufferModel> model;
      std::unique_ptr<tflite::Interpreter> interpreter;
      model = tflite::FlatBufferModel::BuildFromFile(s->model_name.c_str());
      if (!model)
      {
        LOG(FATAL) << "Failed to mmap model: " << s->model_name << "\n";
        exit(-1);
      }
      s->model = model.get();
      LOG(INFO) << "Loaded model: " << s->model_name << "\n";
      model->error_reporter();
      LOG(INFO) << "Resolved reporter\n";

      tflite::ops::builtin::BuiltinOpResolver resolver;

      tflite::InterpreterBuilder(*model, resolver)(&interpreter);
      if (!interpreter)
      {
        LOG(FATAL) << "Failed to construct interpreter\n";
        exit(-1);
      }

      // Attempt to set the accleration flags but depends on your platform i.e. armv6 doesn't support these
      interpreter->SetAllowFp16PrecisionForFp32(s->allow_fp16);

      // Provide detailed info on the model structure and datatypes : useful for identifying the input and output points
      if (s->verbose)
      {
        LOG(INFO) << "tensors size : " << interpreter->tensors_size() << "\n";
        LOG(INFO) << "nodes size   : " << interpreter->nodes_size() << "\n";
        LOG(INFO) << "inputs       : " << interpreter->inputs().size() << "\n";
        LOG(INFO) << "input(0) name: " << interpreter->GetInputName(0) << "\n";

        int t_size = interpreter->tensors_size();
        for (int i = 0; i < t_size; i++)
        {
          if (interpreter->tensor(i)->name)
            LOG(INFO) << i << ": " << interpreter->tensor(i)->name << ", "
                      << interpreter->tensor(i)->bytes << ", "
                      << interpreter->tensor(i)->type << ", "
                      << interpreter->tensor(i)->params.scale << ", "
                      << interpreter->tensor(i)->params.zero_point << "\n";
        }
      }

      if (s->number_of_threads != -1)
      {
        interpreter->SetNumThreads(s->number_of_threads);
      }

      int image_width = IMAGE_WIDTH;
      int image_height = IMAGE_HEIGHT;
      int image_channels = IMAGE_CHANNELS;
      // std::vector<uint8_t> in = read_bmp(s->input_bmp_name, &image_width, &image_height, &image_channels, s);
      std::ifstream file(s->input_bmp_name, std::ios::in | std::ios::binary);
      if (!file)
      {
        LOG(FATAL) << "input file " << s->input_bmp_name << " not found\n";
        exit(-1);
      }
      // Read in the BMP file and decode it to the correct, im-memory structure so that the model is fed with a
      // common input regardless (more or less) of the BMP format
      BMP bmp(s->input_bmp_name.c_str());
      std::vector<uint8_t> in = parse_bmp(&bmp, &image_width, &image_height, &image_channels, s);

      // Instantiare the input and output tensors
      int input = interpreter->inputs()[0];
      if (s->verbose)
        LOG(INFO) << "input: " << input << "\n";

      const std::vector<int> inputs = interpreter->inputs();
      const std::vector<int> outputs = interpreter->outputs();
      if (s->verbose)
      {
        LOG(INFO) << "number of inputs : " << inputs.size() << "\n";
        LOG(INFO) << "number of outputs: " << outputs.size() << "\n";
      }
      // Check the input and output geometry is what we are expecting
      if (inputs.size() != EXPECTED_NUM_INPUTS)
      {
        LOG(FATAL) << "Expecting " << EXPECTED_NUM_INPUTS << " inputs\n";
        exit(-1);
      }
      if (outputs.size() != EXPECTED_NUM_OUTPUTS)
      {
        LOG(FATAL) << "Expecting " << EXPECTED_NUM_OUTPUTS << " outputs\n";
        exit(-1);
      }

      auto delegates_ = GetDelegates(s, delegate_providers);
      for (const auto &delegate : delegates_)
      {
        if (interpreter->ModifyGraphWithDelegate(delegate.second.get()) !=
            kTfLiteOk)
        {
          LOG(ERROR) << "Failed to apply " << delegate.first << " delegate.";
          exit(-1);
        }
        else
        {
          LOG(INFO) << "Applied " << delegate.first << " delegate.";
        }
      }

      if (interpreter->AllocateTensors() != kTfLiteOk)
      {
        LOG(FATAL) << "Failed to allocate tensors!\n";
      }

      if (s->verbose)
        PrintInterpreterState(interpreter.get());

      // Get the input dimension from the input tensor metadata
      // ...note we are assuming one input only here (rightly or wrongly)
      TfLiteIntArray *dims = interpreter->tensor(input)->dims;
      int wanted_height = dims->data[1];
      int wanted_width = dims->data[2];
      int wanted_channels = dims->data[3];
      if (s->verbose)
        LOG(INFO) << "wanted height: " << wanted_height << " | "
                  << "wanted width: " << wanted_width << " | "
                  << "wanted channels: " << wanted_channels << "\n";

      // Resize the input image array for the required input dimensions required by the model i.e. height, width, number of channels
      switch (interpreter->tensor(input)->type)
      {
      case kTfLiteFloat32:
        s->input_floating = true;
        resize<float>(interpreter->typed_tensor<float>(input), in.data(),
                      image_height, image_width, image_channels, wanted_height,
                      wanted_width, wanted_channels, s);
        break;
      case kTfLiteUInt8:
        resize<uint8_t>(interpreter->typed_tensor<uint8_t>(input), in.data(),
                        image_height, image_width, image_channels, wanted_height,
                        wanted_width, wanted_channels, s);
        break;
      default:
        LOG(FATAL) << "Tensor input type: "
                   << interpreter->tensor(input)->type << " not supported\n";
        exit(-1);
      }

      auto profiler =
          absl::make_unique<profiling::Profiler>(s->max_profiling_buffer_entries);
      interpreter->SetProfiler(profiler.get());

      // Start profiling if requested
      if (s->profiling)
        profiler->StartProfiling();
      if (s->loop_count > 1)
        for (int i = 0; i < s->number_of_warmup_runs; i++)
        {
          if (interpreter->Invoke() != kTfLiteOk)
          {
            LOG(FATAL) << "Failed to invoke tflite!\n";
          }
        }

      // Run the Invoke() aka inference 1 or more times
      struct timeval start_time, stop_time;
      gettimeofday(&start_time, nullptr);
      LOG(INFO) << "Invoke started...\n";
      for (int i = 0; i < s->loop_count; i++)
      {
        if (interpreter->Invoke() != kTfLiteOk)
        {
          LOG(FATAL) << "Failed to invoke tflite!\n";
        }
      }

      gettimeofday(&stop_time, nullptr);
      LOG(INFO) << "Invoke finished\n";
      LOG(INFO) << "(average time: "
                << (get_us(stop_time) - get_us(start_time)) / (s->loop_count * 1000)
                << " ms)\n";

      // Finish profiling...if we started it in the first-place
      if (s->profiling)
      {
        profiler->StopProfiling();
        auto profile_events = profiler->GetProfileEvents();
        for (int i = 0; i < profile_events.size(); i++)
        {
          auto op_index = profile_events[i]->event_metadata;
          const auto node_and_registration =
              interpreter->node_and_registration(op_index);
          const TfLiteRegistration registration = node_and_registration->second;
          PrintProfilingInfo(profile_events[i], op_index, registration);
        }
      }

      // Define our confidence threshold for pruning the results, storage vector for a result and the model outputs
      const float threshold = THRESHOLD;

      std::vector<std::tuple<float, int, int>> top_results;

      int output = interpreter->outputs()[0];

      std::vector<string> labels;
      size_t label_count;
      // Read in the object labels file (typically COCO)
      if (ReadLabelsFile(s->labels_file_name, &labels, &label_count) != kTfLiteOk)
      {
        LOG(FATAL) << "Failed to load labels file: " << s->labels_file_name << " \n";
        exit(-1);
      }

      // Get the ouuput tensors (derived from Yijin Liu's repo at https://github.com/YijinLiu/tf-cpu)
      TfLiteTensor *output_locations_ = interpreter->tensor(interpreter->outputs()[0]);
      TfLiteTensor *output_classes_ = interpreter->tensor(interpreter->outputs()[1]);
      TfLiteTensor *output_scores_ = interpreter->tensor(interpreter->outputs()[2]);
      TfLiteTensor *num_detections_ = interpreter->tensor(interpreter->outputs()[3]);
      if (s->verbose)
        LOG(INFO) << "graph output size: " << interpreter->outputs().size() << "\n";

      // Map the output tensors to common datatypes for parsing
      const float *detection_locations = TensorData<float>(output_locations_);
      const float *detection_classes = TensorData<float>(output_classes_);
      const float *detection_scores = TensorData<float>(output_scores_);
      const int num_detections = *TensorData<float>(num_detections_);

      if (s->verbose)
      {
        LOG(INFO) << "number of detections: " << num_detections << "\n";
        // Display all the detection details...
        for (int d = 0; d < num_detections; d++)
        {
          // Increment the class index given tflite conversion removes the initial background class
          const std::string cls = labels[detection_classes[d] + 1];
          // Extract the score and bounding rectangle...from observation, we seem to have to flip the y-axes
          const float score = detection_scores[d];
          const float ymax = (1 - detection_locations[(sizeof(float) * d) + 0]) * image_height;
          const float xmin = detection_locations[(sizeof(float) * d) + 1] * image_width;
          const float ymin = (1 - detection_locations[(sizeof(float) * d) + 2]) * image_height;
          const float xmax = detection_locations[(sizeof(float) * d) + 3] * image_width;
          LOG(INFO) << "------ detection: " << d << " ------\n";
          LOG(INFO) << " score = " << score << "\n";
          LOG(INFO) << " class = " << cls << "\n";
          LOG(INFO) << " ymin  = " << static_cast<unsigned int>(ymin) << "\n";
          LOG(INFO) << " xmin  = " << static_cast<unsigned int>(xmin) << "\n";
          LOG(INFO) << " ymax  = " << static_cast<unsigned int>(ymax) << "\n";
          LOG(INFO) << " xmax  = " << static_cast<unsigned int>(xmax) << "\n";
        }
      }

      // Get the top n results from the candidates
      // ...although inspecting the SSD_mobilenet_v1_10 model archiotecture, it appears to be limited to 10
      switch (interpreter->tensor(output)->type)
      {
      case kTfLiteFloat32:
        get_top_n<float>(detection_scores, detection_classes,
                         num_detections, s->number_of_results,
                         threshold, &top_results, true);
        break;
      case kTfLiteUInt8:
        get_top_n<uint8_t>(detection_scores, detection_classes,
                           num_detections, s->number_of_results,
                           threshold, &top_results, false);
        break;
      default:
        LOG(FATAL) << "Tensor output type: "
                   << interpreter->tensor(output)->type << " not supported\n";
        exit(-1);
      }

      // Display the dectection summaries for objects above the threshold
      LOG(INFO) << "------ detections > " << threshold << " ------\n";
      for (const auto &result : top_results)
      {
        const float confidence = std::get<0>(result);
        const int class_index = std::get<1>(result);
        const int detection_index = std::get<2>(result);

        // Increment the class index given tflite conversion removes the initial background class
        LOG(INFO) << "object [" << detection_index << "] = " << labels[class_index + 1] << " @ " << confidence << "\n";

        // Draw the bounding box for the object on the image - we use the same y axis flip as above
        if (s->output)
        {
          const float ymax = (1 - detection_locations[(sizeof(float) * detection_index) + 0]) * image_height;
          const float xmin = detection_locations[(sizeof(float) * detection_index) + 1] * image_width;
          const float ymin = (1 - detection_locations[(sizeof(float) * detection_index) + 2]) * image_height;
          const float xmax = detection_locations[(sizeof(float) * detection_index) + 3] * image_width;
          auto color = ColorPalette[detection_index];
          auto R = color[0], G = color[1], B = color[2], A = color[3];
          draw_bounding_box(&bmp, xmin, ymin, xmax - xmin, ymax - ymin, R, G, B, A);
        }
      }
      // Save the image out to a (new) BMP-format file
      if (s->output)
        write_bmp(&bmp, s);
    }

    void display_usage()
    {
      LOG(INFO)
          << "label_image\n"
          << "--allow_fp16, -f: [0|1], allow running fp32 models with fp16 or not\n"
          << "--count, -c: loop interpreter->Invoke() for certain times\n"
          << "--input_mean, -b: input mean\n"
          << "--input_std, -s: input standard deviation\n"
          << "--image, -i: image_name.bmp\n"
          << "--labels, -l: labels for the model\n"
          << "--tflite_model, -m: model_name.tflite\n"
          << "--output, -o: [0|1] save output to file\n"
          << "--profiling, -p: [0|1], profiling or not\n"
          << "--num_results, -r: number of results to show\n"
          << "--threads, -t: number of threads\n"
          << "--verbose, -v: [0|1] print more information\n"
          << "--warmup_runs, -w: number of warmup runs\n"
          << "--dummy_delegate, -d: [0|1] dummy delegate\n" //added
          << "\n";
    }

    int Main(int argc, char **argv)
    {
      DelegateProviders delegate_providers;
      bool parse_result = delegate_providers.InitFromCmdlineArgs(
          &argc, const_cast<const char **>(argv));
      if (!parse_result)
      {
        return EXIT_FAILURE;
      }

      Settings s;

      int c;
      while (1)
      {
        static struct option long_options[] = {
            {"count", required_argument, nullptr, 'c'},
            {"allow_fp16", required_argument, nullptr, 'f'},
            {"image", required_argument, nullptr, 'i'},
            {"labels", required_argument, nullptr, 'l'},
            {"tflite_model", required_argument, nullptr, 'm'},
            {"output", required_argument, nullptr, 'o'},
            {"profiling", required_argument, nullptr, 'p'},
            {"num_results", required_argument, nullptr, 'r'},
            {"threads", required_argument, nullptr, 't'},
            {"input_mean", required_argument, nullptr, 'b'},
            {"input_std", required_argument, nullptr, 's'},
            {"max_profiling_buffer_entries", required_argument, nullptr, 'e'},
            {"warmup_runs", required_argument, nullptr, 'w'},
            {"verbose", required_argument, nullptr, 'v'},
            {"dummy_delegate", required_argument, nullptr, 'd'}, //added
            {nullptr, 0, nullptr, 0}};

        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "b:c:d:e:f:i:l:m:o:p:r:s:t:v:w:", long_options,
                        &option_index);

        /* Detect the end of the options. */
        if (c == -1)
          break;

        switch (c)
        {
        case 'b':
          s.input_mean = strtod(optarg, nullptr);
          break;
        case 'c':
          s.loop_count =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'd': // added dummy delegate
          s.dummy_delegate = 
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'e':
          s.max_profiling_buffer_entries =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'f':
          s.allow_fp16 =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'i':
          s.input_bmp_name = optarg;
          break;
        case 'l':
          s.labels_file_name = optarg;
          break;
        case 'm':
          s.model_name = optarg;
          break;
        case 'o':
          s.output =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
        case 'p':
          s.profiling =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'r':
          s.number_of_results =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 's':
          s.input_std = strtod(optarg, nullptr);
          break;
        case 't':
          s.number_of_threads = strtol( // NOLINT(runtime/deprecated_fn)
              optarg, nullptr, 10);
          break;
        case 'v':
          s.verbose =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'w':
          s.number_of_warmup_runs =
              strtol(optarg, nullptr, 10); // NOLINT(runtime/deprecated_fn)
          break;
        case 'h':
        case '?':
          /* getopt_long already printed an error message. */
          display_usage();
          exit(-1);
        default:
          exit(-1);
        }
      }
      RunInference(&s, delegate_providers);
      return 0;
    }

  } // namespace label_image
} // namespace tflite

int main(int argc, char **argv)
{
  return tflite::label_image::Main(argc, argv);
}
