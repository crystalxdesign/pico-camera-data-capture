# PICO-Cam-A shape capture: Pico SDK + USB CDC + Python

**Hardware integration required:** This archive contains the complete capture protocol,
32x32 area-downsampling, Pico SDK build setup, and host collector. It deliberately does
NOT claim a working HM01B0 hardware driver. The exact Waveshare PICO-Cam-A camera GPIO
mapping and vendor camera API were not verifiable from accessible text. The adapter
`src/camera_adapter.c` is a clearly marked fail-closed integration point. Until replaced,
the firmware builds but replies `0` to status queries and cannot capture images.

## Hardware integration
1. Download the **PICO-Cam-A Demo** and schematic from https://www.waveshare.com/wiki/PICO-Cam-A.
2. Find its HM01B0 camera init and frame acquisition routines and its actual camera
   capture resolution. Add the required vendor source files to CMakeLists.txt and link
   their dependencies (possibly hardware_pio, hardware_dma, hardware_i2c, hardware_pwm).
3. Set `CAMERA_W` and `CAMERA_H` in `src/camera_adapter.h` to match the vendor frame.
4. Implement `camera_init()` and `camera_capture(dst)` in `src/camera_adapter.c`.
   The latter must fill the supplied buffer with 8-bit grayscale pixels in row-major
   order, width*height bytes, with no line padding. If vendor output has padding,
   strip it inside the adapter. Do not let the vendor driver write beyond the buffer.
5. If vendor code captures asynchronously, block until the frame is complete before
   returning true. If using DMA, ensure buffers satisfy its alignment requirements.

## Build
Install the Raspberry Pi Pico C/C++ SDK and export `PICO_SDK_PATH`.
The SDK's `pico_sdk_import.cmake` should be copied into this project directory
from `${PICO_SDK_PATH}/external/pico_sdk_import.cmake` if absent.

```
cp "$PICO_SDK_PATH/external/pico_sdk_import.cmake" .
cmake -S . -B build -DPICO_BOARD=pico
cmake --build build -j4
```

Hold BOOTSEL, plug in the board, and copy `build/shape_capture.uf2` to RPI-RP2.
USB CDC will appear as `/dev/ttyACM0` or another serial port. The baud setting
is ignored for native USB CDC. This firmware intentionally does not use the LCD.

## Collect images
```
python3 -m venv .venv
source .venv/bin/activate
pip install -r pc/requirements.txt
python pc/capture.py --port /dev/ttyACM0 --label circle --count 100
python pc/capture.py --port /dev/ttyACM0 --label square --count 100
python pc/capture.py --port /dev/ttyACM0 --label triangle --count 100
```
For validation, pass `--out dataset/validation`. Vary size, angle, distance,
lighting, position, and background. Maintain the same preprocessing when training.

## USB protocol
Host `I`: camera ready response `1` or `0`.
Host `C`: one capture. Error: single ASCII `E`.
Success: `AA 55 00 04` + 1024 grayscale bytes + XOR of all pixel bytes.
Do not enable printf debug messages on USB stdout: they corrupt the binary stream.

## Test scope
Host script has syntax been checked. Firmware has not been compiled against an
installed Pico SDK or tested on a PICO-Cam-A. Camera driver integration is pending.
