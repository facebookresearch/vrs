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

from functools import partial
from typing import Callable, Sequence, TypeVar, Union

import numpy as np

from . import VRSRecord as _VRSRecord

from .utils import stringify_metadata_keys

T = TypeVar("T")


class VRSRecord:
    """Represents a single VRS Record."""

    def __init__(self, record: _VRSRecord) -> None:
        self._record = record

    def __repr__(self) -> str:
        return self._record.__repr__()

    def __str__(self) -> str:
        return self._record.__str__()

    def __getitem__(self, key: str):
        return self._record[key]

    @property
    def format_version(self) -> int:
        """The format version of this VRS record."""
        return self._record.format_version

    @property
    def n_audio_blocks(self) -> int:
        """The number of audio blocks present in this VRS record."""
        return self._record.n_audio_blocks

    @property
    def n_custom_blocks(self) -> int:
        """The number of custom blocks present in this VRS record."""
        return self._record.n_custom_blocks

    @property
    def n_image_blocks(self) -> int:
        """The number of image blocks present in this VRS record."""
        return self._record.n_image_blocks

    @property
    def n_metadata_blocks(self) -> int:
        """The number of metadata blocks present in this VRS record."""
        return self._record.n_metadata_blocks

    @property
    def n_blocks_in_total(self) -> int:
        """The total number of blocks on this record."""
        return self._record.n_blocks_in_total

    @property
    def record_type(self) -> str:
        """The type of this VRS record, e.g. 'configuration' or 'data'."""
        return self._record.record_type

    @property
    def record_index(self) -> int:
        """The absolute index of this VRS record in the VRS file."""
        return self._record.record_index

    @property
    def stream_id(self) -> str:
        """A unique identifier for the type of device that recorded this record, along with an
        enumeration. E.g.: '1001-1'.
        """
        return self._record.stream_id

    @property
    def timestamp(self) -> float:
        """The timestamp associated with this record."""
        return self._record.timestamp

    @property
    def audio_blocks(self) -> "VRSBlocks":
        """The list of audio blocks associated with this record."""
        return VRSBlocks(
            partial(lambda x, idx: np.asarray(x[idx]), self._record.audio_blocks),
            range(self.n_audio_blocks),
        )

    @property
    def audio_specs(self) -> "VRSBlocks":
        """The list of audio block specs associated with this record."""
        return VRSBlocks(
            partial(lambda x, idx: x[idx], self._record.audio_specs),
            range(self.n_audio_blocks),
        )

    @property
    def custom_blocks(self) -> "VRSBlocks":
        """The list of custom blocks associated with this record."""
        return VRSBlocks(
            partial(lambda x, idx: np.asarray(x[idx]), self._record.custom_blocks),
            range(self.n_custom_blocks),
        )

    @property
    def custom_block_specs(self) -> "VRSBlocks":
        """The list of custom block specs associated with this record."""
        return VRSBlocks(
            partial(lambda x, idx: x[idx], self._record.custom_block_specs),
            range(self.n_custom_blocks),
        )

    @property
    def image_blocks(self) -> "VRSBlocks":
        """The list of image blocks associated with this record."""
        return VRSBlocks(
            partial(lambda x, idx: np.asarray(x[idx]), self._record.image_blocks),
            range(self.n_image_blocks),
        )

    @property
    def image_specs(self) -> "VRSBlocks":
        """The list of image block specs associated with this record."""
        return VRSBlocks(
            partial(lambda x, idx: x[idx], self._record.image_specs),
            range(self.n_image_blocks),
        )

    @property
    def metadata_blocks(self) -> "VRSBlocks":
        """The list of metadata blocks associated with this record."""
        return VRSBlocks(
            partial(
                lambda x, idx: stringify_metadata_keys(x[idx]),
                self._record.metadata_blocks,
            ),
            range(self.n_metadata_blocks),
        )


class VRSBlocks(Sequence):
    """A representation of a list of Record blocks (image, metadata, audio or custom).
    Accessing elements returns the type directly.
    """

    def __init__(self, get_func: Callable[[int], T], range_: range) -> None:
        self._range = range_
        self._get_func = get_func

    def __getitem__(self, i: Union[int, slice]) -> Union[T, "VRSBlocks"]:
        if isinstance(i, int) or hasattr(i, "__index__"):
            return self._get_func(self._range[i])
        else:
            # A slice or unknown type is passed. Let list handle it directly.
            return VRSBlocks(self._get_func, self._range[i])

    def __len__(self) -> int:
        return len(self._range)
