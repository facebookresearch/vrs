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
from pathlib import Path

import numpy as np
import pkg_resources
from parameterized import parameterized_class
from PIL import Image
from pyvrs import ImageConversion, RecordType, SyncVRSReader, TimestampNotFoundError
from pyvrs.record import VRSRecord
from pyvrs.slice import VRSReaderSlice


def test_recording_path() -> Path:
    return Path(pkg_resources.resource_filename(__name__, "test_data/synthetic.vrs"))


def rgbx_float_to_rgb8(block: np.ndarray, channel: int) -> np.ndarray:
    assert len(block.shape) == 3
    assert len(block[0][0]) == channel
    minV = []
    maxV = []
    for i in range(channel):
        minV.append(block[0][0][i])
        maxV.append(block[0][0][i])

    img_block = np.zeros(block.shape, dtype=np.uint8)
    for y in range(block.shape[0]):
        for x in range(block.shape[1]):
            for i in range(channel):
                minV[i] = min(minV[i], block[y][x][i])
                maxV[i] = max(maxV[i], block[y][x][i])
    factor = []
    for i in range(channel):
        if maxV[i] > minV[i]:
            factor.append(255.0 / (maxV[i] - minV[i]))
        else:
            factor.append(0)

    for y in range(block.shape[0]):
        for x in range(block.shape[1]):
            for i in range(channel):
                img_block[y][x][i] = int((block[y][x][i] - minV[i]) * factor[i])
    return img_block


class TestVRSTestDataExists(unittest.TestCase):
    def test_test_recording_path_is_file(self):
        self.assertTrue(test_recording_path().is_file())


