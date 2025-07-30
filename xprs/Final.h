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

#pragma once

#include <stdexcept>
#include <string>

namespace xprs {

// Sort of replica of final feature in Java.
// Unlike const in c++ that must be initialsed at the place where it's declared
// or in case of class memeber in the member initializer list
// the final allows declaration in one place and later initialization in other place.
// For example:
// you can declare class member like: Final<int> m_final_some_var;
// Then, later in some method you can initialise it with required value:
//
// m_final_some_var.init(12345, __FILE__, __FUNCTION__, __LINE__);
//
// But only once. If init is called more than once it throws exception.
// init() method takes as parameters the follow macros: __FILE__, __FUNCTION__, __LINE__
// and adds them to the exception message so caller can see where problematic call is made.
// __FILE__, __FUNCTION__, __LINE__ are passed by the caller of the method so it logs
// the file, function and line of the problematic caller, not itself.
// Example of the output when init() is called more than once:
//
// terminate called after throwing an instance of 'std::runtime_error'
// what():  Object is already initialized, attempt to assign new value to the final object at:
// nv_encoder.cpp allocateOutputBuffer():227

template <typename T>
class Final {
 private:
  T m_value;
  bool _isInitialized = false;

 public:
  constexpr Final() : m_value() {}
  explicit constexpr Final(const T& value) : m_value(value), _isInitialized(true) {}

  void init(
      const T& value,
      const char* called_from_file,
      const char* called_from_function,
      const int called_from_line) {
    if (_isInitialized) {
      throw std::runtime_error(
          std::string(
              "Object is already initialized, attempt to assign new value to the final object\
 at: ") + called_from_file +
          " " + called_from_function + "():" + std::to_string(called_from_line));
    }
    m_value = value;
    _isInitialized = true;
  }
  [[nodiscard]] constexpr auto get() const -> const T& {
    return m_value;
  }
};

} // namespace xprs
