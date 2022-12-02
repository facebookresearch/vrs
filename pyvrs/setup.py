#!/usr/bin/env python
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

import os
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext

ROOT_DIR = Path(__file__).parent.parent.resolve()
if os.path.basename(ROOT_DIR) != "vrs":
    # Check if it is FB repo
    FB_ROOT_PATH = os.path.abspath(os.path.join(ROOT_DIR, "../libraries/vrs"))
    if os.path.exists(FB_ROOT_PATH):
        ROOT_DIR = FB_ROOT_PATH
    else:
        PYPI_ROOT_PATH = os.path.abspath(os.path.join(ROOT_DIR, ".."))
        if os.path.exists(PYPI_ROOT_PATH):
            ROOT_DIR = PYPI_ROOT_PATH


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def run(self):
        try:
            subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError("CMake is not available.")
        super().run()

    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        if "DEBUG" in os.environ:
            cfg = "Debug" if os.environ["DEBUG"] == "1" else "Release"
        else:
            cfg = "Debug" if self.debug else "Release"

        cmake_args = [
            f"-DCMAKE_BUILD_TYPE={cfg}",
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            "-GCodeBlocks",
        ]
        build_args = ["--target", os.path.basename(ext.name)]

        # Default to Ninja
        if "CMAKE_GENERATOR" not in os.environ:
            cmake_args += ["-GNinja"]

        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        subprocess.check_call(
            ["cmake", ext.sourcedir] + cmake_args, cwd=self.build_temp
        )
        subprocess.check_call(
            ["cmake", "--build", "."] + build_args, cwd=self.build_temp
        )


def main():
    cwd = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(cwd, "README.md"), encoding="utf-8") as f:
        long_description = f.read()

    setup(
        name="pyvrs",
        version="1.0.0",
        description="Python API for VRS",
        long_description=long_description,
        long_description_content_type="text/markdown",
        url="https://github.com/facebookresearch/vrs",
        author="Meta Reality Labs Research",
        license="Apache-2.0",
        install_requires=["numpy", "typing", "dataclasses"],
        python_requires=">=3.7",
        packages=find_packages(),
        zip_safe=False,
        ext_modules=[CMakeExtension("pyvrs/vrsbindings", sourcedir=ROOT_DIR)],
        cmdclass={
            "build_ext": CMakeBuild,
        },
    )


if __name__ == "__main__":
    main()
