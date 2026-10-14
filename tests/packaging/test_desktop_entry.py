import configparser
import os
import shutil
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _args import pop_args  # noqa: E402

(SOURCE_DIR,) = pop_args(1)
DESKTOP = os.path.join(SOURCE_DIR, "packaging", "ghub-linux.desktop")

REQUIRED = {
    "Type": "Application",
    "Name": "Hubless",
    "GenericName": "Configurador de periféricos",
    "Comment": "Configura teclado, mouse, headset e webcam Logitech",
    "Exec": "ghub-linux",
    "Icon": "ghub-linux",
    "Terminal": "false",
    "Categories": "Settings;HardwareSettings;Qt;",
    "Keywords": "Logitech;G915;G703;G733;Brio;RGB;DPI;",
    "StartupNotify": "true",
}


class DesktopEntry(unittest.TestCase):
    def test_validator_accepts_file_without_output(self):
        if shutil.which("desktop-file-validate") is None:
            self.skipTest("desktop-file-validate ausente")
        result = subprocess.run(["desktop-file-validate", DESKTOP], capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual("", result.stdout + result.stderr)

    def test_required_keys(self):
        parser = configparser.ConfigParser(interpolation=None, delimiters=("=",))
        parser.optionxform = str
        parser.read(DESKTOP, encoding="utf-8")
        self.assertEqual(["Desktop Entry"], parser.sections())
        self.assertEqual(REQUIRED, dict(parser["Desktop Entry"]))


if __name__ == "__main__":
    unittest.main()
