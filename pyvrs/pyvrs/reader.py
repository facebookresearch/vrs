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

import json
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Set, Union

from . import (
    AsyncMultiReader,
    AsyncReader,
    ImageConversion,
    MultiReader,
    Reader,
    RecordableTypeId,
    RecordType,
)

from .base import BaseVRSReader
from .filter import (
    AsyncFilteredVRSReader,
    FilteredVRSReader,
    RecordFilter,
    SyncFilteredVRSReader,
)
from .record import VRSRecord
from .slice import (
    async_index_or_slice_records,
    AsyncVRSReaderSlice,
    index_or_slice_records,
    VRSReaderSlice,
)
from .utils import (
    filter_by_record_type,
    filter_by_stream_ids,
    get_recordable_type_id_name,
    string_of_set,
    tags_to_justified_table_str,
)


__all__ = [
    "AsyncVRSReader",
    "SyncVRSReader",
    "VRSReader",
]


PathType = Union[
    Path, str, List[Path], List[str], Dict[str, Union[int, str, List[str]]]
]


class VRSReader(BaseVRSReader, ABC):
    """A Pythonic reader for VRS files. Behaves as a filterable list - has a length
    (number of records), can be indexed to retrieve VRSRecords, and can be iterated
    over and sliced just like a regular Python list. Significant file reads are
    only done when record state is queried, so it remains performant even for larger
    VRS files.

    The methods in child classes of VRSReader operates against all records. When user call
    filtered_by_fields method, that call creates FilteredVRSReader that represents a slice of the file.

    Example:
        Basic usage:

        >>> reader = SyncVRSReader("path/to/a.vrs")
        >>> print(reader)  # print to get an overview of what's in the file
        >>> for record in reader:  # loop over all records
        >>>     print(record)  # print all the state of the record

        Filters can be added to only consider certain types of records from certain
        streams:

        >>> filtered_reader = reader.filtered_by_fields(
        >>>                     stream_ids = {'1010-*', '1001-*'}, # limit to some streams
        >>>                     record_types = {'configuration'}, # only read config records
        >>>                   )
        >>> for filtered_record in filtered_reader:
        >>>     print(filtered_record)  # only configs from streams matching 1010-*/1001-*

        List slicing and indexing is supported:

        >>> for one_interesting_record in reader[42]:
        >>>     print(one_interesting_record)

        >>> for every_100th_record in reader[::100]:
        >>>     print(every_100th_record)

        >>> for a_last_five_record in reader[-5:]:
        >>>     print(a_last_five_record)

    **IMPORTANT NOTE ON CONFIGURATION RECORDS**

    VRS is a streaming file format, which means order of record reads
    matters for one important case - namely reading configuration records, as
    they instruct VRS how to parse future data records. It's possible that a
    single stream (i.e. the list of records associated with a single Recordable ID)
    contains multiple records of type 'configuration', each of which encode how
    future records in this stream should be interpreted. An example of this would
    be if the resolution of a image changes mid-stream - a new configuration would
    encode the new image size, and reading linearly through the records would mean
    that VRS understands how to parse the images correctly.

    `pyvrs` surfaces a flexible random access API, with list-like behavior. This is
    very convenient and powerful, but requires careful management of read order of
    configuration records - if the matching preceding configuration record is not
    read, a record may not be interpretable at all.

    `pyvrs` can assist users by always ensuring preceding configuration
    records are automatically read before any record access. This, for the vast
    majority of use cases, 'just works', and allows people to read all records in a
    file without consideration to configuration records. However, for some niche use
    cases (i.e. badly formatted files with incorrect configuration record placement)
    this automated behavior could be undesirable.

    To avoid any confusion on the matter, `pyvrs` requires that users decide whether
    or not they want to opt-in to automatic configuration record reading at
    instantiation of the VRSReader. To opt-in, pass:

        auto_read_configuration_records=True

    to the constructor. In most cases, this is the behavior non-VRS experts want,
    and we set auto_read_configuration_records=True by default.

    If you would prefer to disable all automatic reading of configuration records,
    pass:

        auto_read_configuration_records=False

    Users who pass this must take care to manually read configuration records in
    the correct order before reading data records.

    If you wish to read multiple VRS files simultaneously, use the following flag (False by default):

        multi_path=True
    """

    def __init__(
        self,
        path: PathType,
        auto_read_configuration_records: bool = True,
        encoding: str = "utf-8",
        multi_path: bool = False,
    ) -> None:
        """
        Args:
            path: a path to the VRS file
            auto_read_configuration_records: When this is True, configuration records are automatically read
            encoding: set encoding to use for strings while reading the records
            multi_path: set True if you want to read multiple VRS files simultaneously
        """
        self._init_reader(path, auto_read_configuration_records, multi_path)
        self._reader.set_encoding(encoding)
        # Read all streams so we can index absolutely for the lifetime of
        # this reader
        self._reader.enable_all_streams()
        self._n_records = self._reader.get_available_records_size()
        self._record_types = self._reader.get_available_record_types()
        self._stream_ids = self._reader.get_available_stream_ids()
        self._min_timestamp = self._reader.get_min_available_timestamp()
        self._max_timestamp = self._reader.get_max_available_timestamp()

        self._record_counts_by_type = {}
        self._auto_read_configuration_records = auto_read_configuration_records

    def _init_reader(
        self,
        path: PathType,
        auto_read_configuration_records: bool,
        multi_path: bool,
    ):
        if multi_path:
            if isinstance(path, List):
                path = [str(p) for p in path]
            else:
                path = [str(path)]
        else:
            if isinstance(path, List):
                path = json.dumps({"chunks": path})
            elif isinstance(path, Dict):
                path = json.dumps(path)
        reader_cls = self._get_reader_class(multi_path=multi_path)
        self._reader = reader_cls(auto_read_configuration_records)
        self._open_files(multi_path, path)

    def close(self):
        """explicitly close the VRS reader without waiting for Python grabage collection."""
        return self._reader.close()

    def __getitem__(self, i: Union[int, slice]) -> Union[VRSRecord, VRSReaderSlice]:
        return self._read_record(range(self.n_records), i)

    def __len__(self) -> int:
        return self.n_records

    @abstractmethod
    def __repr__(self) -> str:
        raise NotImplementedError()

    def __str__(self) -> str:
        s = "\n".join(
            [
                self._path,
                tags_to_justified_table_str(self.file_tags),
                f"All {len(self)} records are enabled (no filters applied)",
                "Automatic configuration record reading is {}".format(
                    "enabled" if self._auto_read_configuration_records else "disabled"
                ),
                "Available Stream IDs: {}".format(string_of_set(self.stream_ids)),
                "Available Record Types: {}".format(string_of_set(self.record_types)),
            ]
        )
        if len(self) > 0:
            s += "\n" + "\n".join(
                [
                    "{:.2f}s of available records: {:.2f}s - {:.2f}s".format(
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

    @property
    def file_tags(self) -> Mapping[str, str]:
        """
        Return a dict of all file tags present in this VRS file.

        Returns:
            Dictionary of all file tags: {<tag>: <value>}
        """
        return self._reader.get_tags()

    @property
    def stream_tags(self) -> Mapping[str, Mapping[str, Any]]:
        """
        Return a dict of all per-stream tags present in this VRS file.

        Returns:
            Dictionary of all per-stream tags: {<stream_id>: {<tag>: <value>}}
        """
        return {k: self._reader.get_tags(k) for k in self.stream_ids}

    @property
    def n_records(self) -> int:
        """Return a number of records in this VRS file."""
        return self._n_records

    @property
    def record_types(self) -> Set[str]:
        """Return a set of record types in this VRS file."""
        return self._record_types

    @property
    def stream_ids(self) -> Set[str]:
        """Return a set of stream ids in this VRS file."""
        return self._stream_ids

    @property
    def min_timestamp(self) -> float:
        """Return a minimum timestamp of this VRS file."""
        return self._min_timestamp

    @property
    def max_timestamp(self) -> float:
        """Return a maximum timestamp of this VRS file."""
        return self._max_timestamp

    @property
    def time_range(self) -> float:
        """Return a timestamp range of this VRS file."""
        return self.max_timestamp - self.min_timestamp

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
        return self._reader.find_stream(
            RecordableTypeId(recordable_type_id), tag_name, tag_value
        )

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
        if recordable_type_id == 0:
            recordable_type_id = 65535  # "Undefined", to filter by flavor only
        return self._reader.get_streams(RecordableTypeId(recordable_type_id), flavor)

    def get_stream_info(self, stream_id: str) -> Dict[str, str]:
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

    def get_timestamp_list(self, indices: Optional[List[int]] = None) -> List[float]:
        """
        Get the list of timestamps corresponding to the given indices.

        Args:
            indices: the list of indices we want to get the timestamp.

        Returns:
            A list of timestamps correspond to the indices, if indices are None, we get the full timestamp list.
        """
        if indices is None:
            indices = range(self.n_records)
        return self._reader.get_timestamp_list_for_indices(indices)

    def get_timestamp_for_index(self, index: int) -> float:
        """
        Get the timestamp corresponding to the given index.

        Args:
            index: the index for the record

        Returns:
            A timestamp corresponds to the index
        """
        if index >= self.n_records:
            raise IndexError("Index {} is out of range.".format(index))
        return self._reader.get_timestamp_for_index(index)

    def set_image_conversion(self, conversion: ImageConversion) -> None:
        """
        Set default image conversion policy, and clears any stream specific setting.

        Args:
            conversion: The image conversion you want to apply for all streams.
        """
        self._reader.set_image_conversion(conversion)

    def set_stream_image_conversion(
        self, stream_id: str, conversion: ImageConversion
    ) -> None:
        """
        Set image conversion policy for a specific stream.

        Args:
            stream_id: The stream_id you want to apply image conversion to.
            conversion: The image conversion you want to apply for a specific stream.
        """
        self._reader.set_image_conversion(stream_id, conversion)

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
        return self._reader.set_image_conversion(
            RecordableTypeId(recordable_type_id), conversion
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

        Returns:
            The absolute index of the record corresponds to the stream_id & timestamp.

        Raises:
            TimestampNotFoundError: If epsilon is not None and the record doesn't exist within the time range.
            ValueError: If epsilon is None and the record isn't found using lower_bound.
        """
        assert stream_id in self.stream_ids
        if epsilon:
            return self._reader.get_nearest_record_index_by_time(
                timestamp, epsilon, stream_id
            )
        else:
            return self._reader.get_record_index_by_time(stream_id, timestamp)

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
        if epsilon:
            index_in_all = self._reader.get_nearest_record_index_by_time(
                timestamp, epsilon, stream_id
            )
        else:
            index_in_all = self._reader.get_record_index_by_time(stream_id, timestamp)
        record = self._reader.read_record(index_in_all)
        return VRSRecord(record)

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
        try:
            prev_index = self._reader.get_prev_index(stream_id, record_type, index)
        except IndexError as e:
            print(e)
            return None
        record = self._reader.read_record(prev_index)
        return VRSRecord(record)

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
            next_index = self._reader.get_next_index(stream_id, record_type, index)
        except IndexError as e:
            print(e)
            return None
        record = self._reader.read_record(next_index)
        return VRSRecord(record)

    def _read_record(
        self, indices: List[int], i: Union[int, slice]
    ) -> Union[VRSRecord, VRSReaderSlice]:
        return index_or_slice_records(self._path, self._reader, indices, i)

    def _record_count_by_type_from_stream_id(self, stream_id: str) -> Mapping[str, int]:
        """Retrieve the number of records from a given stream id sorted by type."""
        if stream_id in self._record_counts_by_type:
            return self._record_counts_by_type[stream_id]

        self._record_counts_by_type[
            stream_id
        ] = self._reader.record_count_by_type_from_stream_id(stream_id)
        return self._record_counts_by_type[stream_id]

    def _generate_filtered_indices(self, record_filter: RecordFilter) -> List[int]:
        return self._reader.regenerate_enabled_indices(
            record_filter.record_types,
            record_filter.stream_ids,
            record_filter.min_timestamp,
            record_filter.max_timestamp,
        )

    @abstractmethod
    def _get_reader_class(self, multi_path: bool):
        raise NotImplementedError()

    @abstractmethod
    def _open_files(self, multi_path: bool, path: PathType):
        raise NotImplementedError()

    @abstractmethod
    def filtered_by_fields(
        self,
        *,
        stream_ids: Optional[Union[Set[str], str]] = None,
        record_types: Optional[Union[Set[str], str]] = None,
        min_timestamp: Optional[float] = None,
        max_timestamp: Optional[float] = None,
    ) -> "FilteredVRSReader":
        """Filter this reader to only read records with given condition.

        Args:
            stream_ids: Stream Ids that you are interested in.
                Note:
                    Filtering can be performed by glob-like wildcard matching,
                    e.g. if the available Stream IDs are {'1010-1', '1011-1', '1011-2', '1011-10'}
                    >>> reader.filtered_by_fields(stream_ids='1011*')
                    would keep '1011-1', '1011-2', '1011-10', but would discard '1010-1'.
                    >>> reader.filtered_by_fields(stream_ids='1011-?')
                    would only keep '1011-1', '1011-2'.
                    >>> reader.filtered_by_fields(stream_ids='1011-1')
                    would only keep '1011-1'.
            record_types: Record types that you are interested in. The options are {"data", "configuration", "state"}.
            min_timestamp: Minimum timestamp you want to read from.
            max_timestamp: Maxmum timestamp you want to read to.

        Returns:
            FilteredVRSReader that represents filtered VRS file.
        """
        raise NotImplementedError()


class SyncVRSReader(VRSReader):
    def _get_reader_class(self, multi_path: bool):
        """Returns pybind reader class to construct based on the parameter.
        This will return either, vrsbindings.Reader, MultiReader.
        """
        if multi_path:
            return MultiReader
        else:
            return Reader

    def _open_files(self, multi_path: bool, path: PathType):
        if multi_path:
            self._reader.open(path)
        else:
            self._reader.open(str(path))
        self._path = str(path)

    def __repr__(self) -> str:
        return (
            f"SyncVRSReader({self._path!r}, "
            f"auto_read_configuration_records={self._auto_read_configuration_records!r})"
        )

    def filtered_by_fields(
        self,
        *,
        stream_ids: Optional[Union[Set[str], str]] = None,
        record_types: Optional[Union[Set[str], str]] = None,
        min_timestamp: Optional[float] = None,
        max_timestamp: Optional[float] = None,
    ) -> SyncFilteredVRSReader:
        """Filter this reader to only read records with given condition.

        Args:
            stream_ids: Stream Ids that you are interested in.
                Note:
                    Filtering can be performed by glob-like wildcard matching,
                    e.g. if the available Stream IDs are {'1010-1', '1011-1', '1011-2', '1011-10'}
                    >>> reader.filtered_by_fields(stream_ids='1011*')
                    would keep '1011-1', '1011-2', '1011-10', but would discard '1010-1'.
                    >>> reader.filtered_by_fields(stream_ids='1011-?')
                    would only keep '1011-1', '1011-2'.
                    >>> reader.filtered_by_fields(stream_ids='1011-1')
                    would only keep '1011-1'.
            record_types: Record types that you are interested in. The options are {"data", "configuration", "state"}.
            min_timestamp: Minimum timestamp you want to read from.
            max_timestamp: Maxmum timestamp you want to read to.

        Returns:
            FilteredVRSReader that represents filtered VRS file.
        """
        # apply default values if not provided
        if stream_ids is None:
            stream_ids = self.stream_ids
        elif isinstance(stream_ids, str):
            stream_ids = {stream_ids}
        if record_types is None:
            record_types = self.record_types
        elif isinstance(record_types, str):
            record_types = {record_types}
        if min_timestamp is None:
            min_timestamp = self.min_timestamp
        if max_timestamp is None:
            max_timestamp = self.max_timestamp

        record_filter = RecordFilter(
            record_types=filter_by_record_type(self.record_types, record_types),
            stream_ids=filter_by_stream_ids(self.stream_ids, stream_ids),
            min_timestamp=min_timestamp,
            max_timestamp=max_timestamp,
        )
        return SyncFilteredVRSReader(self, record_filter)


class AsyncVRSReader(VRSReader):
    def _open_files(self, multi_path: bool, path: PathType):
        if multi_path:
            self._reader.open(path)
        else:
            self._reader.open(str(path))
        self._path = str(path)

    def _get_reader_class(self, multi_path: bool):
        if multi_path:
            return AsyncMultiReader
        else:
            return AsyncReader

    def __repr__(self) -> str:
        return (
            f"AsyncVRSReader({self._path!r}, "
            f"auto_read_configuration_records={self._auto_read_configuration_records!r})"
        )

    def filtered_by_fields(
        self,
        stream_ids: Optional[Union[Set[str], str]] = None,
        record_types: Optional[Union[Set[str], str]] = None,
        min_timestamp: Optional[float] = None,
        max_timestamp: Optional[float] = None,
    ) -> AsyncFilteredVRSReader:
        """Filter this reader to only read records with given condition.

        Args:
            stream_ids: Stream Ids that you are interested in.
                Note:
                    Filtering can be performed by glob-like wildcard matching,
                    e.g. if the available Stream IDs are {'1010-1', '1011-1', '1011-2', '1011-10'}
                    >>> reader.filtered_by_fields(stream_ids='1011*')
                    would keep '1011-1', '1011-2', '1011-10', but would discard '1010-1'.
                    >>> reader.filtered_by_fields(stream_ids='1011-?')
                    would only keep '1011-1', '1011-2'.
                    >>> reader.filtered_by_fields(stream_ids='1011-1')
                    would only keep '1011-1'.
            record_types: Record types that you are interested in. The options are {"data", "configuration", "state"}.
            min_timestamp: Minimum timestamp you want to read from.
            max_timestamp: Maxmum timestamp you want to read to.

        Returns:
            AsyncFilteredVRSReader that represents filtered VRS file.
        """
        # apply default values if not provided
        if stream_ids is None:
            stream_ids = self.stream_ids
        elif isinstance(stream_ids, str):
            stream_ids = {stream_ids}
        if record_types is None:
            record_types = self.record_types
        elif isinstance(record_types, str):
            record_types = {record_types}
        if min_timestamp is None:
            min_timestamp = self.min_timestamp
        if max_timestamp is None:
            max_timestamp = self.max_timestamp

        record_filter = RecordFilter(
            record_types=filter_by_record_type(self.record_types, record_types),
            stream_ids=filter_by_stream_ids(self.stream_ids, stream_ids),
            min_timestamp=min_timestamp,
            max_timestamp=max_timestamp,
        )
        return AsyncFilteredVRSReader(self, record_filter)

    def __aiter__(self) -> "AsyncVRSReader":
        self._index = 0
        return self

    async def __anext__(self) -> Union[VRSRecord, AsyncVRSReaderSlice]:
        if self._index == len(self):
            raise StopAsyncIteration
        result = await self[self._index]
        self._index += 1
        return result

    async def __getitem__(
        self, i: Union[int, slice]
    ) -> Union[VRSRecord, AsyncVRSReaderSlice]:
        return await self._async_read_record(range(self.n_records), i)

    async def _async_read_record(self, indices: List[int], i: Union[int, slice]):
        return await async_index_or_slice_records(self._path, self._reader, indices, i)
