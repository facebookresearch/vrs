pyvrs is a Python library that provides read VRS capability to python code with a Pythonic API, using a pybind11 module to handle VRS files via the official VRS C++ API.

# Documentation
[The VRS documentation](https://facebookresearch.github.io/vrs/) explains how
VRS works. It is complemented by the
[API documentation](https://facebookresearch.github.io/vrs/python_api/index.html).

# Installation (macOS and Ubuntu and container)
## Requirements
- Ubuntu or macOS with Python >= 3.7

## Install via pip
```
# Install from pypi
pip install vrs

# Install from Github
pip install 'git+https://github.com/facebookresearch/pyvrs.git'

# Install from a local clone
git clone https://github.com/facebookresearch/pyvrs.git
pip install -e pyvrs
```