@parameterized_class(
    ("multi_path"),
    [
        [False],
        [True],
    ],
)
class TestTestRecordingReadingWithAutoConfigTrue(unittest.TestCase):
    def setUp(self):
        self.reader = SyncVRSReader(
            test_recording_path(),
            auto_read_configuration_records=True,
            multi_path=self.multi_path,
        )

    def test_len_of_reader_is_number_of_records(self):
        self.assertEqual(len(self.reader), 9538)

    def test_len_of_available_record_types(self):
        self.assertEqual(len(self.reader.record_types), 3)

    def test_len_of_available_stream_ids(self):
        self.assertEqual(len(self.reader.stream_ids), 19)

    def test_n_enabled_same_as_len(self):
        self.assertEqual(self.reader.n_records, len(self.reader))

    def test_accessing_first_record_returns_VRSRecord(self):
        self.assertIsInstance(self.reader[0], VRSRecord)

    def test_accessing_last_record_by_negative_one(self):
        self.assertEqual(
            self.reader[-1].record_index, self.reader[len(self.reader) - 1].record_index
        )

    def test_accessing_second_to_last_record_by_negative_two(self):
        self.assertEqual(
            self.reader[-2].record_index, self.reader[len(self.reader) - 2].record_index
        )

    def test_iterating_all_records_in_timestamp_order(self):
        prev_timestamp = -9999999
        for record in self.reader:
            self.assertGreaterEqual(record.timestamp, prev_timestamp)
            prev_timestamp = record.timestamp

    def test_filter_by_record_type_only_iterates_that_type(self):
        filtered_reader = self.reader.filtered_by_fields(record_types="configuration")
        for record in filtered_reader:
            self.assertEqual(record.record_type, "configuration")

    def test_filter_by_stream_ids_only_iterates_that_id(self):
        filtered_reader = self.reader.filtered_by_fields(stream_ids="100-1")
        for record in filtered_reader:
            self.assertEqual(record.stream_id, "100-1")

    def test_filter_by_timestamp_min_only_iterates_bigger_timestamps(self):
        min_timestamp = 5.0
        filtered_reader = self.reader.filtered_by_fields(min_timestamp=min_timestamp)
        for record in filtered_reader:
            self.assertTrue(record.timestamp >= min_timestamp)

    def test_filter_by_timestamps(self):
        min_timestamp = 5.0
        max_timestamp = 8.0
        filtered_reader = self.reader.filtered_by_fields(
            min_timestamp=min_timestamp, max_timestamp=max_timestamp
        )

        for record in filtered_reader:
            self.assertTrue(record.timestamp >= min_timestamp)
            self.assertTrue(record.timestamp <= max_timestamp)

    def test_filter_by_stream_id_record_type_and_timestamp(self):
        min_timestamp = 5.0
        stream_id = "100-1"
        record_type = "configuration"
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=[stream_id],
            record_types=[record_type],
            min_timestamp=min_timestamp,
        )
        for record in filtered_reader:
            self.assertTrue(record.timestamp >= min_timestamp)
            self.assertEqual(record.stream_id, stream_id)
            self.assertEqual(record.record_type, record_type)

    def test_filter_by_stream_ids_by_recordable_type_id(self):
        filtered_reader = self.reader.filtered_by_fields(stream_ids="100*")
        expected_stream_ids = set()
        for i in range(1, 20):
            expected_stream_ids.add(f"100-{i}")
        self.assertEqual(filtered_reader.stream_ids, expected_stream_ids)

    def test_filter_by_stream_ids_still_in_timestamp_order(self):
        filtered_reader = self.reader.filtered_by_fields(stream_ids="100-1")
        prev_timestamp = -9999999
        for record in filtered_reader:
            self.assertGreaterEqual(record.timestamp, prev_timestamp)
            prev_timestamp = record.timestamp

    def test_filtering_impacts_len(self):
        l1 = len(self.reader)
        filtered_reader = self.reader.filtered_by_fields(record_types="configuration")
        l2 = len(filtered_reader)
        self.assertLess(l2, l1)

    def test_filtered_len_equal_to_iteration_count(self):
        filtered_reader = self.reader.filtered_by_fields(record_types="configuration")
        _i = 0
        for _i, _ in enumerate(filtered_reader):
            pass
        self.assertEqual(len(filtered_reader), _i + 1)

    # TODO: Decide how to handle bad inputs. Currently ignoring it.
    # def test_filter_by_record_type_raises_on_bad_input(self):
    #     with self.assertRaises(ValueError):
    #         self.reader.filtered_by_fields(record_types="notavalidtype")

    def test_reset_filter_restores_len(self):
        full_len = 9538
        config_len = 19
        self.assertEqual(len(self.reader), full_len)
        filtered_reader = self.reader.filtered_by_fields(record_types="configuration")
        self.assertEqual(len(filtered_reader), config_len)

    def test_slicing_returns_VRSReaderSlice(self):
        self.assertIsInstance(self.reader[:2], VRSReaderSlice)

    def test_slicing_at_start(self):
        [a, b] = self.reader[:2]
        self.assertEqual(a.record_index, self.reader[0].record_index)
        self.assertEqual(b.record_index, self.reader[1].record_index)

    def test_slicing_at_end(self):
        [c, d] = self.reader[-2:]
        self.assertEqual(c.record_index, self.reader[-2].record_index)
        self.assertEqual(d.record_index, self.reader[-1].record_index)

    def test_reversing_always_decrements(self):
        filtered_reader = self.reader.filtered_by_fields(stream_ids="100-1")
        prev_timestamp = 999999999
        for record in filtered_reader[::-1]:
            self.assertLessEqual(record.timestamp, prev_timestamp)
            prev_timestamp = record.timestamp

    def test_filtering_by_stream_ids_still_in_timestamp_order(self):
        filtered_reader = self.reader.filtered_by_fields(stream_ids="100-1")
        prev_timestamp = -9999999
        for record in filtered_reader:
            self.assertGreaterEqual(record.timestamp, prev_timestamp)
            prev_timestamp = record.timestamp

    def test_get_records_count(self):
        self.assertEqual(
            self.reader.get_records_count("100-1", RecordType.CONFIGURATION), 1
        )
        self.assertEqual(self.reader.get_records_count("100-1", RecordType.DATA), 500)
        self.assertEqual(self.reader.get_records_count("100-1", RecordType.STATE), 1)

    def test_get_record_index_by_time(self):
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids="100-1", record_types=["data"]
        )
        self.assertEqual(filtered_reader.get_record_index_by_time("100-1", 1.0), 0)
        self.assertEqual(filtered_reader.get_record_index_by_time("100-1", 1.2), 5)
        self.assertEqual(filtered_reader.get_record_index_by_time("100-1", 10.0), 226)

        # Testing nearest record.
        self.assertEqual(
            filtered_reader.get_record_index_by_time("100-1", 1.0390, 1e-2), 1
        )
        self.assertEqual(
            filtered_reader.get_record_index_by_time("100-1", 1.0410, 1e-2), 1
        )

        # Testing exception
        # TODO: Replace this to TimestampNotFoundError once we can make sure it is safe to replace the existing API.
        with self.assertRaises(ValueError):
            # Using timestamp > last record timestamp
            filtered_reader.get_record_index_by_time("100-1", 21)

        # Record exists at 5.184436352000375
        # When we use epsilon 1e-6, 5.18443535200 - 5.18443735200 is the range,
        # therefore we should see the raise right above the threshold
        with self.assertRaises(TimestampNotFoundError):
            filtered_reader.get_record_index_by_time("100-1", 5.18443535, 1e-6)
        with self.assertRaises(TimestampNotFoundError):
            filtered_reader.get_record_index_by_time("100-1", 5.18443736, 1e-6)

    def test_read_recod_by_time(self):
        record = self.reader.read_record_by_time("100-1", 4.52)
        self.assertEqual(record.record_index, 1710)
        self.assertEqual(record.stream_id, "100-1")
        self.assertEqual(record.record_type, "data")
        self.assertEqual(record.timestamp, 4.520000000000003)

        # Testing nearest record.
        record = self.reader.read_record_by_time("100-1", 4.2, 1e-3)
        self.assertEqual(record.record_index, 1558)
        self.assertEqual(record.stream_id, "100-1")
        self.assertEqual(record.record_type, "data")
        self.assertEqual(record.timestamp, 4.200000000000003)

        # Testing exception
        # TODO: Replace this to TimestampNotFoundError once we can make sure it is safe to replace the existing API.
        with self.assertRaises(ValueError):
            # Using timestamp > last record timestamp
            self.reader.read_record_by_time("100-1", 21)

        # Record exists at 4.200000000000003
        # When we use epsilon 1e-3, 4.199000000000003 - 4.201000000000003 is the range,
        # therefore we should see the raise right above the threshold
        with self.assertRaises(TimestampNotFoundError):
            self.reader.read_record_by_time("100-1", 4.1989, 1e-3)
        with self.assertRaises(TimestampNotFoundError):
            self.reader.read_record_by_time("100-1", 4.2011, 1e-3)

    def test_get_timestamp_list(self):
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=["100-1"], record_types=["data"]
        )
        timestamp_list = filtered_reader.get_timestamp_list()
        self.assertEqual(len(timestamp_list), 500)
        self.assertAlmostEqual(timestamp_list[0], 1.0, 3)
        self.assertAlmostEqual(timestamp_list[1], 1.04, 3)
        self.assertAlmostEqual(timestamp_list[-1], 20.960, 3)

    def test_read_next_record(self):
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=["100-1"], record_types=["data"]
        )
        record = filtered_reader.read_next_record("100-2", "data", 1)
        self.assertEqual(record.stream_id, "100-2")
        self.assertEqual(record.record_index, 58)

        record = filtered_reader.read_next_record("100-2", "data", 5)
        self.assertEqual(record.stream_id, "100-2")
        self.assertEqual(record.record_index, 134)

        record = filtered_reader.read_next_record("100-2", "data", 10)
        self.assertEqual(record.stream_id, "100-2")
        self.assertEqual(record.record_index, 229)

        record = filtered_reader.read_next_record("100-1", "data", 500)
        self.assertEqual(record, None)

    def test_get_image_spec(self):
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=["100-1"], record_types=["data"]
        )
        # 3rd record in 100-1 is the first record with image
        record = filtered_reader[3]
        spec = record.image_specs[0]
        self.assertEqual(spec["width"], 200)
        self.assertEqual(spec["height"], 200)
        self.assertEqual(spec["pixel_format"], "grey8")


