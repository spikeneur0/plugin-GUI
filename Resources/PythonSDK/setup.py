from setuptools import setup, find_packages

setup(
    name="snap-neuro",
    version="0.1.0",
    description="Python SDK for SNAP (Spike Neuro Acquisition Platform) remote control",
    author="Spike Neuro",
    author_email="support@spikeneuro.com",
    url="https://github.com/spikeneuro/snap",
    packages=find_packages(),
    install_requires=["requests>=2.20.0"],
    python_requires=">=3.7",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Topic :: Scientific/Engineering :: Bio-Informatics",
    ],
)
