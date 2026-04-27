"""Post-build hook for [env:simulator].

Wokwi's browser editor wants a single .bin (bootloader + partition table +
application). PlatformIO emits them as separate files. This script runs after
the linker and produces .pio/build/simulator/feedme-merged.bin so the iteration
loop is `pio run -e simulator` -> upload merged file -> done.
"""

import os
import subprocess

Import("env")  # type: ignore  # noqa: F821 - injected by PlatformIO


def merge_after_build(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    esptool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
    esptool = os.path.join(esptool_dir, "esptool.py")
    python = env.subst("$PYTHONEXE")
    out = os.path.join(build_dir, "feedme-merged.bin")
    cmd = [
        python, esptool,
        "--chip", "esp32s3",
        "merge_bin",
        "-o", out,
        "--flash_mode", "dio",
        # Wokwi's S3 emulation is reliable at 40m. 80m boots fine on the real
        # board (esp32-s3-lcd-1_28 env handles its own merge if ever needed)
        # but loops here.
        "--flash_freq", "40m",
        "--flash_size", "16MB",
        "0x0",     os.path.join(build_dir, "bootloader.bin"),
        "0x8000",  os.path.join(build_dir, "partitions.bin"),
        "0x10000", os.path.join(build_dir, "firmware.bin"),
    ]
    print(f"[merge] -> {out}")
    subprocess.run(cmd, check=True)


env.AddPostAction(  # type: ignore  # noqa: F821
    "$BUILD_DIR/${PROGNAME}.bin", merge_after_build
)
