// Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/operation/tile.h"

#include <vector>
#include "core/hal/types.h"
#include "utility/debug.h"
#include "utility/logging.h"
#include "utility/modeling.h"
#include "utility/utility.h"

namespace nnadapter {
namespace operation {

int PrepareTile(hal::Operation* operation) {
  TILE_OPERATION_EXTRACT_INPUTS_OUTPUTS

  // Infer the shape and type of output operands
  auto input_type = input_operand->type;
  auto& output_type = output_operand->type;
  auto& repeats_type = repeats_operand->type;
  CopyOperandTypeExceptQuantParams(&output_type, input_type);

  uint32_t repeats_count;
  int32_t* repeats_data;
  std::vector<int32_t> repeats;
  if (repeats_type.lifetime == NNADAPTER_TEMPORARY_SHAPE) {
    auto repeats_operand_dimension =
        *reinterpret_cast<NNAdapterOperandDimensionType*>(
            repeats_operand->buffer);
    repeats_count = repeats_operand_dimension.count;
    repeats_data = repeats_operand_dimension.data;
    repeats = std::vector<int32_t>(repeats_data, repeats_count + repeats_data);
  } else if (IsConstantOperand(repeats_operand)) {
    repeats_count = repeats_operand->length / sizeof(int32_t);
    repeats_data = reinterpret_cast<int32_t*>(repeats_operand->buffer);
    repeats = std::vector<int32_t>(repeats_data, repeats_data + repeats_count);
  } else {
    NNADAPTER_LOG(FATAL) << "Unsupported repeats lifetime: "
                         << static_cast<int32_t>(repeats_type.lifetime);
    return NNADAPTER_INVALID_PARAMETER;
  }

  auto infer_output_shape = [&](int32_t* input_dimensions_data,
                                uint32_t input_dimensions_count,
                                int32_t* output_dimensions_data) {
    // broadcast for vec_in_dims.size() equal to repeats.size()
    std::vector<int> input_dims_vec;
    for (uint32_t i = 0; i < input_dimensions_count; i++) {
      input_dims_vec.push_back(input_dimensions_data[i]);
    }

    if (repeats_count < input_dimensions_count) {
      int diff = input_dimensions_count - repeats_count;
      repeats.insert(repeats.begin(), diff, 1);
      output_type.dimensions.count = input_dimensions_count;
    } else {
      int diff = repeats_count - input_dimensions_count;
      input_dims_vec.insert(input_dims_vec.begin(), diff, 1);
      output_type.dimensions.count = repeats_count;
    }

    for (uint32_t i = 0; i < output_type.dimensions.count; i++) {
      output_dimensions_data[i] = input_dims_vec[i] * repeats_data[i];
    }
  };

  infer_output_shape(input_type.dimensions.data,
                     input_type.dimensions.count,
                     output_type.dimensions.data);
  for (uint32_t i = 0; i < input_type.dimensions.dynamic_count; i++) {
    infer_output_shape(input_type.dimensions.dynamic_data[i],
                       input_type.dimensions.count,
                       output_type.dimensions.dynamic_data[i]);
  }

  NNADAPTER_VLOG(5) << "output: " << OperandToString(output_operand);
  return NNADAPTER_NO_ERROR;
}

}  // namespace operation
}  // namespace nnadapter
