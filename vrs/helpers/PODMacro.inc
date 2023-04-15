/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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

/// System to repeat an existing preprocessor macro for each POD type supported by DataLayout.
/// Having this shared definition helps guarantee that we can repeat the behaviors everywhere
/// with the same list of POD type defined here.

// Note: do NOT define size_t, because its size is not guaranteed!
// Plus, size_t collides with some type below on some platforms... :-(

// These macros mean that a lot of code is generated for each type using templates.
// *** Do not extend this list lightly ***

POD_MACRO(Bool)
POD_MACRO(char)
POD_MACRO(double)
POD_MACRO(float)
POD_MACRO(int64_t)
POD_MACRO(uint64_t)
POD_MACRO(int32_t)
POD_MACRO(uint32_t)
POD_MACRO(int16_t)
POD_MACRO(uint16_t)
POD_MACRO(int8_t)
POD_MACRO(uint8_t)

POD_MACRO(Point2Dd)
POD_MACRO(Point2Df)
POD_MACRO(Point2Di)
POD_MACRO(Point3Dd)
POD_MACRO(Point3Df)
POD_MACRO(Point3Di)
POD_MACRO(Point4Dd)
POD_MACRO(Point4Df)
POD_MACRO(Point4Di)

POD_MACRO(Matrix2Dd)
POD_MACRO(Matrix2Df)
POD_MACRO(Matrix2Di)
POD_MACRO(Matrix3Dd)
POD_MACRO(Matrix3Df)
POD_MACRO(Matrix3Di)
POD_MACRO(Matrix4Dd)
POD_MACRO(Matrix4Df)
POD_MACRO(Matrix4Di)

#undef POD_MACRO
