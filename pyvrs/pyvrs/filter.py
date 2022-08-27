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

from abc import ABC, abstractmethod
from bisect import bisect
from dataclasses import dataclass
from typing import Any, List, Mapping, Optional, Set, Union

from . import ImageConversion, RecordType

from .base import BaseVRSReader
from .record import VRSRecord
from .slice import VRSReaderSlice
from .utils import (
    get_recordable_type_id_name,
    string_of_set,
    tags_to_justified_table_str,
)

__all__ = [
    "FilteredVRSReader",
    "SyncFilteredVRSReader",
    "RecordFilter",
]


@dataclass
class RecordFilter:
    """RecordFilter represents a filter that's applied to the VRS file."""

    record_types: Set[str]
    stream_ids: Set[str]
    min_timestamp: float
    max_timestamp: float


class FilteredVRSReader(BaseVRSReader, ABC):
    """FilteredVRSReader represents subset of VRSReader after applying filter.
    This class essentially has the exact same methods as VRSReader but operates against subset of the file.
    Note that you can't `re-filter` an already filtered VRSReader.
    """

    def __init__(self, reader: BaseVRSReader, record_filter: RecordFilter):
        """
        Args:
            reader: reader object for the whole VRS file (i.e. without any filters)
            record_fitler: filter that's applied to the VRS file
        """
        self._reader = reader
        self._record_filter = record_filter

        self._filtered_indices = self._generate_filtered_indices()
        self._min_timestamp = (
            0
            if len(self._filtered_indices) == 0
            else self._reader.get_timestamp_for_index(self._filtered_indices[0])
        )
        self._max_timestamp = (
            0
            if len(self._filtered_indices) == 0
            else self._reader.get_timestamp_for_index(self._filtered_indices[-1])
        )

    def __getitem__(self, i: Union[int, slice]) -> Union[VRSRecord, VRSReaderSlice]:
        raise NotImplementedError()

    def __len__(self) -> int:
        return self.n_records

    def __repr__(self) -> str:
        raise NotImplementedError()

    def __str__(self) -> str:
        raise NotImplementedError()

    @property
    def file_tags(self) -> Mapping[str, str]:
        """
        Return a dict of all file tags present in this VRS file.

        Returns:
            Dictionary of all file tags: {<tag>: <value>}
        """
        return self._reader.file_tags

    @property
    def stream_tags(self) -> Mapping[str, Mapping[str, Any]]:
        """
        Return a dict of all file tags present in this VRS file.

        Returns:
            Dictionary of all file tags: {<tag>: <value>}
        """
        return self._reader.stream_tags

    @property
    def n_records(self) -> int:
        """The number of records that this reader is configured to read, given current filters."""
        return len(self._filtered_indices)

    @property
    def record_types(self) -> Set[str]:
        """The set of record types that this reader is configured to read, given current
        filters.
        """
        return self._record_filter.record_types

    @property
    def stream_ids(self) -> Set[str]:
        """The set of stream ids that this reader is configured to read, given current
        filters.
        """
        return self._record_filter.stream_ids

    @property
    def min_timestamp(self) -> float:
        """The timestamp for the first record that this reader is configured to read, given current filters.
        Return 0 if there are no record.
        """
        return self._min_timestamp

    @property
    def max_timestamp(self) -> float:
        """The timestamp for the last record that this reader is configured to read, given current filters.
        Return 0 if there are no record.
        """
        return self._max_timestamp

    @property
    def min_filter_timestamp(self) -> float:
        """The value of min_timestamp when user called filter method on VRSReader."""
        return self._record_filter.min_timestamp

    @property
    def max_filter_timestamp(self) -> float:
        """The value of max_timestamp when user called filter method on VRSReader."""
        return self._record_filter.max_timestamp

    def find_stream(
        self, recordable_type_id: int, tag_name: str, tag_value: str
    ) -> str:
        """
        Find stream matching recordable type and tag, and return its stream id.
        This call isn't affected by the filter.

        Args:
            recordable_type_id: stream_id is `<recordable_type_id>-<instance_id>`
            tag_name: tag name that you are interested in
            tag_value: tag value that you are interested in

        Returns:
            Stream ID that starts with recordable_type_id and has a given tag pair.
        """
        return self._reader.find_stream(recordable_type_id, tag_name, tag_value)

    def find_streams(self, recordable_type_id: int, flavor: str = "") -> List[str]:
        """
        Find streams matching recordable type and flavor, and return sets of stream ids.
        This call isn't affected by the filter.

        Args:
            recordable_type_id: stream_id is `<recordable_type_id>-<instance_id>`
            tag_name: tag name that you are interested in
            tag_value: tag value that you are interested in

        Returns:
            A set of stream IDs that start with recordable_type_id and has a given flavor.
        """
        return self._reader.find_streams(recordable_type_id, flavor)

    def get_stream_info(self, stream_id: str) -> Mapping[str, str]:
        """
        Get details about a stream.

        Args:
            stream_id: stream_id you are interested in.

        Returns:
            An information about the stream in a dictionary.
        """
        return self._reader.get_stream_info(stream_id)

    def get_records_count(self, stream_id: str, record_type: RecordType) -> int:
        """
        Get the number of records for the stream_id & record_type.

        Args:
            stream_id: stream_id you are interested in.
            record_type: record type you are interested in.

        Returns:
            The number of records for stream_id & record type
        """
        return self._reader.get_records_count(stream_id, record_type)

    def get_timestamp_list(self) -> List[float]:
        """
        Get the list of timestamps corresponding to the given indices.

        Args:
            indices: the list of indices we want to get the timestamp.

        Returns:
            A list of timestamps correspond to the indices, if indices are None, we get the full timestamp list.
        """
        return self._reader.get_timestamp_list(self._filtered_indices)

    def get_timestamp_for_index(self, index: int) -> float:
        """
        Get the timestamp corresponding to the given index.

        Args:
            index: the index for the record

        Returns:
            A timestamp corresponds to the index
        """
        if index >= len(self._filtered_indices):
            raise IndexError("Index {} is out of range.".format(index))
        return self._reader.get_timestamp_for_index(self._filtered_indices[index])

    def set_image_conversion(self, conversion: ImageConversion) -> None:
        """
        Set default image conversion policy, and clears any stream specific setting.

        Args:
            conversion: The image conversion you want to apply for all streams.
        """
        raise NotImplementedError(
            "This should be called in SyncVRSReader before applying filter."
        )

    def set_stream_image_conversion(
        self, stream_id: str, conversion: ImageConversion
    ) -> None:
        """
        Set image conversion policy for a specific stream.

        Args:
            stream_id: The stream_id you want to apply image conversion to.
            conversion: The image conversion you want to apply for a specific stream.
        """
        raise NotImplementedError(
            "This should be called in SyncVRSReader before applying filter."
        )

    def set_stream_type_image_conversion(
        self, recordable_type_id: str, conversion: ImageConversion
    ) -> int:
        """
        Set image conversion policy for streams of a specific device type.

        Args:
            recordable_type_id: The recordable_type_id you want to apply image conversion to.
                                If you specify 1000, streams with id 1000-* are the targets.
            conversion: The image conversion you want to apply for a specific stream.

        Returns:
            The number of streams affected.
        """
        raise NotImplementedError(
            "This should be called in SyncVRSReader before applying filter."
        )

    def might_contain_images(self, stream_id: str) -> bool:
        """
        Check if the given stream_id contains an image data.

        Args:
            stream_id: stream_id that you are interested in.

        Returns:
            Based on the config record, return if the stream contains an image data.
        """
        return self._reader.might_contain_images(stream_id)

    def might_contain_audio(self, stream_id: str) -> bool:
        """
        Check if the given stream_id contains an audio data.

        Args:
            stream_id: stream_id that you are interested in.

        Returns:
            Based on the config record, return if the stream contains an audio data.
        """
        return self._reader.might_contain_audio(stream_id)

    def get_estimated_frame_rate(self, stream_id: str) -> float:
        """
        Get the estimated frame rate for the given stream_id.

        Args:
            stream_id: stream_id that you are interested in.

        Returns:
            The estimated frame rate.
        """
        return self._reader.get_estimated_frame_rate(stream_id)

    def get_record_index_by_time(
        self, stream_id: str, timestamp: float, epsilon: Optional[float] = None
    ) -> int:
        """
        Get index in filtered records by timestamp.

        Args:
            stream_id: stream_id that you are interested in.
            timestamp: timestamp that you are interested in.
            epsilon: Optional argument. If specified we search for record in range of
               (timestamp-epsilon)-(timestamp+epsilon) and returns the nearest record.
        """
        assert stream_id in self.stream_ids

        index = self._reader.get_record_index_by_time(stream_id, timestamp, epsilon)

        # TODO: Fix this logic
        return max(bisect(self._filtered_indices, index) - 1, 0)

    def read_record_by_time(
        self, stream_id: str, timestamp: float, epsilon: Optional[float] = None
    ) -> VRSRecord:
        """
        Read record by timestamp.

        Args:
            stream_id: stream_id that you are interested in.
            timestamp: timestamp that you are interested in.
            epsilon: Optional argument. If specified we search for record in range of
               (timestamp-epsilon)-(timestamp+epsilon) and returns the nearest record.

        Returns:
            VRSRecord corresponds to the stream_id & timestamp.

        Raises:
            TimestampNotFoundError: If epsilon is not None and the record doesn't exist within the time range.
            ValueError: If epsilon is None and the record isn't found using lower_bound.
        """
        return self._reader.read_record_by_time(stream_id, timestamp, epsilon)

    def read_prev_record(
        self, stream_id: str, record_type: str, index: int
    ) -> Optional[VRSRecord]:
        """
        Read the last record that matches stream_id and record_type and its index is smaller or equal than given index.

        Args:
            stream_id: stream_id that you are interested in.
            record_type: record_type that you are interested in.
            index: the absolute index in the file. Based on this index, try to find the previous record that matches stream_id & record_type

        Returns:
            VRSRecord if there is a record, otherwise None
        """
        index_in_all = self._filtered_indices[index]
        return self._reader.read_prev_record(stream_id, record_type, index_in_all)

    def read_next_record(
        self, stream_id: str, record_type: str, index: int
    ) -> Optional[VRSRecord]:
        """
        Read the first record that matches stream_id and record_type and its index is greater or equal than given index.

        Args:
            stream_id: stream_id that you are interested in.
            record_type: record_type that you are interested in.
            index: the absolute index in the file. Based on this index, try to find the previous record that matches stream_id & record_type

        Returns:
            VRSRecord if there is a record, otherwise None
        """
        try:
            index_in_all = self._filtered_indices[index]
        except IndexError as e:
            print(e)
            return None
        return self._reader.read_next_record(stream_id, record_type, index_in_all)

    @abstractmethod
    def _read_record(
        self, indices: List[int], i: Union[int, slice]
    ) -> Union[VRSRecord, VRSReaderSlice]:
        raise NotImplementedError()

    def _record_count_by_type_from_stream_id(self, stream_id: str) -> Mapping[str, int]:
        return self._reader._record_count_by_type_from_stream_id(stream_id)

    def _generate_filtered_indices(self) -> List[int]:
        return self._reader._generate_filtered_indices(self._record_filter)


