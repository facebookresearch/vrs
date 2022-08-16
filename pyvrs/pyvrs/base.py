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
from typing import Any, Dict, List, Mapping, Optional, Set, Union

from . import ImageConversion, RecordType

from .record import VRSRecord
from .slice import VRSReaderSlice


class BaseVRSReader(ABC):
    """A Pythonic reader for VRS files. Behaves as a filterable list - has a length
    (number of records), can be indexed to retrieve VRSRecords, and can be iterated
    over and sliced just like a regular Python list. Significant file reads are
    only done when record state is queried, so it remains performant even for larger
    VRS files.

    This has several child classes and following are the relationship between them.
    ::

                                        |-- SyncVRSReader
                        |-- VRSReader --
                        |               |-- AsyncVRSReader
        BaseVRSReader --|
                        |                       |-- SyncFilteredVRSReader
                        |-- FilteredVRSReader --
                                                |-- AsyncFilteredVRSReader

    Note:
        - BaseVRSReader: Base abstract class that defines the common functions across all child classes.
        - VRSReader: Abstract class that represents entire file (e.g. file without any filters).
          The methods in child classes of this operats against all records.
          When user call filtered_by_fields method, that call will create FilteredVRSReader that represents slice of the file.
        - FilteredVRSReader: Abstract class that represents the slice of the original file (After applying filter).
          This class essentially has the exact same methods as VRSReader but operate against subset of the file.
          Note that you can't 're-filter' an already filtered VRSReader.
        - SyncVRSReader: Synchronous version of VRSReader.
        - AsyncVRSReader: Asynchronous version of VRSReader, only difference between SyncVRSReader is AsyncVRSReader supports
          __aiter__, __anext__ for async iteration as well as __getitem__ operates asynchronously.
        - SyncFilteredVRSReader: Synchronous version of FilteredVRSReader.
        - AsyncFilteredVRSReader: Asynchronous version of FilteredVRSReader, same difference as SyncVRSReader vs AsyncVRSReader.
    """

    @abstractmethod
    def __getitem__(self, i: Union[int, slice]) -> Union[VRSRecord, VRSReaderSlice]:
        raise NotImplementedError()

    @abstractmethod
    def __len__(self) -> int:
        raise NotImplementedError()

    @abstractmethod
    def __repr__(self) -> str:
        raise NotImplementedError()

    @abstractmethod
    def __str__(self) -> str:
        raise NotImplementedError()

    @property
    @abstractmethod
    def file_tags(self) -> Mapping[str, str]:
        """
        Return a dict of all file tags present in this VRS file.

        Returns:
            Dictionary of all file tags: {<tag>: <value>}
        """
        raise NotImplementedError()

    @property
    @abstractmethod
    def stream_tags(self) -> Mapping[str, Mapping[str, Any]]:
        """
        Return a dict of all per-stream tags present in this VRS file.

        Returns:
            Dictionary of all per-stream tags: {<stream_id>: {<tag>: <value>}}
        """
        raise NotImplementedError()

    @property
    @abstractmethod
    def n_records(self) -> int:
        """Return a number of records in this VRS file."""
        raise NotImplementedError()

    @property
    @abstractmethod
    def record_types(self) -> Set[str]:
        """Return a set of record types in this VRS file."""
        raise NotImplementedError()

    @property
    @abstractmethod
    def stream_ids(self) -> Set[str]:
        """Return a set of stream ids in this VRS file."""
        raise NotImplementedError()

    @property
    @abstractmethod
    def min_timestamp(self) -> float:
        """Return a minimum timestamp of this VRS file."""
        raise NotImplementedError()

    @property
    @abstractmethod
    def max_timestamp(self) -> float:
        """Return a maximum timestamp of this VRS file."""
        raise NotImplementedError()

    @property
    def time_range(self) -> float:
        """Return a timestamp range of this VRS file."""
        return self.max_timestamp - self.min_timestamp

    @abstractmethod
    def find_stream(
        self, recordable_type_id: int, tag_name: str, tag_value: str
    ) -> str:
        """
        Find stream matching recordable type and tag, and return its stream id.

        Args:
            recordable_type_id: stream_id is `<recordable_type_id>-<instance_id>`
            tag_name: tag name that you are interested in
            tag_value: tag value that you are interested in

        Returns:
            Stream ID that starts with recordable_type_id and has a given tag pair.
        """
        raise NotImplementedError()

    @abstractmethod
    def find_streams(self, recordable_type_id: int, flavor: str = "") -> List[str]:
        """
        Find streams matching recordable type and flavor, and return sets of stream ids.

        Args:
            recordable_type_id: stream_id is `<recordable_type_id>-<instance_id>`
            tag_name: tag name that you are interested in
            tag_value: tag value that you are interested in

        Returns:
            A set of stream IDs that start with recordable_type_id and has a given flavor.
        """
        raise NotImplementedError()

    @abstractmethod
    def get_stream_info(self, stream_id: str) -> Dict[str, str]:
        """
        Get details about a stream.

        Args:
            stream_id: stream_id you are interested in.

        Returns:
            An information about the stream in a dictionary.
        """
        raise NotImplementedError()

    @abstractmethod
    def get_records_count(self, stream_id: str, record_type: RecordType) -> int:
        """
        Get the number of records for the stream_id & record_type.

        Args:
            stream_id: stream_id you are interested in.
            record_type: record type you are interested in.

        Returns:
            The number of records for stream_id & record type
        """
        raise NotImplementedError()

    @abstractmethod
    def get_timestamp_list(self, indices: Optional[List[int]] = None) -> List[float]:
        """
        Get the list of timestamps corresponding to the given indices.

        Args:
            indices: the list of indices we want to get the timestamp.

        Returns:
            A list of timestamps correspond to the indices, if indices are None, we get the full timestamp list.
        """
        raise NotImplementedError()

    @abstractmethod
    def get_timestamp_for_index(self, index: int) -> float:
        """
        Get the timestamp corresponding to the given index.

        Args:
            index: the index for the record

        Returns:
            A timestamp corresponds to the index
        """
        raise NotImplementedError()

    @abstractmethod
    def set_image_conversion(self, conversion: ImageConversion) -> None:
        """
        Set default image conversion policy, and clears any stream specific setting.

        Args:
            conversion: The image conversion you want to apply for all streams.
        """
        raise NotImplementedError()

    @abstractmethod
    def set_stream_image_conversion(
        self, stream_id: str, conversion: ImageConversion
    ) -> None:
        """
        Set image conversion policy for a specific stream.

        Args:
            stream_id: The stream_id you want to apply image conversion to.
            conversion: The image conversion you want to apply for a specific stream.
        """
        raise NotImplementedError()

    @abstractmethod
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
        raise NotImplementedError()

    @abstractmethod
    def might_contain_images(self, stream_id: str) -> bool:
        """
        Check if the given stream_id contains an image data.

        Args:
            stream_id: stream_id that you are interested in.

        Returns:
            Based on the config record, return if the stream contains an image data.
        """
        raise NotImplementedError()

    @abstractmethod
    def might_contain_audio(self, stream_id: str) -> bool:
        """
        Check if the given stream_id contains an audio data.

        Args:
            stream_id: stream_id that you are interested in.

        Returns:
            Based on the config record, return if the stream contains an audio data.
        """
        raise NotImplementedError()

    @abstractmethod
    def get_estimated_frame_rate(self, stream_id: str) -> float:
        """
        Get the estimated frame rate for the given stream_id.

        Args:
            stream_id: stream_id that you are interested in.

        Returns:
            The estimated frame rate.
        """
        raise NotImplementedError()

    @abstractmethod
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

        Returns:
            The absolute index of the record corresponds to the stream_id & timestamp.

        Raises:
            TimestampNotFoundError: If epsilon is not None and the record doesn't exist within the time range.
            ValueError: If epsilon is None and the record isn't found using lower_bound.
        """
        raise NotImplementedError()

    @abstractmethod
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
        raise NotImplementedError()

    @abstractmethod
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
        raise NotImplementedError()

    @abstractmethod
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
        raise NotImplementedError()

    @abstractmethod
    def _read_record(self, indices: List[int], i: Union[int, slice]):
        raise NotImplementedError()

    @abstractmethod
    def _record_count_by_type_from_stream_id(self, stream_id: str) -> Mapping[str, int]:
        """Retrieve the number of messages from a given stream id sorted by type."""
        raise NotImplementedError()

    @abstractmethod
    def _generate_filtered_indices(self) -> List[int]:
        raise NotImplementedError()
