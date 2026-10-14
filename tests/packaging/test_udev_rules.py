import json
import os
import re
import shutil
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _args import pop_args  # noqa: E402

(SOURCE_DIR,) = pop_args(1)
RULES = os.path.join(SOURCE_DIR, "packaging", "70-ghub-linux.rules")
PROPS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hidraw_properties.json")

ALLOWED_KEYS = {"SUBSYSTEM", "ENV{ID_VENDOR_ID}", "ENV{ID_MODEL_ID}", "ENV{ID_USB_INTERFACE_NUM}"}
ALLOWED_IDS = {("046d", "c547", "02"), ("046d", "c539", "02")}
TERM = re.compile(r'\s*([A-Za-z_]+(?:\{[^}]*\})?)\s*(==|!=|\+=|-=|:=|=)\s*"([^"]*)"\s*(,|$)')


class RuleError(Exception):
    pass


def parse_rules(text):
    """Analisa as regras; levanta RuleError em qualquer chave, operador ou ação fora da lista permitida."""
    rules = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        pos, terms = 0, []
        while pos < len(line):
            m = TERM.match(line, pos)
            if not m:
                raise RuleError(f"sintaxe não reconhecida: {line!r}")
            terms.append((m.group(1), m.group(2), m.group(3)))
            pos = m.end()
        matches, actions = {}, []
        for key, op, value in terms:
            if key in ALLOWED_KEYS:
                if op != "==" or key in matches:
                    raise RuleError(f"uso inválido de {key}{op}")
                matches[key] = value
            elif (key, op, value) == ("TAG", "+=", "uaccess"):
                actions.append(value)
            else:
                raise RuleError(f"chave/ação proibida: {key}{op}")
        if set(matches) != ALLOWED_KEYS or matches["SUBSYSTEM"] != "hidraw" or len(actions) != 1:
            raise RuleError(f"regra incompleta: {line!r}")
        rules.append(matches)
    return rules


def grants_uaccess(rules, props):
    for rule in rules:
        if all(props.get(k.removeprefix("ENV{").removesuffix("}") if k.startswith("ENV") else k) == v
               for k, v in rule.items()):
            return True
    return False



class UdevRules(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(RULES, encoding="utf-8") as f:
            cls.text = f.read()
        cls.rules = parse_rules(cls.text)

    def test_file_name_prefix_is_between_60_and_73(self):
        prefix = int(os.path.basename(RULES).split("-")[0])
        self.assertTrue(60 < prefix < 73)

    def test_only_allowed_vendor_model_interface_present(self):
        ids = {(r["ENV{ID_VENDOR_ID}"], r["ENV{ID_MODEL_ID}"], r["ENV{ID_USB_INTERFACE_NUM}"]) for r in self.rules}
        self.assertEqual(ALLOWED_IDS, ids)
        self.assertEqual(len(ALLOWED_IDS), len(self.rules))

    def test_real_properties_grant_uaccess_exactly(self):
        with open(PROPS, encoding="utf-8") as f:
            nodes = json.load(f)["nodes"]
        granted = {n for n, p in nodes.items() if grants_uaccess(self.rules, p)}
        self.assertEqual({"hidraw2", "hidraw7", "hidraw8", "hidraw9"}, granted)

    def test_synthetic_negative_cases(self):
        base = {"SUBSYSTEM": "hidraw", "ID_VENDOR_ID": "046d", "ID_MODEL_ID": "c547", "ID_USB_INTERFACE_NUM": "02"}
        self.assertTrue(grants_uaccess(self.rules, base))
        for change in ({"ID_USB_INTERFACE_NUM": "00"}, {"ID_MODEL_ID": "c33e"}, {"ID_VENDOR_ID": "048d"},
                       {"ID_MODEL_ID": "0afe", "ID_USB_INTERFACE_NUM": "03"}, {"SUBSYSTEM": "usb"}):
            with self.subTest(change=change):
                self.assertFalse(grants_uaccess(self.rules, {**base, **change}))

    def test_analyzer_rejects_bad_rules(self):
        good = ('SUBSYSTEM=="hidraw", ENV{ID_VENDOR_ID}=="046d", ENV{ID_MODEL_ID}=="c547", '
                'ENV{ID_USB_INTERFACE_NUM}=="02", TAG+="uaccess"')
        self.assertEqual(1, len(parse_rules(good)))
        for extra in (', MODE="0666"', ', GROUP="users"', ', OWNER="x"', ', RUN+="/bin/true"',
                      ', PROGRAM="/bin/true"', ', IMPORT{program}="x"', ', ATTRS{idProduct}=="c547"',
                      ', ATTR{power/control}="on"', ', SYMLINK+="x"'):
            with self.subTest(extra=extra):
                with self.assertRaises(RuleError):
                    parse_rules(good + extra)
        with self.assertRaises(RuleError):
            parse_rules(good.replace(', TAG+="uaccess"', ""))
        with self.assertRaises(RuleError):
            parse_rules(good.replace('TAG+="uaccess"', 'TAG+="seat"'))

    def test_udevadm_verify(self):
        if shutil.which("udevadm") is None:
            self.skipTest("udevadm ausente")
        result = subprocess.run(["udevadm", "verify", RULES], capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