class SyncFilteredVRSReader(FilteredVRSReader):
    """
    Synchrnous version of FilteredVRSReader.
    """

    def __getitem__(self, i: Union[int, slice]) -> Union[VRSRecord, VRSReaderSlice]:
        return self._read_record(self._filtered_indices, i)

    def __repr__(self) -> str:
        return (
            f"SyncFilteredVRSReader({self._reader._path!r}, "
            f"auto_read_configuration_records={self._reader._auto_read_configuration_records!r})"
            f"filter={self._record_filter!r}"
        )

    def __str__(self) -> str:
        s = "\n".join(
            [
                self._reader._path,
                tags_to_justified_table_str(self.file_tags),
                f"{len(self)}/{len(self._reader)} records are enabled (based on filters)",
                "Automatic configuration record reading is {}".format(
                    "enabled"
                    if self._reader._auto_read_configuration_records
                    else "disabled"
                ),
                "Available Stream IDs: {}".format(
                    string_of_set(self._reader.stream_ids)
                ),
                "  Enabled Stream IDs: {}".format(string_of_set(self.stream_ids)),
                "Available Record Types: {}".format(
                    string_of_set(self._reader.record_types)
                ),
                "  Enabled Record Types: {}".format(string_of_set(self.record_types)),
            ]
        )
        if len(self) > 0:
            s += "\n" + "\n".join(
                [
                    "{:.2f}s of available records: {:.2f}s - {:.2f}s".format(
                        self._reader.time_range,
                        self._reader.min_timestamp,
                        self._reader.max_timestamp,
                    ),
                    "{:.2f}s of enabled records: {:.2f}s - {:.2f}s".format(
                        self.time_range,
                        self.min_timestamp,
                        self.max_timestamp,
                    ),
                ]
            )
        s += "\n\nAvailable Streams in VRS: \n"
        for stream_id in self.stream_ids:
            s += "  Stream ID: {} ({} no. {})\n      No. of records {}\n".format(
                stream_id,
                get_recordable_type_id_name(stream_id),
                stream_id.split("-")[1],
                self._record_count_by_type_from_stream_id(stream_id),
            )
        return s

    def _read_record(self, indices: List[int], i: Union[int, slice]):
        return self._reader._read_record(indices, i)


