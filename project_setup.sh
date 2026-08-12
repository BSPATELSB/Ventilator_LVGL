#!/usr/bin/env bash
# ==============================================================================
# Project Setup Script for Medical Ventilator LVGL App
# ==============================================================================
# This script sets up all required dependencies, packages, git submodules,
# and builds the project on a fresh Ubuntu / Debian Linux environment.
# ==============================================================================

set -e

# ANSI Color Codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=====================================================${NC}"
echo -e "${BLUE}  Starting Ventilator App Automated Project Setup   ${NC}"
echo -e "${BLUE}=====================================================${NC}"

# Root / Sudo privilege check
SUDO=""
if [ "$EUID" -ne 0 ]; then
    if command -v sudo &> /dev/null; then
        SUDO="sudo"
    else
        echo -e "${RED}Error: This script requires root privileges or sudo to install system packages.${NC}"
        exit 1
    fi
fi

# 1. Package Installation
echo -e "\n${YELLOW}[1/4] Checking and installing system packages...${NC}"

PACKAGES=(
    build-essential
    cmake
    pkg-config
    git
    python3
    libsdl2-dev
    libsdl2-image-dev
    libcjson-dev
    libevdev-dev
    libdrm-dev
    libgbm-dev
    libinput-dev
    libfreetype6-dev
    wayland-protocols
    libwayland-dev
    libxkbcommon-dev
    libx11-dev
    alsa-utils
    libasound2-dev
)

if command -v apt-get &> /dev/null; then
    echo "Updating package repositories..."
    $SUDO apt-get update -y || true

    echo "Installing required development packages..."
    $SUDO apt-get install -y "${PACKAGES[@]}"
else
    echo -e "${YELLOW}Notice: apt-get not found. Assuming build packages are managed externally.${NC}"
fi

# 2. Git Submodules Setup
echo -e "\n${YELLOW}[2/4] Initializing and updating Git submodules (LVGL)...${NC}"
if [ -d ".git" ]; then
    git submodule update --init --recursive
    echo -e "${GREEN}Git submodules initialized successfully.${NC}"
else
    echo -e "${YELLOW}Notice: .git folder not present, skipping submodule update.${NC}"
fi

#Setup Python Environment
sudo apt install python3.10-venv

# 3. Create Build Directory & Configure CMake
echo -e "\n${YELLOW}[3/4] Configuring project with CMake...${NC}"
mkdir -p build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 4. Build Executable
echo -e "\n${YELLOW}[4/4] Compiling project...${NC}"
NPROC=$(nproc 2>/dev/null || echo 2)
cmake --build build -j"$NPROC"

# Verification
if [ -f "build/bin/ventilator_app" ]; then
    echo -e "\n${GREEN}=====================================================${NC}"
    echo -e "${GREEN}  Project setup & build completed successfully!    ${NC}"
    echo -e "${GREEN}=====================================================${NC}"
    echo -e "You can launch the application with:"
    echo -e "  ${BLUE}./build/bin/ventilator_app${NC}"
    echo -e "or using CMake:"
    echo -e "  ${BLUE}cmake --build build --target run${NC}"
else
    echo -e "\n${RED}Error: ventilator_app binary was not created.${NC}"
    exit 1
fi
