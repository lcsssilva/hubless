import os
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _args import pop_args  # noqa: E402

BUILD_DIR, SOURCE_DIR = pop_args(2)

EXPECTED = {
    "usr/bin/ghub-linux",
    "usr/share/applications/ghub-linux.desktop",
    "usr/share/icons/hicolor/scalable/apps/ghub-linux.svg",
    "usr/lib/udev/rules.d/70-ghub-linux.rules",
}


def install(destdir, *extra):
    env = dict(os.environ, DESTDIR=destdir)
    return subprocess.run(
        ["cmake", "--install", BUILD_DIR, "--prefix", "/usr", *extra],
        env=env, capture_output=True, text=True, check=True)


def tree(destdir):
    found = set()
    for root, _dirs, files in os.walk(destdir):
        for name in files:
            found.add(os.path.relpath(os.path.join(root, name), destdir))
    return found


class InstallTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        install(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_exact_file_set(self):
        self.assertEqual(EXPECTED, tree(self.tmp.name))

    def test_icon_is_valid_svg(self):
        path = os.path.join(self.tmp.name, "usr/share/icons/hicolor/scalable/apps/ghub-linux.svg")
        root = ET.parse(path).getroot()
        self.assertEqual("{http://www.w3.org/2000/svg}svg", root.tag)

    def test_installed_binary_runs_smoke_test(self):
        exe = os.path.join(self.tmp.name, "usr/bin/ghub-linux")
        with tempfile.TemporaryDirectory() as cfg:
            env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_CONFIG_HOME=cfg)
            result = subprocess.run([exe, "--smoke-test"], env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stderr)

    def test_udev_rules_can_be_disabled(self):
        # Só configura (sem compilar): o script de instalação gerado deve omitir a regra.
        with tempfile.TemporaryDirectory() as bdir:
            subprocess.run(
                ["cmake", "-S", SOURCE_DIR, "-B", bdir, "-G", "Ninja", "-DGHUB_INSTALL_UDEV_RULES=OFF",
                 "-DGHUB_BUILD_APP=ON", "-DGHUB_BUILD_TESTS=OFF"],
                capture_output=True, text=True, check=True)
            script = open(os.path.join(bdir, "packaging", "cmake_install.cmake")).read()
            self.assertNotIn("70-ghub-linux.rules", script)
            self.assertIn("ghub-linux.desktop", script)


if __name__ == "__main__":
    unittest.main()