@parameterized_class(
    ("multi_path"),
    [
        [False],
        [True],
    ],
)
class TestReadMultiChannelImage(unittest.TestCase):
    def setUp(self):
        self.reader = SyncVRSReader(
            test_recording_path(),
            auto_read_configuration_records=True,
            multi_path=self.multi_path,
        )

    def test_grayscale_images(self):
        stream_ids = self.reader.find_streams(100, "test/synthetic/grey8")
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=stream_ids, record_types={"data"}
        )

        for record in filtered_reader:
            if not record.image_blocks:
                continue
            img = record.image_blocks[0]
            img_spec = record.image_specs[0]
            self.assertEqual(img_spec.pixel_format, "grey8")
            self.assertEqual(img.shape[0], 200)
            self.assertEqual(img.shape[1], 200)
            self.assertEqual(img.dtype, np.uint8)

    def test_rgb8_images(self):
        stream_ids = self.reader.find_streams(100, "test/synthetic/rgb8")
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=stream_ids, record_types={"data"}
        )
        for record in filtered_reader:
            if not record.image_blocks:
                continue
            img = record.image_blocks[0]
            img_spec = record.image_specs[0]
            self.assertEqual(img_spec.pixel_format, "rgb8")
            self.assertEqual(img.shape[0], 200)
            self.assertEqual(img.shape[1], 200)
            self.assertEqual(img.shape[2], 3)
            self.assertEqual(img.dtype, np.uint8)

    def test_depth32_images(self):
        stream_ids = self.reader.find_streams(100, "test/synthetic/depth32f")
        filtered_reader = self.reader.filtered_by_fields(
            stream_ids=stream_ids, record_types={"data"}
        )
        for record in filtered_reader:
            if not record.image_blocks:
                continue
            img = record.image_blocks[0]
            img_spec = record.image_specs[0]
            self.assertEqual(img_spec.pixel_format, "depth32f")
            self.assertEqual(img.shape[0], 200)
            self.assertEqual(img.shape[1], 200)
            self.assertEqual(img.dtype, np.float32)


