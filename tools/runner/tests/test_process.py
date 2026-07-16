from __future__ import annotations

import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

from greenfield_agent.process import run_timed_process


PROCESS_TREE = r'''#!/usr/bin/env python3
import signal
import subprocess
import sys
import time
from pathlib import Path

child_ready = Path(sys.argv[1])
survival_marker = Path(sys.argv[2])
child_program = """
import signal
import sys
import time
from pathlib import Path

signal.signal(signal.SIGTERM, signal.SIG_IGN)
Path(sys.argv[1]).write_text("ready\\n", encoding="utf-8")
time.sleep(0.8)
Path(sys.argv[2]).write_text("survived\\n", encoding="utf-8")
time.sleep(30)
"""
subprocess.Popen([sys.executable, "-c", child_program, str(child_ready), str(survival_marker)])
deadline = time.monotonic() + 5
while not child_ready.exists():
    if time.monotonic() >= deadline:
        raise SystemExit("child did not start")
    time.sleep(0.01)
time.sleep(30)
'''


class TimedProcessTests(unittest.TestCase):
    def test_timeout_terminates_the_entire_process_group(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            program = root / "process-tree.py"
            child_ready = root / "child-ready"
            survival_marker = root / "child-survived"
            program.write_text(PROCESS_TREE, encoding="utf-8")
            program.chmod(0o755)
            with open(os.devnull, "rb") as stdin:
                with open(os.devnull, "wb") as stdout:
                    with open(os.devnull, "wb") as stderr:
                        result = run_timed_process(
                            [sys.executable, str(program), str(child_ready), str(survival_marker)],
                            cwd=root,
                            stdin=stdin,
                            stdout=stdout,
                            stderr=stderr,
                            timeout_seconds=0.3,
                            terminate_grace_seconds=0.1,
                        )
            self.assertTrue(result.timed_out)
            self.assertEqual(result.returncode, 124)
            time.sleep(0.8)
            self.assertFalse(
                survival_marker.exists(),
                "a grandchild survived the process-group timeout termination",
            )


if __name__ == "__main__":
    unittest.main()
