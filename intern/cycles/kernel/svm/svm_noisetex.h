/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* The following offset functions generate random offsets to be added to texture
 * coordinates to act as a seed since the noise functions don't have seed values.
 * A seed value is needed for generating distortion textures and color outputs.
 * The offset's components are in the range [100, 200], not too high to cause
 * bad precision and not to small to be noticeable. We use float seed because
 * OSL only support float hashes.
 */

ccl_device_inline float random_float_offset(float seed)
{
  return 100.0f + hash_float_to_float(seed) * 100.0f;
}

ccl_device_inline float2 random_float2_offset(float seed)
{
  return make_float2(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f);
}

ccl_device_inline float3 random_float3_offset(float seed)
{
  return make_float3(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 2.0f)) * 100.0f);
}

ccl_device_inline float4 random_float4_offset(float seed)
{
  return make_float4(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 2.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 3.0f)) * 100.0f);
}

ccl_device void tex_noise_1d(
    float p, float detail, float distortion, bool color_is_needed, float *value, float3 *color)
{
  if (distortion != 0.0f) {
    p += noise_1d(p + random_float_offset(0.0f)) * distortion;
  }

  *value = noise_turbulence_1d(p, detail);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_turbulence_1d(p + random_float_offset(1.0f), detail),
                         noise_turbulence_1d(p + random_float_offset(2.0f), detail));
  }
}

ccl_device void tex_noise_2d(
    float2 p, float detail, float distortion, bool color_is_needed, float *value, float3 *color)
{
  if (distortion != 0.0f) {
    p += make_float2(noise_2d(p + random_float2_offset(0.0f)) * distortion,
                     noise_2d(p + random_float2_offset(1.0f)) * distortion);
  }

  *value = noise_turbulence_2d(p, detail);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_turbulence_2d(p + random_float2_offset(2.0f), detail),
                         noise_turbulence_2d(p + random_float2_offset(3.0f), detail));
  }
}

ccl_device void tex_noise_3d(
    float3 p, float detail, float distortion, bool color_is_needed, float *value, float3 *color)
{
  if (distortion != 0.0f) {
    p += make_float3(noise_3d(p + random_float3_offset(0.0f)) * distortion,
                     noise_3d(p + random_float3_offset(1.0f)) * distortion,
                     noise_3d(p + random_float3_offset(2.0f)) * distortion);
  }

  *value = noise_turbulence_3d(p, detail);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_turbulence_3d(p + random_float3_offset(3.0f), detail),
                         noise_turbulence_3d(p + random_float3_offset(4.0f), detail));
  }
}

ccl_device void tex_noise_4d(
    float4 p, float detail, float distortion, bool color_is_needed, float *value, float3 *color)
{
  if (distortion != 0.0f) {
    p += make_float4(noise_4d(p + random_float4_offset(0.0f)) * distortion,
                     noise_4d(p + random_float4_offset(1.0f)) * distortion,
                     noise_4d(p + random_float4_offset(2.0f)) * distortion,
                     noise_4d(p + random_float4_offset(3.0f)) * distortion);
  }

  *value = noise_turbulence_4d(p, detail);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_turbulence_4d(p + random_float4_offset(4.0f), detail),
                         noise_turbulence_4d(p + random_float4_offset(5.0f), detail));
  }
}

ccl_device void svm_node_tex_noise(KernelGlobals *kg,
                                   ShaderData *sd,
                                   float *stack,
                                   uint dimensions,
                                   uint offsets1,
                                   uint offsets2,
                                   int *offset)
{
  uint vector_offset, w_offset, scale_offset, detail_offset, distortion_offset, value_offset,
      color_offset;

  decode_node_uchar4(offsets1, &vector_offset, &w_offset, &scale_offset, &detail_offset);
  decode_node_uchar4(offsets2, &distortion_offset, &value_offset, &color_offset, NULL);

  uint4 node1 = read_node(kg, offset);

  float3 vector = stack_load_float3(stack, vector_offset);
  float w = stack_load_float_default(stack, w_offset, node1.x);
  float scale = stack_load_float_default(stack, scale_offset, node1.y);
  float detail = stack_load_float_default(stack, detail_offset, node1.z);
  float distortion = stack_load_float_default(stack, distortion_offset, node1.w);

  vector *= scale;
  w *= scale;

  float value;
  float3 color;

  switch (dimensions) {
    case 1:
      tex_noise_1d(w, detail, distortion, stack_valid(color_offset), &value, &color);
      break;
    case 2:
      tex_noise_2d(make_float2(vector.x, vector.y),
                   detail,
                   distortion,
                   stack_valid(color_offset),
                   &value,
                   &color);
      break;
    case 3:
      tex_noise_3d(vector, detail, distortion, stack_valid(color_offset), &value, &color);
      break;
    case 4:
      tex_noise_4d(make_float4(vector.x, vector.y, vector.z, w),
                   detail,
                   distortion,
                   stack_valid(color_offset),
                   &value,
                   &color);
      break;
    default:
      kernel_assert(0);
  }

  if (stack_valid(value_offset)) {
    stack_store_float(stack, value_offset, value);
  }
  if (stack_valid(color_offset)) {
    stack_store_float3(stack, color_offset, color);
  }
}

CCL_NAMESPACE_END
