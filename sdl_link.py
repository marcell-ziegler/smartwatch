"""
extra_scripts = pre:sdl_link.py  (native env only)

build_flags already runs `pkg-config --cflags sdl2` for the compiler; this
adds the matching `--libs` to the linker so `program` actually links against
libSDL2.
"""
import subprocess

Import("env")

libs = subprocess.check_output(["pkg-config", "--libs", "sdl2"]).decode().split()
env.Append(LINKFLAGS=libs)
