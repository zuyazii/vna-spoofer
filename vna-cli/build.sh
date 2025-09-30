#!/bin/bash
# Build script for vna-cli
# This script builds the C++ VNA CLI tool

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== VNA-CLI Build Script ===${NC}"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found${NC}"
    echo "Please run this script from the vna-cli directory"
    exit 1
fi

# Check for required tools
echo "Checking build dependencies..."

# Check CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake not found${NC}"
    echo "Please install CMake >= 3.16"
    exit 1
fi

# Check for Qt5
if ! command -v qmake &> /dev/null && ! pkg-config --exists Qt5Core; then
    echo -e "${YELLOW}Warning: Qt5 may not be installed${NC}"
    echo "Install Qt5: sudo apt-get install qtbase5-dev (Ubuntu/Debian)"
fi

# Check for libusb
if ! pkg-config --exists libusb-1.0; then
    echo -e "${YELLOW}Warning: libusb-1.0 may not be installed${NC}"
    echo "Install libusb: sudo apt-get install libusb-1.0-0-dev (Ubuntu/Debian)"
fi

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false
INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --install)
            INSTALL=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--debug] [--clean] [--install]"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure
echo -e "${GREEN}Configuring build ($BUILD_TYPE)...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Check if LibreVNA source is available
if [ ! -d "../third_party/librevna" ]; then
    echo ""
    echo -e "${YELLOW}=== WARNING ===${NC}"
    echo "LibreVNA source code not found in third_party/librevna"
    echo ""
    echo "This build will fail at the linking stage because the LibreVNA"
    echo "C++ driver code is not yet integrated."
    echo ""
    echo "To complete the implementation, you need to:"
    echo "  1. Add LibreVNA as a git submodule:"
    echo "     cd third_party"
    echo "     git submodule add https://github.com/jankae/LibreVNA.git librevna"
    echo ""
    echo "  2. Update CMakeLists.txt to include LibreVNA source files"
    echo ""
    echo "  3. Rebuild the project"
    echo ""
    echo "See docs/implementation-plan.md for detailed instructions."
    echo ""
    echo -e "${YELLOW}Continuing with build (will fail at link stage)...${NC}"
    echo ""
fi

# Build
echo -e "${GREEN}Building...${NC}"
if cmake --build . --config $BUILD_TYPE -j$(nproc 2>/dev/null || echo 4); then
    echo ""
    echo -e "${GREEN}=== Build Successful ===${NC}"
    echo "Executable: build/bin/vna-cli"
    
    # Install if requested
    if [ "$INSTALL" = true ]; then
        echo ""
        echo -e "${GREEN}Installing...${NC}"
        sudo cmake --install .
        echo "Installed to: /usr/local/bin/vna-cli"
    fi
    
    exit 0
else
    echo ""
    echo -e "${RED}=== Build Failed ===${NC}"
    echo ""
    echo "Common issues:"
    echo "  1. Missing dependencies (Qt5, libusb-1.0)"
    echo "  2. LibreVNA source not integrated (see warning above)"
    echo "  3. Compiler errors"
    echo ""
    echo "Check the error messages above for details."
    exit 1
fi
