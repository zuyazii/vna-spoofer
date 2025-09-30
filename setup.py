"""
Setup script for LibreVNA CLI

This script configures the installation of the LibreVNA CLI package.
It defines package metadata, dependencies, and entry points for the
command-line interface.

Usage:
    pip install .                    # Install in development mode
    pip install -e .                 # Install in editable mode
    python setup.py build            # Build package
    python setup.py install          # Install package
"""

from setuptools import setup, find_packages
import os

# Read README file for long description
def read_readme():
    readme_path = os.path.join(os.path.dirname(__file__), 'README.md')
    if os.path.exists(readme_path):
        with open(readme_path, 'r', encoding='utf-8') as f:
            return f.read()
    return "LibreVNA CLI - Command-line interface for LibreVNA Vector Network Analyzer"

# Read requirements
def read_requirements():
    requirements_path = os.path.join(os.path.dirname(__file__), 'requirements.txt')
    if os.path.exists(requirements_path):
        with open(requirements_path, 'r', encoding='utf-8') as f:
            return [line.strip() for line in f if line.strip() and not line.startswith('#')]
    return []

setup(
    name="librevna-cli",
    version="0.1.0",
    author="VNA-CLI Development Team",
    author_email="",
    description="Command-line interface for LibreVNA Vector Network Analyzer",
    long_description=read_readme(),
    long_description_content_type="text/markdown",
    url="https://github.com/your-username/librevna-cli",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Science/Research",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Scientific/Engineering",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    python_requires=">=3.8",
    install_requires=read_requirements(),
    extras_require={
        "dev": [
            "pytest>=6.2.0",
            "pytest-cov>=2.12.0",
            "black>=21.0.0",
            "flake8>=3.9.0",
            "mypy>=0.910",
            "pre-commit>=2.15.0",
        ],
    },
    entry_points={
        "console_scripts": [
            "librevna=cli:cli",
            "vna-cli=cli:cli",
        ],
    },
    include_package_data=True,
    zip_safe=False,
    keywords="librevna vna vector network analyzer rf microwave usb tcp",
    project_urls={
        "Bug Reports": "https://github.com/your-username/librevna-cli/issues",
        "Source": "https://github.com/your-username/librevna-cli",
        "Documentation": "https://github.com/your-username/librevna-cli/blob/main/README.md",
    },
)
