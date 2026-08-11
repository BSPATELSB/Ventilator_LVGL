# LifeLine Medical Ventilator Application (LVGL)

This repository contains the full GUI application for the LifeLine Medical Ventilator UI built using C99, LVGL 9, SDL2, and CMake.

## Quick Setup & Automated Installation

To set up this project automatically on a fresh Ubuntu / Debian Linux system (installing all required packages, initializing submodules, configuring CMake, and building the project executable):

1. **Clone the repository**:
   ```bash
   git clone https://github.com/BSPATELSB/Ventilator_LVGL.git
   cd my_ventilator_app
   ```

2. **Run the Project Setup Script**:
   ```bash
   ./project_setup.sh
   # Or alternatively:
   ./setup.sh
   ```

The script will automatically:
- Install all required Linux build tools (`build-essential`, `cmake`, `pkg-config`, `git`, `python3`)
- Install graphic & system libraries (`libsdl2-dev`, `libsdl2-image-dev`, `libcjson-dev`, `libevdev-dev`, `libdrm-dev`, `libgbm-dev`, `libinput-dev`, `libfreetype6-dev`, etc.)
- Initialize and update Git submodules (`lvgl`)
- Create the build folder and configure CMake
- Compile the project binary `ventilator_app`

## Running the Application

After running `./project_setup.sh`, execute the application with:
```bash
./build/bin/ventilator_app
```

or via CMake target:
```bash
cmake --build build --target run
```
