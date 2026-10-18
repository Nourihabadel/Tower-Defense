"""Download MinGW, Ninja and CMake and add them to PATH (Windows).

Run `python setup_tools.py` and answer the prompts. Answer "n" to the download
question to point the script at tools you already have (e.g. offline machines).
"""
import ctypes
import hashlib
import pathlib
import urllib.request
import winreg
import zipfile

# name -> (download url, sha256 of the zip, executable used to locate the bin folder)
# When changing a URL, update its sha256 too (CMake and WinLibs publish them next to the zip).
TOOLS = {
    "mingw": ("https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-19.1.1-12.0.0-ucrt-r2/"
              "winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r2.zip",
              "d41933cef13113018418d7b596319c2ed59a567395bbb87afe27a171e111d553", "gcc.exe"),
    "ninja": ("https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip",
              "f550fec705b6d6ff58f2db3c374c2277a37691678d6aba463adcbb129108467a", "ninja.exe"),
    "cmake": ("https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-windows-x86_64.zip",
              "d163cd3ab4959b0a53fa8988f2ddbd2e6c501658201e6a154386bad9dbe4f836", "cmake.exe"),
}

USER_ENV = (winreg.HKEY_CURRENT_USER, r"Environment")
SYSTEM_ENV = (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment")


def progress(name, done, total, width=30):
    mb = done / 1e6
    if total > 0:
        frac = min(done / total, 1)
        bar = "#" * int(frac * width)
        print(f"\rDownloading {name} [{bar:<{width}}] {frac:4.0%} {mb:.1f}/{total / 1e6:.1f} MB", end="", flush=True)
    else:  # server didn't send a size
        print(f"\rDownloading {name} {mb:.1f} MB", end="", flush=True)


def download(name, url, sha256, dest):
    target = dest / name
    archive = dest / f"{name}.zip"
    urllib.request.urlretrieve(url, archive, lambda n, size, total: progress(name, n * size, total))
    print()
    if hashlib.sha256(archive.read_bytes()).hexdigest() != sha256:
        archive.unlink()
        raise SystemExit(f"Checksum mismatch for {name}: the download is corrupted or not the expected file.")
    print(f"Extracting {name} ...")
    with zipfile.ZipFile(archive) as z:
        z.extractall(target)
    archive.unlink()


def find_bin(root, exe):
    matches = sorted(root.rglob(exe), key=lambda p: len(p.parts))  # shallowest match wins
    if not matches:
        raise SystemExit(f"Could not find {exe} under {root}")
    return str(matches[0].parent.resolve())


def add_to_path(dirs, scope):
    hive, key = SYSTEM_ENV if scope == "system" else USER_ENV
    try:
        with winreg.OpenKey(hive, key, 0, winreg.KEY_READ | winreg.KEY_WRITE) as k:
            try:
                current, kind = winreg.QueryValueEx(k, "Path")
            except FileNotFoundError:
                current, kind = "", winreg.REG_EXPAND_SZ
            entries = [p for p in current.split(";") if p]
            new = [d for d in dirs if d.lower() not in (p.lower() for p in entries)]
            winreg.SetValueEx(k, "Path", 0, kind, ";".join(entries + new))
    except PermissionError:
        raise SystemExit("Permission denied: run as Administrator to modify the system PATH.")
    for d in dirs:
        print(("Added   " if d in new else "Present ") + d)
    # Tell running programs (Explorer, new terminals) that the environment changed.
    ctypes.windll.user32.SendMessageTimeoutW(0xFFFF, 0x1A, 0, "Environment", 2, 5000, None)


def ask(question, choices=None, default=None):
    """Prompt until a valid answer is given. Empty input returns the default."""
    if choices:
        question += f" ({'/'.join(choices)})"
    if default:
        question += f" [{default}]"
    while True:
        answer = input(question + ": ").strip().strip('"') or default
        if answer and (choices is None or answer.lower() in choices):
            return answer.lower() if choices else answer


def main():
    if ask("Download the tools?", ["y", "n"], "y") == "y":
        root = pathlib.Path(ask("Install directory", default=str(pathlib.Path.home() / "devtools")))
        root.mkdir(parents=True, exist_ok=True)
        for name, (url, sha256, _) in TOOLS.items():
            download(name, url, sha256, root)
    else:
        root = pathlib.Path(ask("Directory that already contains the tools"))

    dirs = [find_bin(root, exe) for _, _, exe in TOOLS.values()]
    add_to_path(dirs, ask("Add to which PATH?", ["user", "system"], "user"))
    print("Done. Open a new terminal to use the tools.")


if __name__ == "__main__":
    main()