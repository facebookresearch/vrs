#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest

from pyvrs.utils import stringify_metadata_keys


class TestMetadataStringKeys(unittest.TestCase):
    def test_basic_stringify_types(self):
        self.assertEqual(
            stringify_metadata_keys(
                {("varnamea", "typenamea"): "aval", ("varnameb", "typenameb"): "bval"}
            ),
            {"varnamea": "aval", "varnameb": "bval"},
        )

    def test_stringify_types_with_collision(self):
        self.assertEqual(
            stringify_metadata_keys(
                {
                    ("varname", "typenamea"): "aval",
                    ("varname", "typenameb"): "bval",
                    ("varnameother", "typenameb"): "cval",
                }
            ),
            {
                "varname<typenamea>": "aval",
                "varname<typenameb>": "bval",
                "varnameother": "cval",
            },
        )

    def test_stringify_types_with_ambiguity(self):
        self.assertEqual(
            stringify_metadata_keys(
                {
                    ("acceleration<int>", "int"): 2,
                    ("acceleration", "int"): 3,
                    ("acceleration", "float"): 4,
                    ("other", "int"): 5,
                }
            ),
            {
                "acceleration<int>": 2,
                "acceleration<<int>>": 3,
                "acceleration<float>": 4,
                "other": 5,
            },
        )


if __name__ == "__main__":
    unittest.main()
