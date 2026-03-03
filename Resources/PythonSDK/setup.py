import re
from pathlib import Path

from setuptools import setup, find_packages

# Single source of truth: snap/__init__.py
_version = re.search(
    r'^__version__\s*=\s*["\']([^"\']+)["\']',
    Path(__file__).parent.joinpath("snap", "__init__.py").read_text(),
    re.M,
).group(1)

setup(
    name="snap-neuro",
    version=_version,
    description="Python SDK for SNAP (Spike Neuro Acquisition Platform) remote control",
    author="Spike Neuro",
    author_email="support@spikeneuro.com",
    url="https://github.com/spikeneuro/snap",
    packages=find_packages(),
    install_requires=["requests>=2.20.0"],
    python_requires=">=3.9",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Topic :: Scientific/Engineering :: Bio-Informatics",
    ],
)
