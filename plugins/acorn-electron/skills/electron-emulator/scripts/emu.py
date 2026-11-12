#!/usr/bin/env python3
"""Helper for driving the Acorn Electron emulator from an agent or a script.

Works on macOS, Linux and Windows with nothing but the Python standard library.

    python emu.py check
        find the emulator, and check that it has its ROMs and starts

    python emu.py path
        print where the emulator is

    python emu.py setup PATH
        remember where the emulator is (the program, its folder or the .app),
        for when it is not in one of the usual places

    python emu.py run [--png FILE] [--cwd DIR] [emulator options ...]
        run the emulator headless; --png saves the last picture as a PNG
        (the emulator itself writes BMP). Exit status is the emulator's.
        File names inside a monitor script are relative to the directory this
        is run from. Tapes the machine saves go to --saves, which is set to
        that same directory unless given (never the Documents folder).

    python emu.py test [--png-dir DIR] [FILE.mon | DIR] ...
        run every monitor script (*.mon) headless, each in its own directory,
        and report which pass (exit 0) and which fail. A line in a script
        '#! <options>' adds emulator options, e.g. '#! --sideways-ram 16'.

    python emu.py bmp2png IN.bmp OUT.png
        convert a screenshot

    python emu.py uefcat OUT.uef IN1.uef IN2.uef ...
        join tapes into one, in that order (e.g. a BASIC loader + code)

The emulator is the installed program, found through, in this order:
$ELECTRON_EMU, the path remembered by 'setup', the PATH, the build directory
when working inside the emulator's own source code, and the usual places to
install it (/Applications/Acorn Electron.app, %ProgramFiles%\\acornelectron,
~/.local/bin, ...).
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


EXE = "acornelectron.exe" if os.name == "nt" else "acornelectron"
APP = "Acorn Electron.app"


def settings_folder():
    """The emulator's own folder for settings (and ROMs) on this system."""
    home = os.path.expanduser("~")
    if os.name == "nt":
        base = os.environ.get("APPDATA") or os.path.join(home, "AppData", "Roaming")
    elif sys.platform == "darwin":
        base = os.path.join(home, "Library", "Application Support")
    else:
        base = os.path.join(home, ".local", "share")
    return os.path.join(base, "Emulators", "acornelectron")


REMEMBERED = os.path.join(settings_folder(), "agent-emulator-path.txt")


def program_in(path):
    """The executable for a path that may be the program, its folder or a Mac app."""
    if not path:
        return None
    path = os.path.expanduser(path.strip().strip('"'))
    if os.path.isdir(path):
        if path.rstrip("/\\").endswith(".app"):
            path = os.path.join(path, "Contents", "MacOS", "acornelectron")
        else:
            for c in (os.path.join(path, EXE), os.path.join(path, APP, "Contents", "MacOS", "acornelectron")):
                if os.path.isfile(c):
                    return os.path.abspath(c)
            return None
    return os.path.abspath(path) if os.path.isfile(path) else None


def standard_places():
    home = os.path.expanduser("~")
    if os.name == "nt":
        places = []
        for var in ("ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA"):
            base = os.environ.get(var)
            if not base:
                continue
            if var == "LOCALAPPDATA":
                base = os.path.join(base, "Programs")
            places += [os.path.join(base, "acornelectron"), os.path.join(base, "Acorn Electron")]
        places += [os.path.join(home, "acornelectron"), os.path.join(home, "Downloads", "acornelectron"),
                   os.path.join(home, "Desktop", "acornelectron")]
        return places
    if sys.platform == "darwin":
        return [os.path.join("/Applications", APP), os.path.join(home, "Applications", APP),
                os.path.join(home, "Downloads", "acornelectron", APP),
                os.path.join(home, "Downloads", APP)]
    return [os.path.join(home, ".local", "bin"), "/usr/local/bin", "/opt/acornelectron",
            os.path.join(home, "acornelectron"), os.path.join(home, "Downloads", "acornelectron"),
            os.path.join(home, "Applications", "acornelectron")]


def source_tree_builds():
    """Only when working inside the emulator's own source code."""
    d = os.getcwd()
    while True:
        if os.path.isfile(os.path.join(d, "CMakeLists.txt")) and os.path.isfile(os.path.join(d, "Host", "Shell.cpp")):
            b = os.path.join(d, "build")
            return [b, os.path.join(b, "Release"), os.path.join(b, "Debug"), os.path.join(b, APP)]
        parent = os.path.dirname(d)
        if parent == d:
            return []
        d = parent


