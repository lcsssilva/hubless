"""Teste de tools/compare_ui_v01.py sem rodar o magick nem o Qt: o executor de comandos é injetado."""
import contextlib
import importlib.util
import io
import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SPEC = importlib.util.spec_from_file_location("compare_ui_v01", os.path.join(ROOT, "tools", "compare_ui_v01.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)


class FakeRun:
    """Registra os comandos e responde como o magick: tamanho 1000x500 e N pixels diferentes."""

    def __init__(self, differing=5000, compare_code=1):
        self.commands = []
        self.differing = differing
        self.compare_code = compare_code

    def __call__(self, command, **kwargs):
        self.commands.append((command, kwargs))
        if "identify" in command:
            return subprocess.CompletedProcess(command, 0, stdout="1000 500", stderr="")
        if "compare" in command:
            return subprocess.CompletedProcess(command, self.compare_code, stdout="", stderr="%d" % self.differing)
        return subprocess.CompletedProcess(command, 0, stdout="", stderr="")


class CompareTest(unittest.TestCase):
    def mapping(self):
        return {"window_crop": "1022x752+129+0", "render_crop": "1022x752+0+0", "fuzz": "8%", "masks": [[0, 0, 10, 10]],
                "screens": [{"render": "01-inicio", "capture": "a.png", "masks": [[20, 30, 40, 50]]}]}

    def test_percent_is_differing_pixels_over_window_area(self):
        run = FakeRun(differing=5000)
        with tempfile.TemporaryDirectory() as work:
            percent = tool.compare_screen("magick", self.mapping()["screens"][0], self.mapping(), "r", "c", work, work, run)
        self.assertAlmostEqual(percent, 1.0)

    def test_masks_are_drawn_on_both_images_and_capture_is_cropped(self):
        run = FakeRun()
        with tempfile.TemporaryDirectory() as work:
            tool.compare_screen("magick", self.mapping()["screens"][0], self.mapping(), "r", "c", work, work, run)
        prepares = [c for c, _ in run.commands if "-fill" in c]
        self.assertEqual(len(prepares), 2)
        for command in prepares:
            self.assertIn("rectangle 20,30 59,79", command)
            self.assertIn("rectangle 0,0 9,9", command)
        self.assertTrue(any("1022x752+129+0" in c for c in prepares))
        compare = [c for c, _ in run.commands if "compare" in c][0]
        self.assertEqual(compare[:4], ["magick", "compare", "-metric", "AE"])
        self.assertIn("-fuzz", compare)
        self.assertTrue(compare[-1].endswith("01-inicio-diff.png"))

    def test_compare_error_code_is_reported(self):
        run = FakeRun(compare_code=2)
        with tempfile.TemporaryDirectory() as work, self.assertRaises(RuntimeError):
            tool.compare_screen("magick", self.mapping()["screens"][0], self.mapping(), "r", "c", work, work, run)

    def test_exits_2_when_magick_is_missing(self):
        code = tool.main(["--build-dir", "b", "--diff-dir", "d"], run=FakeRun(), which=lambda _: None)
        self.assertEqual(code, 2)

    def test_main_renders_with_the_documented_environment_and_prints_percent(self):
        run = FakeRun()
        out = io.StringIO()
        with tempfile.TemporaryDirectory() as diff, contextlib.redirect_stdout(out):
            code = tool.main(["--build-dir", "build/x", "--diff-dir", diff], run=run, which=lambda _: "/usr/bin/magick")
        self.assertEqual(code, 0)
        first, kwargs = run.commands[0]
        self.assertEqual(first[0], "build/x/tests/ui/ghub_ui_tests")
        self.assertIn("test_rendersEachScreenAtMinimumSize", first[1])
        self.assertEqual(kwargs["env"]["QT_QPA_PLATFORMTHEME"], "kde")
        self.assertEqual(kwargs["env"]["QML_XHR_ALLOW_FILE_READ"], "1")
        self.assertEqual(kwargs["env"]["QT_QPA_PLATFORM"], "offscreen")
        self.assertIn("%", out.getvalue())
        self.assertIn("01-inicio", out.getvalue())

    def test_shipped_mapping_loads(self):
        data = tool.load_mapping(os.path.join(ROOT, "tools", "compare_ui_v01.json"))
        self.assertTrue(data["screens"])


if __name__ == "__main__":
    unittest.main()
