"""Memory-only Superstars unlock with an isolated copy of the normal save."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "patches/sonic-superstars-temporary-unlock.json"
SESSION = ROOT / "build/src/launcher/_SuperstarsUnlockSession"
NORMAL = ROOT / "build/src/launcher/_SaveData/PPSA06888"

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def hashes(folder):
    return {str(p.relative_to(folder)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in folder.rglob("*") if p.is_file()}

def validate(game, plan):
    param = json.loads((game / "sce_sys/param.json").read_bytes())
    require(param.get("titleId") == plan["title_id"] and
            param.get("contentVersion") == plan["game_version"], "Wrong game/version")
    raw = (game / plan["module"]).read_bytes()
    require(hashlib.sha256(raw).hexdigest() == plan["module_sha256"], "Wrong module SHA256")
    require(raw[:4] == bytes.fromhex("4f153d1d"), "Unsupported SELF")
    count = struct.unpack_from("<H", raw, 24)[0]
    elf = 32 + count * 32
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", raw, elf)
    ph = [struct.unpack_from("<IIQQQQQQ", raw, elf + header[5] + i * header[9])
          for i in range(header[10])]
    for patch in plan["patches"]:
        va = int(patch["offset"], 16)
        expected = bytes.fromhex(patch["expected"])
        require(0 < len(bytes.fromhex(patch["replacement"])) <= len(expected), "Bad patch size")
        found = False
        for i in range(count):
            flags, offset, size, unpacked = struct.unpack_from("<QQQQ", raw, 32 + i * 32)
            if not flags & 0x800:
                continue
            p = ph[(flags >> 20) & 0xfff]
            if p[0] == 1 and p[1] & 1 and p[3] <= va and va + len(expected) <= p[3] + p[5]:
                require(size == unpacked == p[5], "Compressed code unsupported")
                pos = offset + va - p[3]
                require(raw[pos:pos + len(expected)] == expected, "Wrong bytes: " + patch["name"])
                found = True
                break
        require(found, "Address outside executable code")

class Memory:
    def __init__(self, pid):
        self.k = c.WinDLL("kernel32", use_last_error=True)
        self.n = c.WinDLL("ntdll", use_last_error=True)
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.CloseHandle.argtypes = [w.HANDLE]
        self.k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t,
                                            c.POINTER(c.c_size_t)]
        self.k.WriteProcessMemory.argtypes = self.k.ReadProcessMemory.argtypes
        self.k.VirtualProtectEx.argtypes = [w.HANDLE, c.c_void_p, c.c_size_t, w.DWORD, c.POINTER(w.DWORD)]
        self.k.FlushInstructionCache.argtypes = [w.HANDLE, c.c_void_p, c.c_size_t]
        self.n.NtSuspendProcess.argtypes = [w.HANDLE]
        self.n.NtResumeProcess.argtypes = [w.HANDLE]
        self.h = self.k.OpenProcess(0x838, False, pid)
        require(self.h, "Cannot access own test process")

    def read(self, address, size):
        buf = c.create_string_buffer(size)
        done = c.c_size_t()
        require(self.k.ReadProcessMemory(self.h, address, buf, size, c.byref(done)) and
                done.value == size, "Memory read failed")
        return buf.raw

    def write(self, address, value):
        old = w.DWORD()
        require(self.k.VirtualProtectEx(self.h, address, len(value), 0x40, c.byref(old)),
                "Memory protection change failed")
        try:
            done = c.c_size_t()
            buf = c.create_string_buffer(value)
            require(self.k.WriteProcessMemory(self.h, address, buf, len(value), c.byref(done)) and
                    done.value == len(value), "Memory write failed")
            require(self.k.FlushInstructionCache(self.h, address, len(value)), "Cache flush failed")
            require(self.read(address, len(value)) == value, "Patch readback failed")
        finally:
            ignored = w.DWORD()
            require(self.k.VirtualProtectEx(self.h, address, len(value), old.value, c.byref(ignored)),
                    "Memory protection restoration failed")

    def apply(self, base, plan):
        require(self.n.NtSuspendProcess(self.h) == 0, "Could not pause test process")
        written = []
        try:
            # All guards first: some guards overlap, actual writes never overlap.
            for p in plan["patches"]:
                expected = bytes.fromhex(p["expected"])
                require(self.read(base + int(p["offset"], 16), len(expected)) == expected,
                        "Runtime mismatch: " + p["name"])
            try:
                for p in plan["patches"]:
                    address = base + int(p["offset"], 16)
                    value = bytes.fromhex(p["replacement"])
                    written.append((address, self.read(address, len(value))))
                    self.write(address, value)
            except Exception:
                for address, value in reversed(written):
                    self.write(address, value)
                raise
        finally:
            require(self.n.NtResumeProcess(self.h) == 0, "Could not resume test process")


def wait_for_module(process, trace, module):
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        require(process.poll() is None, "Game exited before patch")
        if trace.exists():
            lines = trace.read_bytes().replace(bytes([0]), b"").decode(errors="replace").splitlines()
            for i, line in enumerate(lines):
                if line.startswith("Loading: ") and line.replace(chr(92), "/").endswith(module):
                    for detail in lines[i + 1:i + 30]:
                        if detail.startswith("base_vaddr"):
                            return int(detail.split("=", 1)[1].strip(), 16)
        time.sleep(0.1)
    raise RuntimeError("Timed out waiting for game module")



def build_command(emulator, game, trace, graphics_profile):
    command = [str(emulator), "--game", str(game / "eboot.bin"),
               "--printf-direction", "File", "--printf-output-file", str(trace)]
    if graphics_profile == "fsr4k":
        # This title already presents a 1080p framebuffer. Request the same size
        # from the engine; FSR's measured source, not this request, proves it.
        command += ["--guest-render-width", "1920", "--guest-render-height", "1080",
                    "--upscaler-method", "Fsr1", "--upscaler-quality", "Performance",
                    "--upscaler-sharpness", "0.3",
                    "--fsr-output-width", "3840", "--fsr-output-height", "2160",
                    "--present-mode", "Fifo"]
    elif graphics_profile == "native":
        command += ["--upscaler-method", "Off"]
    else:
        raise ValueError("Unknown graphics profile: " + graphics_profile)
    for binding in ["Cross=J", "Circle=K", "Options=Return", "Up=Up", "Down=Down", "Left=Left", "Right=Right"]:
        command += ["--keymap", binding]
    return command

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True, help="Root of your legally dumped game")
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/src/launcher/kyty_emulator.exe")
    parser.add_argument("--save-dir", type=Path, default=NORMAL, help="Normal PPSA06888 save directory")
    parser.add_argument("--session-dir", type=Path, default=SESSION, help="Separate test-session directory")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--graphics-profile", choices=["fsr4k", "native"], default="fsr4k",
                        help="fsr4k: 1080p input request, 4K FSR output (default); native: FSR off")
    args = parser.parse_args()
    require(os.name == "nt", "Windows required")
    args.game = args.game.resolve()
    normal = args.save_dir.resolve()
    session = args.session_dir.resolve()
    require(normal != session and normal not in session.parents and session not in normal.parents,
            "Save and session directories must be separate")
    plan = json.loads(PLAN.read_text())
    validate(args.game, plan)
    if args.check:
        print("Version, module SHA256, code ranges and original instructions verified")
        return 0
    tasks = subprocess.run(["tasklist", "/FI", "IMAGENAME eq kyty_emulator.exe", "/FO", "CSV", "/NH"],
                           capture_output=True, text=True, check=True)
    require("kyty_emulator.exe" not in tasks.stdout.lower(), "Close the running game first")
    require(normal.is_dir(), "Normal save not found")
    before = hashes(normal)
    session.mkdir(parents=True, exist_ok=True)
    backup = session / "OriginalSaveBackup/PPSA06888"
    copied = session / "_SaveData/PPSA06888"
    if not backup.exists():
        shutil.copytree(normal, backup)
        require(hashes(backup) == before, "Backup verification failed")
        (session / "original-save-hashes.json").write_text(json.dumps(before, indent=2))
    if not copied.exists():
        shutil.copytree(backup, copied)
    emulator = args.emulator.resolve()
    require(emulator.is_file(), "Emulator not found")
    trace = session / "unlock-runtime.log"
    # Avoid reading a previous run's base address before this process has loaded.
    if trace.exists():
        trace.rename(session / ("previous-runtime-" + str(time.time_ns()) + ".log"))
    command = build_command(emulator, args.game, trace, args.graphics_profile)
    (session / "launch-command.json").write_text(json.dumps(command, indent=2))
    print("Graphics profile: " + args.graphics_profile +
          (" (1080p requested -> 4K FSR; see stderr.log for actual dimensions)"
           if args.graphics_profile == "fsr4k" else " (FSR disabled)"), flush=True)
    with (session / "stdout.log").open("wb") as out, (session / "stderr.log").open("wb") as err:
        process = subprocess.Popen(command, cwd=session, stdout=out, stderr=err)
        (session / "process-id.txt").write_text(str(process.pid))
        try:
            base = wait_for_module(process, trace, plan["module"])
            memory = Memory(process.pid)
            try:
                memory.apply(base, plan)
            finally:
                memory.k.CloseHandle(memory.h)
            require(hashes(normal) == before, "Normal save changed during startup")
            (session / "applied.json").write_text(json.dumps({"pid": process.pid, "base": hex(base),
                                                           "patches": plan["patches"]}, indent=2))
            print("Temporary unlock applied to isolated session. Normal save untouched.", flush=True)
            print("J: confirm/jump; K: back; arrows: move; Enter: pause. Close game to disable.", flush=True)
            code = process.wait()
            require(hashes(normal) == before, "Normal save changed during session")
            print("Session ended; normal save hashes unchanged.", flush=True)
            return code
        except Exception:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=15)
            raise


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        print("Unlock refused: " + str(error), file=sys.stderr)
        raise SystemExit(1)
