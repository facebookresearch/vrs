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

import asyncio
import functools
import unittest
from pathlib import Path
from typing import Any

import numpy as np
import pkg_resources
from pyvrs import AsyncVRSReader, SyncVRSReader


def test_recording_path() -> Path:
    return Path(pkg_resources.resource_filename(__name__, "test_data/synthetic.vrs"))


def async_test(func):
    if not asyncio.iscoroutinefunction(func):
        # If you are investigating why you have an `async def` and this fires,
        # make sure that you don't use `yield` in the test function, as `async def` + `yield`
        # is an async generator!
        raise TypeError("Only 'async def' is supported, please fix your call site")

    @functools.wraps(func)
    def wrapper(*args: Any, **kws: Any) -> None:
        loop = asyncio.get_event_loop()
        loop.run_until_complete(func(*args, **kws))

    return wrapper


class TestAsyncRead(unittest.TestCase):
    @async_test
    async def test_async_read(self):
        reader = SyncVRSReader(
            test_recording_path(),
            auto_read_configuration_records=True,
        )
        async_reader = AsyncVRSReader(
            test_recording_path(),
            auto_read_configuration_records=True,
        )
        filtered_reader = reader.filtered_by_fields(
            stream_ids="100-1",
            record_types="data",
        )
        async_filtered_reader = async_reader.filtered_by_fields(
            stream_ids="100-1",
            record_types="data",
        )
        self.assertEqual(len(filtered_reader), 500)
        self.assertEqual(len(async_filtered_reader), 500)

        records = [record for record in filtered_reader[:500]]
        async_records = [record async for record in async_filtered_reader]
        async_records_widht_slice = [
            record async for record in await async_filtered_reader[:500]
        ]

        self.assertEqual(len(records), len(async_records))
        self.assertEqual(len(records), len(async_records_widht_slice))
        for i in range(len(records)):
            b1 = records[i].image_blocks[0]
            b2 = async_records[i].image_blocks[0]
            b3 = async_records_widht_slice[i].image_blocks[0]
            self.assertTrue(np.array_equal(b1, b2))
            self.assertTrue(np.array_equal(b1, b3))


if __name__ == "__main__":
    unittest.main()