def find_emulator(explain=False):
    """Returns (program, where it was found) or (None, None)."""
    found = program_in(os.environ.get("ELECTRON_EMU"))
    if found:
        return found, "$ELECTRON_EMU"
    if os.path.isfile(REMEMBERED):
        with open(REMEMBERED, encoding="utf-8") as f:
            found = program_in(f.read())
        if found:
            return found, "remembered in " + REMEMBERED
    found = shutil.which("acornelectron")
    if found:
        return os.path.abspath(found), "PATH"
    # working on the emulator itself: test what was just built, not what is installed
    for place in source_tree_builds():
        found = program_in(place)
        if found:
            return found, "built from source in " + place
    for place in standard_places():
        found = program_in(place)
        if found:
            return found, "installed in " + place
    return None, None


NOT_FOUND = """the Acorn Electron emulator (acornelectron) was not found.
Looked at $ELECTRON_EMU, a remembered path, the PATH and the usual places:
  macOS:   /Applications/Acorn Electron.app, ~/Applications
  Windows: %ProgramFiles%\\acornelectron, %LOCALAPPDATA%\\Programs\\acornelectron
  Linux:   ~/.local/bin, /usr/local/bin, /opt/acornelectron, ~/acornelectron
If it is somewhere else, remember where with:
  emu.py setup <path to the program, its folder or the .app>"""


def need_emulator():
    e, _ = find_emulator()
    if not e:
        sys.exit(NOT_FOUND)
    return e


def setup(path):
    found = program_in(path)
    if not found:
        print("? no emulator at " + path)
        return 1
    os.makedirs(os.path.dirname(REMEMBERED), exist_ok=True)
    with open(REMEMBERED, "w", encoding="utf-8") as f:
        f.write(found + "\n")
    print("remembered: " + found)
    return check()


def check():
    """Is there an emulator, does it have its ROMs, does it start?"""
    e, where = find_emulator()
    if not e:
        print(NOT_FOUND)
        return 1
    print("emulator: %s (%s)" % (e, where))
    p = subprocess.run([e, "--headless", "--frames", "100"], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, universal_newlines=True)
    if p.returncode != 0:
        print(p.stdout.strip())
        print("? the emulator does not start; if the ROMs are missing, put os.rom and "
              "basic.rom (16 kB each) in " + os.path.join(settings_folder(), "roms"))
        return 1
    print("ROMs found, machine starts: ok")
    return 0


# ---------------------------------------------------------------- BMP -> PNG

