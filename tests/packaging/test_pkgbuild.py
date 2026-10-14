import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _args import pop_args  # noqa: E402

SOURCE_DIR, EXECUTABLE = pop_args(2)
PKGBUILD = os.path.join(SOURCE_DIR, "packaging", "arch", "PKGBUILD")

VARS = "pkgname pkgver pkgrel pkgdesc arch url license depends makedepends checkdepends optdepends provides conflicts source sha256sums"


def read_variables():
    if shutil.which("bash") is None:
        raise unittest.SkipTest("bash ausente")
    with tempfile.TemporaryDirectory() as cwd:
        out = subprocess.run(
            ["bash", "-c", f'source "$1"; declare -p {VARS}', "bash", PKGBUILD],
            cwd=cwd, env={"PATH": os.environ.get("PATH", ""), "LANG": "C"},
            capture_output=True, text=True, check=True).stdout
    result = {}
    for line in out.splitlines():
        m = re.match(r"declare -[-a]+ (\w+)=(.*)$", line)
        name, value = m.group(1), m.group(2)
        if value.startswith("("):
            result[name] = re.findall(r'\[\d+\]="((?:[^"\\]|\\.)*)"', value)
        else:
            result[name] = value.strip('"')
    return result


class PkgBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(PKGBUILD, encoding="utf-8") as f:
            cls.text = f.read()
        cls.v = read_variables()

    def test_bash_syntax(self):
        subprocess.run(["bash", "-n", PKGBUILD], check=True)

    def test_fields(self):
        v = self.v
        self.assertEqual("hubless-git", v["pkgname"])
        self.assertRegex(v["pkgver"], r"^r\d+\.[0-9a-f]{7}$")
        self.assertEqual("1", v["pkgrel"])
        self.assertEqual(["x86_64"], v["arch"])
        self.assertEqual("https://github.com/lcsssilva/hubless", v["url"])
        self.assertEqual(["GPL-3.0-or-later"], v["license"])
        self.assertEqual(["hubless"], v["provides"])
        self.assertEqual(["hubless"], v["conflicts"])
        self.assertEqual(["hubless::git+https://github.com/lcsssilva/hubless.git"], v["source"])
        self.assertEqual(["SKIP"], v["sha256sums"])
        self.assertEqual(["cmake", "git", "gtest", "ninja"], sorted(v["makedepends"]))
        self.assertEqual(["python"], v["checkdepends"])
        self.assertEqual(
            ["gcc-libs", "glibc", "hicolor-icon-theme", "libglvnd", "libpipewire", "qt6-base", "qt6-declarative"],
            sorted(v["depends"]))

    def test_optdepends_have_descriptions(self):
        opt = {item.split(": ", 1)[0]: item.split(": ", 1)[1] for item in self.v["optdepends"]}
        self.assertEqual({"headsetcontrol", "libratbag", "pipewire", "noise-suppression-for-voice"}, set(opt))
        self.assertTrue(all(opt.values()))

    def test_functions_exist(self):
        out = subprocess.run(
            ["bash", "-c", 'source "$1"; declare -F', "bash", PKGBUILD],
            cwd=tempfile.gettempdir(), env={"PATH": os.environ.get("PATH", "")},
            capture_output=True, text=True, check=True).stdout
        names = {line.split()[-1] for line in out.splitlines()}
        self.assertTrue({"pkgver", "build", "check", "package"} <= names)

    def test_build_package_commands(self):
        self.assertIn("-DCMAKE_BUILD_TYPE=None", self.text)
        self.assertIn("-DCMAKE_INSTALL_PREFIX=/usr", self.text)
        self.assertIn("QT_QPA_PLATFORM=offscreen ctest", self.text)
        self.assertIn('DESTDIR="$pkgdir" cmake --install', self.text)

    def test_forbidden_content(self):
        code = "\n".join(l for l in self.text.splitlines() if not l.lstrip().startswith("#"))
        for word in ("sudo", "udevadm"):
            self.assertNotRegex(code, rf"\b{word}\b")
        self.assertNotRegex(code, r"^\s*install\s*=", )

    @staticmethod
    def pulled_by(depends, env):
        """Pacotes que os próprios depends exigem (um nível): cobre gcc-libs dividido em libgcc/libstdc++."""
        pulled = set()
        for dep in depends:
            info = subprocess.run(["pacman", "-Qi", dep], env=env, capture_output=True, text=True).stdout
            m = re.search(r"^Depends On\s*:\s*(.*)$", info, re.M)
            if m:
                pulled |= {re.split(r"[<>=]", d)[0] for d in m.group(1).split()}
        return pulled

    def test_needed_libraries_belong_to_depends(self):
        for tool in ("readelf", "pacman"):
            if shutil.which(tool) is None:
                self.skipTest(f"{tool} ausente")
        if not os.path.exists(EXECUTABLE):
            self.skipTest("executável não compilado")
        env = dict(os.environ, LANG="C")
        dyn = subprocess.run(["readelf", "-d", EXECUTABLE], env=env, capture_output=True, text=True,
                             check=True).stdout
        needed = re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", dyn)
        self.assertTrue(needed)
        depends = set(self.v["depends"])
        checked = 0
        for lib in needed:
            path = next((p for p in (f"/usr/lib/{lib}", f"/usr/lib64/{lib}") if os.path.exists(p)), None)
            if path is None:
                continue
            res = subprocess.run(["pacman", "-Qoq", os.path.realpath(path)], env=env, capture_output=True, text=True)
            owner = res.stdout.strip()
            if res.returncode != 0 or not owner:
                continue
            checked += 1
            with self.subTest(lib=lib, owner=owner):
                self.assertTrue(owner in depends or owner in self.pulled_by(depends, env),
                                f"{owner} (dono de {lib}) não está em depends nem é puxado por ele")
        if checked == 0:
            self.skipTest("nenhuma biblioteca pertence a pacote")


if __name__ == "__main__":
    unittest.main()
