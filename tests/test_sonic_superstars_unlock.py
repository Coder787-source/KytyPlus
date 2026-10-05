"""Portable checks without accessing another process or a game dump."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("unlock", ROOT / "scripts/sonic_superstars_unlock.py")
unlock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(unlock)


class UnlockTests(unittest.TestCase):
    def test_graphics_profiles(self):
        command = unlock.build_command(Path("emulator.exe"), Path("game"), Path("trace.log"), "fsr4k")
        for flag, value in {"--guest-render-width": "1920", "--guest-render-height": "1080",
                            "--fsr-output-width": "3840", "--fsr-output-height": "2160",
                            "--upscaler-method": "Fsr1", "--upscaler-sharpness": "0.3"}.items():
            self.assertEqual(command[command.index(flag) + 1], value)
        native = unlock.build_command(Path("emulator.exe"), Path("game"), Path("trace.log"), "native")
        self.assertEqual(native[native.index("--upscaler-method") + 1], "Off")
        self.assertNotIn("--fsr-output-width", native)
        with self.assertRaises(ValueError):
            unlock.build_command(Path("e"), Path("g"), Path("t"), "unknown")

    def test_plan_guards_and_no_overlapping_writes(self):
        plan = json.loads(unlock.PLAN.read_text())
        self.assertEqual(plan["title_id"], "PPSA06888")
        self.assertEqual(plan["game_version"], "01.001.008")
        self.assertEqual(len(plan["module_sha256"]), 64)
        intervals = []
        for patch in plan["patches"]:
            expected = bytes.fromhex(patch["expected"])
            value = bytes.fromhex(patch["replacement"])
            self.assertEqual(len(expected), 32)
            self.assertTrue(0 < len(value) <= len(expected))
            start = int(patch["offset"], 16)
            intervals.append((start, start + len(value)))
        intervals.sort()
        for left, right in zip(intervals, intervals[1:]):
            self.assertLessEqual(left[1], right[0])

    def test_validation_rejects_wrong_title_and_module_without_writes(self):
        plan = json.loads(unlock.PLAN.read_text())
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            (game / "sce_sys").mkdir()
            param = game / "sce_sys/param.json"
            param.write_text(json.dumps({"titleId": "wrong", "contentVersion": plan["game_version"]}))
            with self.assertRaisesRegex(RuntimeError, "Wrong game/version"):
                unlock.validate(game, plan)
            param.write_text(json.dumps({"titleId": plan["title_id"], "contentVersion": plan["game_version"]}))
            module = game / plan["module"]
            module.parent.mkdir(parents=True)
            module.write_bytes(b"not the supported module")
            before = unlock.hashes(game)
            with self.assertRaisesRegex(RuntimeError, "Wrong module SHA256"):
                unlock.validate(game, plan)
            self.assertEqual(unlock.hashes(game), before)


if __name__ == "__main__":
    unittest.main()
