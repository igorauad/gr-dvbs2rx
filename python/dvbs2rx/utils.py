import platform
import subprocess


def parse_version(dvbs2rx):
    is_macos = platform.system() == "Darwin"
    cpython_lib = dvbs2rx.dvbs2rx_python.__file__
    if is_macos:
        libs = subprocess.check_output(['otool', '-L', cpython_lib]).decode()
    else:
        libs = subprocess.check_output(['ldd', cpython_lib]).decode()
    dvbs2rx_lib_version = "Not Found"
    for line in libs.splitlines():
        if "libgnuradio-dvbs2rx" in line:
            dvbs2rx_lib_version = line.split()[-1][:-1] if is_macos else \
                line.split()[0].replace("libgnuradio-dvbs2rx.so.", "")
    return dvbs2rx_lib_version
