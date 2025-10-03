# LibreVNA Headless CLI

This repository couples a small Python wrapper with the native librevna-cli executable so you can run calibrated LibreVNA sweeps from the command line and capture the JSON/CSV results.

## Architecture

- cpp/ builds the native librevna-cli binary (CMake + libusb).
- cli.py exposes a Click-based command that shells out to the binary, collects the JSON payload, and stores JSON/CSV traces.
- src/librevna/device/headless_cli.py contains the tiny Python adapter used by the CLI. There are no other Python-side measurements or calibration routines.

## Requirements

- Python 3.10+
- Click (pip install click)
- A built librevna-cli binary with libusb support
- Windows 10+

## Building the Native Binary

```powershell
cmake -S cpp -B cpp/build \
      -DLIBUSB_INCLUDE_DIR=C:/Users/<you>/vcpkg/installed/x64-windows/include \
      -DLIBUSB_LIBRARY=C:/Users/<you>/vcpkg/installed/x64-windows/lib/libusb-1.0.lib
cmake --build cpp/build --config Release
copy C:/Users/<you>/vcpkg/installed/x64-windows/bin/libusb-1.0.dll cpp/build/Release
```

## Install Python dependencies

```powershell
python -m venv .venv
.\.venv\Scripts\activate
pip install -r requirements.txt
```

## Running a Sweep

```powershell
python cli.py headless-sweep \
  --cal calibration\calibration.cal \
  --start-freq 2.99 \
  --stop-freq 4.0 \
  --points 500 \
  --ifbw 1000 \
  --power -10
```

Frequencies are specified in GHz; the wrapper converts them to Hz before calling librevna-cli. The command writes JSON and CSV outputs under `output/headless_sweep_result_<timestamp>/`.

List connected devices at any time with:

```powershell
python cli.py --list
```

## Environment Variable Override

Set `LIBREVNA_CLI_BIN` to point at a custom build if the wrapper cannot locate
cpp/build/Release/librevna-cli.exe automatically.
