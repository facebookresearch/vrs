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

from pathlib import Path
from typing import List, Sequence, TYPE_CHECKING, Union

if TYPE_CHECKING:
    from .reader import AsyncVRSReader, VRSReader

from .record import VRSRecord


class VRSReaderSlice(Sequence):
    """A slice into a VRS file, i.e. an arbitrary collection of records. Can be further sliced and
    indexed, but looses some of the smarts present in the full VRSReader (e.g. the ability to
    filter) and some richer properties like temporal information.
    """

    def __init__(self, path: Union[str, Path], r, indices: List[int]) -> None:
        self._path = Path(path)
        self._reader = r
        self._indices = indices

    def __getitem__(self, i: Union[int, slice]) -> Union[VRSRecord, "VRSReaderSlice"]:
        return index_or_slice_records(self._path, self._reader, self._indices, i)

    def __len__(self) -> int:
        return len(self._indices)

    def __repr__(self) -> str:
        return f"VRSReaderSlice(path={str(self._path)!r}, len={len(self._indices)})"

    def __str__(self) -> str:
        return "\n".join(
            [str(self._path), f"Slice containing {len(self._indices)} records"]
        )


class AsyncVRSReaderSlice(Sequence):
    """A slice into a VRS file, i.e. an arbitrary collection of records. Can be further sliced and
    indexed, but looses some of the smarts present in the full VRSReader (e.g. the ability to
    filter) and some richer properties like temporal information.
    This should be created only via AsyncVRSReader.async_read_record call.
    """

    def __init__(self, path: Union[str, Path], r, indices: List[int]) -> None:
        self._path = Path(path)
        self._reader = r
        self._indices = indices

    def __aiter__(self):
        self.index = 0
        return self

    async def __anext__(self):
        if self.index == len(self):
            raise StopAsyncIteration
        result = await self[self.index]
        self.index += 1
        return result

    async def __getitem__(
        self, i: Union[int, slice]
    ) -> Union[VRSRecord, "AsyncVRSReaderSlice"]:
        return await async_index_or_slice_records(
            self._path, self._reader, self._indices, i
        )

    def __len__(self) -> int:
        return len(self._indices)

    def __repr__(self) -> str:
        return (
            f"AsyncVRSReaderSlice(path={str(self._path)!r}, len={len(self._indices)})"
        )

    def __str__(self) -> str:
        return "\n".join(
            [str(self._path), f"Slice containing {len(self._indices)} records"]
        )


def index_or_slice_records(
    path: Path,
    reader: "VRSReader",
    vrs_indices: List[int],
    indices: Union[int, slice],
) -> Union[VRSRecord, VRSReaderSlice]:
    """Shared logic to index or slice into a VRSReader or VRSReaderSlice. Returns either a
    VRSReaderSlice (if i is a slice or iterable) or a VRSRecord (if i is an integer).

    Args:
        path: a file path
        reader: a reader instance which is based on the class extended from BaseVRSReader
        vrs_indices: a list of absolute indices for the VRS file that you are interested in.
                     If you apply filter, this list should be the one after applying the filter.
        indices: A set of indices or an index for vrs_indices that you want to read.

    Returns:
        VRSReaderSlice if the given indices is a slice, otherwise read a record and returns VRSRecord object.
    """
    if isinstance(indices, int) or hasattr(indices, "__index__"):
        record = reader.read_record(vrs_indices[indices])
        return VRSRecord(record)
    else:
        # A slice or unknown type is passed. Let list handle it directly.
        return VRSReaderSlice(path, reader, vrs_indices[indices])


async def async_index_or_slice_records(
    path: Path,
    reader: "AsyncVRSReader",
    vrs_indices: List[int],
    indices: Union[int, slice],
) -> Union[VRSRecord, AsyncVRSReaderSlice]:
    """Shared logic to index or slice into a AsyncVRSReader or AsyncVRSReaderSlice. Returns either a
    AsyncVRSReaderSlice (if i is a slice or iterable) or a VRSRecord (if i is an integer).

    Args:
        path: a file path
        reader: a reader instance which is based on the class extended from AsyncVRSReader
        vrs_indices: a list of absolute indices for the VRS file that you are interested in.
                     If you apply filter, this list should be the one after applying the filter.
        indices: A set of indices or an index for vrs_indices that you want to read.

    Returns:
        AsyncVRSReaderSlice if the given indices is a slice, otherwise read a record and returns VRSRecord object.
    """
    if isinstance(indices, int) or hasattr(indices, "__index__"):
        record = await reader.async_read_record(vrs_indices[indices])
        return VRSRecord(record)
    else:
        # A slice or unknown type is passed. Let list handle it directly.
        return AsyncVRSReaderSlice(path, reader, vrs_indices[indices])