@parameterized_class(
    ("multi_path"),
    [
        [False],
        [True],
    ],
)
class TestStreamTags(unittest.TestCase):
    def setUp(self):
        self.reader = SyncVRSReader(
            test_recording_path(),
            auto_read_configuration_records=True,
            multi_path=self.multi_path,
        )

    def test_find_stream(self):
        self.assertEqual(
            self.reader.find_streams(100, "test/synthetic/grey8"), ["100-1"]
        )
        self.assertEqual(
            self.reader.find_streams(100, "test/synthetic/bgr8"), ["100-2"]
        )

        self.assertEqual(self.reader.find_streams(100, "test/synthetic/grey42"), [])


class TestSyncVRSReader(unittest.TestCase):
    def test_json_path(self):
        path = {"chunks": [str(test_recording_path())], "storage": "diskfile"}
        reader = SyncVRSReader(path, auto_read_configuration_records=True)

        self.assertEqual(len(reader), 9538)


@parameterized_class(
    ("multi_path"),
    [
        [False],
        [True],
    ],
)
class TestImageConversion(unittest.TestCase):
    def setUp(self):
        self.reader = SyncVRSReader(
            test_recording_path(),
            auto_read_configuration_records=True,
            multi_path=self.multi_path,
        )

    def test_set_image_conversion_policy(self):
        self.reader.set_image_conversion(ImageConversion.NORMALIZE)
        self.assertEqual(
            self.reader.set_stream_type_image_conversion(
                100, ImageConversion.NORMALIZE_GREY8
            ),
            19,
        )
        self.reader.set_stream_image_conversion("100-1", ImageConversion.OFF)


class TestPixelFormat(unittest.TestCase):
    def setUp(self):
        self.reader = SyncVRSReader(test_recording_path())

    def test_pixel_format_auto_conversion(self):
        filtered_reader = self.reader.filtered_by_fields(record_types="data")
        self.assertEqual(len(filtered_reader), 9500)
        self.assertEqual(len(filtered_reader.stream_ids), 19)
        for i in range(10):
            grey8 = Image.fromarray(
                filtered_reader[i * len(filtered_reader.stream_ids)].image_blocks[0]
            ).convert("RGB")
            for j in range(len(filtered_reader.stream_ids)):
                idx = i * len(filtered_reader.stream_ids) + j
                pixel_format = filtered_reader[idx].image_specs[0]["pixel_format"]
                if (
                    "raw10" in pixel_format
                    or pixel_format == "rgb_ir_4x4"
                    or pixel_format == "yuy2"
                ):
                    continue
                elif "10" in pixel_format:
                    img = Image.fromarray(
                        np.uint8(filtered_reader[idx].image_blocks[0] // 4)
                    )
                elif "12" in pixel_format:
                    img = Image.fromarray(
                        np.uint8(filtered_reader[idx].image_blocks[0] // 16)
                    )
                elif pixel_format == "rgb32F":
                    block = rgbx_float_to_rgb8(filtered_reader[idx].image_blocks[0], 3)
                    img = Image.fromarray(block)
                elif pixel_format == "rgba32F":
                    block = rgbx_float_to_rgb8(filtered_reader[idx].image_blocks[0], 4)
                    img = Image.fromarray(block)

                else:
                    img = Image.fromarray(filtered_reader[idx].image_blocks[0])
                img = img.convert("RGB")
                self.assertEqual(grey8, img)


if __name__ == "__main__":
    unittest.main()
