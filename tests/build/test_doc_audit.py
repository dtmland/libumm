#!/usr/bin/env python3
"""Session 51: every heading in audited docs maps to a test or an untested reason."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
AUDIT = REPO_ROOT / "tests" / "verification" / "doc-audit.json"
HEADING_RE = re.compile(r"^(#{1,3}) (.+)$")


def headings(path: Path) -> list[str]:
    found: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = HEADING_RE.match(line)
        if match:
            found.append(match.group(2).strip())
    return found


class TestDocAudit(unittest.TestCase):
    def test_json_is_lf_utf8(self) -> None:
        raw = AUDIT.read_bytes()
        self.assertFalse(raw.startswith(b"\xef\xbb\xbf"))
        self.assertNotIn(b"\r\n", raw)
        raw.decode("utf-8")
        self.assertTrue(raw.endswith(b"\n"))

    def test_headings_are_covered(self) -> None:
        data = json.loads(AUDIT.read_text(encoding="utf-8"))
        self.assertEqual(data["version"], 1)
        documents = data["documents"]
        claims = data["claims"]
        by_doc: dict[str, list[dict]] = {doc: [] for doc in documents}
        for claim in claims:
            self.assertIn(claim["document"], documents, claim["heading"])
            tests = claim.get("tests") or []
            untested = (claim.get("untested") or "").strip()
            self.assertTrue(
                tests or untested,
                f"{claim['document']} #{claim['heading']} has neither tests nor untested",
            )
            for relpath in tests:
                path = REPO_ROOT / relpath
                self.assertTrue(
                    path.is_file(),
                    f"{claim['heading']}: missing test {relpath}",
                )
            by_doc[claim["document"]].append(claim)

        for relpath in documents:
            path = REPO_ROOT / relpath
            self.assertTrue(path.is_file(), relpath)
            have = headings(path)
            listed = [claim["heading"] for claim in by_doc[relpath]]
            self.assertEqual(
                have,
                listed,
                f"{relpath} headings drifted from tests/verification/doc-audit.json",
            )


if __name__ == "__main__":
    unittest.main()