class AsyncFilteredVRSReader(FilteredVRSReader):
    def __init__(self, reader: BaseVRSReader, record_filter: RecordFilter):
        super().__init__(reader, record_filter)

        _async_read_record_op = getattr(self._reader, "_async_read_record", None)
        if not callable(_async_read_record_op):
            raise NotImplementedError(
                "AsyncFilteredVRSReader._reader doesn't have method _async_read_record."
                " You should only construct AsyncFilteredVRSReader via AsyncVRSReader.filtered_by_fields method."
            )

    def __aiter__(self):
        self._index = 0
        return self

    async def __anext__(self):
        if self._index == len(self):
            raise StopAsyncIteration
        result = await self[self._index]
        self._index += 1
        return result

    async def __getitem__(
        self, i: Union[int, slice]
    ) -> Union[VRSRecord, VRSReaderSlice]:
        return await self._reader._async_read_record(self._filtered_indices, i)

    def __repr__(self) -> str:
        return (
            f"AsyncFilteredVRSReader({self._reader._path!r}, "
            f"auto_read_configuration_records={self._reader._auto_read_configuration_records!r})"
            f"filter={self._record_filter!r}"
        )

    def __str__(self) -> str:
        s = "\n".join(
            [
                self._reader._path,
                tags_to_justified_table_str(self.file_tags),
                f"{len(self)}/{len(self._reader)} records are enabled (based on filters)",
                "Automatic configuration record reading is {}".format(
                    "enabled" if self._auto_read_configuration_records else "disabled"
                ),
                "Available Stream IDs: {}".format(
                    string_of_set(self._reader.stream_ids)
                ),
                "  Enabled Stream IDs: {}".format(string_of_set(self.stream_ids)),
                "Available Record Types: {}".format(
                    string_of_set(self._reader.record_types)
                ),
                "  Enabled Record Types: {}".format(string_of_set(self.record_types)),
            ]
        )
        if len(self) > 0:
            s += "\n" + "\n".join(
                [
                    "{:.2f}s of available records: {:.2f}s - {:.2f}s".format(
                        self._reader.time_range,
                        self._reader.min_timestamp,
                        self._reader.max_timestamp,
                    ),
                    "{:.2f}s of enabled records: {:.2f}s - {:.2f}s".format(
                        self.time_range,
                        self.min_timestamp,
                        self.max_timestamp,
                    ),
                ]
            )
        s += "\n\nAvailable Streams in VRS: \n"
        for stream_id in self.stream_ids:
            s += "  Stream ID: {} ({} no. {})\n      No. of records {}\n".format(
                stream_id,
                get_recordable_type_id_name(stream_id),
                stream_id.split("-")[1],
                self._record_count_by_type_from_stream_id(stream_id),
            )
        return s

    def _read_record(self, indices: List[int], i: Union[int, slice]):
        raise NotImplementedError()
