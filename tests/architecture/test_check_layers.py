#!/usr/bin/env python3
"""Autoteste do verificador de camadas: cada regra precisa detectar a violação e aceitar o caso válido."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_layers  # noqa: E402


def make_tree(files: dict[str, str]) -> Path:
    root = Path(tempfile.mkdtemp(prefix="ghub-arch-"))
    for rel, content in files.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    return root


class CheckLayersTest(unittest.TestCase):
    def violations(self, files: dict[str, str]) -> list[str]:
        return check_layers.check(make_tree(files))

    def test_accepts_valid_tree(self) -> None:
        self.assertEqual(self.violations({
            "src/shared_kernel/Result.hpp": "#include <expected>\n",
            "src/modules/a/domain/X.hpp": '#include "shared_kernel/Result.hpp"\n#include <mutex>\n',
            "src/modules/a/application/Y.hpp": '#include "modules/a/domain/X.hpp"\n#include "modules/b/api/B.hpp"\n',
            "src/modules/a/infrastructure/Z.cpp": '#include "modules/a/application/Y.hpp"\n#include <QString>\n',
            "src/modules/a/presentation/V.cpp": '#include "modules/a/application/Y.hpp"\n#include <QObject>\n',
        }), [])

    def test_rejects_qt_in_domain(self) -> None:
        found = self.violations({"src/modules/a/domain/X.hpp": "#include <QString>\n"})
        self.assertEqual(len(found), 1)
        self.assertIn("src/modules/a/domain/X.hpp:1", found[0])

    def test_rejects_io_in_application(self) -> None:
        for header in ("<filesystem>", "<fstream>", "<hidapi/hidapi.h>", "<linux/videodev2.h>",
                       "<pipewire/pipewire.h>", "<sys/socket.h>", "<QtCore/QObject>", "<dbus/dbus.h>"):
            with self.subTest(header=header):
                found = self.violations({"src/modules/a/application/Y.hpp": f"#include {header}\n"})
                self.assertEqual(len(found), 1, found)

    def test_rejects_qt_in_shared_kernel(self) -> None:
        self.assertEqual(len(self.violations({"src/shared_kernel/K.hpp": "#include <QObject>\n"})), 1)

    def test_rejects_internal_layer_of_other_module(self) -> None:
        for layer in ("domain", "application", "infrastructure", "presentation"):
            with self.subTest(layer=layer):
                found = self.violations({"src/modules/a/application/Y.hpp": f'#include "modules/b/{layer}/X.hpp"\n'})
                self.assertEqual(len(found), 1, found)

    def test_rejects_inward_layer_depending_on_outer(self) -> None:
        cases = {
            "src/modules/a/domain/X.hpp": '#include "modules/a/application/Y.hpp"\n',
            "src/modules/a/application/Y.hpp": '#include "modules/a/infrastructure/Z.hpp"\n',
            "src/modules/a/infrastructure/Z.hpp": '#include "modules/a/presentation/V.hpp"\n',
            "src/modules/a/presentation/V.hpp": '#include "modules/a/infrastructure/Z.hpp"\n',
        }
        for path, content in cases.items():
            with self.subTest(path=path):
                self.assertEqual(len(self.violations({path: content})), 1)

    def test_rejects_domain_including_other_module_api(self) -> None:
        found = self.violations({"src/modules/a/domain/X.hpp": '#include "modules/b/api/B.hpp"\n'})
        self.assertEqual(len(found), 1, found)

    def test_rejects_shared_kernel_including_module(self) -> None:
        self.assertEqual(len(self.violations({"src/shared_kernel/K.hpp": '#include "modules/a/api/A.hpp"\n'})), 1)

    def test_rejects_api_depending_on_infrastructure(self) -> None:
        self.assertEqual(len(self.violations({"src/modules/a/api/A.hpp": '#include "modules/a/infrastructure/Z.hpp"\n'})), 1)

    def test_ignores_commented_include(self) -> None:
        self.assertEqual(self.violations({"src/modules/a/domain/X.hpp": "// #include <QString>\n"}), [])

    def test_reports_missing_src(self) -> None:
        self.assertEqual(len(check_layers.check(make_tree({}))), 1)


if __name__ == "__main__":
    unittest.main()