def bmp_to_png(src, dst):
    with open(src, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError(src + " is not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    header_size = struct.unpack_from("<I", data, 14)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if bpp not in (24, 32):
        raise ValueError("only 24 and 32 bit BMPs are supported, not %d" % bpp)

    masks = None
    if compression in (3, 6):  # BI_BITFIELDS / BI_ALPHABITFIELDS
        if header_size >= 52:
            masks = struct.unpack_from("<III", data, 54)
        else:
            masks = struct.unpack_from("<III", data, 14 + header_size)

    def shift_of(m):
        s = 0
        while m and not (m >> s) & 1:
            s += 1
        return s

    top_down = height < 0
    height = abs(height)
    stride = ((width * bpp + 31) // 32) * 4
    rows = []
    for y in range(height):
        sy = y if top_down else height - 1 - y
        row = data[offset + sy * stride: offset + sy * stride + width * (bpp // 8)]
        out = bytearray(width * 3)
        if bpp == 24:
            out[0::3] = row[2::3]
            out[1::3] = row[1::3]
            out[2::3] = row[0::3]
        elif masks is None or masks == (0x00FF0000, 0x0000FF00, 0x000000FF):
            out[0::3] = row[2::4]
            out[1::3] = row[1::4]
            out[2::3] = row[0::4]
        else:
            shifts = [shift_of(m) for m in masks]
            for x in range(width):
                p = struct.unpack_from("<I", row, x * 4)[0]
                for c in range(3):
                    out[x * 3 + c] = ((p & masks[c]) >> shifts[c]) & 0xFF
        rows.append(b"\x00" + bytes(out))

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) +
           chunk(b"IEND", b""))
    with open(dst, "wb") as f:
        f.write(png)


# ---------------------------------------------------------------- commands

def run(args):
    png = None
    cwd = None
    rest = []
    i = 0
    while i < len(args):
        if args[i] == "--png" and i + 1 < len(args):
            png = os.path.abspath(args[i + 1]); i += 2
        elif args[i] == "--cwd" and i + 1 < len(args):
            cwd = args[i + 1]; i += 2
        else:
            rest.append(args[i]); i += 1
    return run_emulator(rest, png=png, cwd=cwd)


def run_emulator(options, png=None, cwd=None, capture=False):
    # Always say where files go: relative names in monitor scripts and what the
    # machine saves to tape are taken relative to --saves, and without it the
    # Mac application would use the Documents folder.
    # Paths on the command line are made absolute, so that they mean the same
    # whatever the emulator does with them.
    base = os.path.abspath(cwd or os.getcwd())
    absolute = []
    takes_path = {"--saves", "--roms", "--monitor", "--screenshot"}
    for i, a in enumerate(options):
        before = options[i - 1] if i else ""
        if before in takes_path and a != "-":
            a = os.path.join(base, a)
        elif not a.startswith("-") and before != "--type" and os.path.exists(os.path.join(base, a)):
            a = os.path.join(base, a)  # a tape, snapshot, program or ROM file
        absolute.append(a)
    options = absolute
    if "--saves" not in options:
        options = ["--saves", base] + options
    cmd = [need_emulator(), "--headless"] + options
    bmp = None
    if png:
        fd, bmp = tempfile.mkstemp(suffix=".bmp")
        os.close(fd)
        cmd[2:2] = ["--screenshot", bmp]
    if capture:
        p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           universal_newlines=True)
        code, output = p.returncode, p.stdout
    else:
        code, output = subprocess.call(cmd, cwd=cwd), None
    if bmp:
        try:
            if os.path.getsize(bmp) > 0:
                bmp_to_png(bmp, png)
                if not capture:
                    print("picture: " + png)
        finally:
            os.remove(bmp)
    return (code, output) if capture else code


def options_in(script):
    extra = []
    with open(script, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("#!"):
                extra += line[2:].split()
    return extra


def test(args):
    png_dir = None
    targets = []
    i = 0
    while i < len(args):
        if args[i] == "--png-dir" and i + 1 < len(args):
            png_dir = os.path.abspath(args[i + 1]); i += 2
        else:
            targets.append(args[i]); i += 1
    if not targets:
        targets = ["tests"]

    scripts = []
    for t in targets:
        if os.path.isdir(t):
            for d, _, files in os.walk(t):
                scripts += [os.path.join(d, f) for f in sorted(files) if f.endswith(".mon")]
        elif os.path.isfile(t):
            scripts.append(t)
        else:
            print("? no such file or directory: " + t)
            return 1
    if not scripts:
        print("? no *.mon scripts found in " + ", ".join(targets))
        return 1
    if png_dir:
        os.makedirs(png_dir, exist_ok=True)

    failed = []
    for s in scripts:
        s = os.path.abspath(s)
        name = os.path.splitext(os.path.basename(s))[0]
        png = os.path.join(png_dir, name + ".png") if png_dir else None
        code, output = run_emulator(options_in(s) + ["--monitor", s], png=png,
                                    cwd=os.path.dirname(s), capture=True)
        if code == 0:
            print("PASS  " + s)
        else:
            failed.append(s)
            print("FAIL  " + s + "  (exit %d)" % code)
            for line in output.splitlines():
                if line.startswith("?") or "?" in line[:3]:
                    print("        " + line)
    print("%d passed, %d failed" % (len(scripts) - len(failed), len(failed)))
    return 1 if failed else 0


def uefcat(out, inputs):
    """Join tape images into one tape, files in the given order."""
    parts = []
    for i, name in enumerate(inputs):
        with open(name, "rb") as f:
            data = f.read()
        if data[:2] == b"\x1f\x8b":
            import gzip
            data = gzip.decompress(data)
        if data[:10] != b"UEF File!\x00":
            print("? not a UEF file: " + name)
            return 1
        parts.append(data if i == 0 else data[12:])
    with open(out, "wb") as f:
        f.write(b"".join(parts))
    print("%s: %d files joined" % (out, len(inputs)))
    return 0


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "path":
        e, where = find_emulator()
        if not e:
            print(NOT_FOUND)
            return 1
        print(e)
        print("(" + where + ")")
        return 0
    if cmd == "check":
        return check()
    if cmd == "setup" and len(args) == 1:
        return setup(args[0])
    if cmd == "run":
        return run(args)
    if cmd == "test":
        return test(args)
    if cmd == "bmp2png" and len(args) == 2:
        bmp_to_png(args[0], args[1])
        return 0
    if cmd == "uefcat" and len(args) >= 3:
        return uefcat(args[0], args[1:])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
