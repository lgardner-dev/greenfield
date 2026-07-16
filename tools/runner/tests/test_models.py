from __future__ import annotations

import unittest

from greenfield_agent.models import TaskState, transition_state
from greenfield_agent.runner import RunnerError, validate_task_id


class TaskIdTests(unittest.TestCase):
    def test_accepts_documented_task_ids(self) -> None:
        for task_id in ("a", "Task-12", "release_1.2", "9.x"):
            with self.subTest(task_id=task_id):
                self.assertEqual(validate_task_id(task_id), task_id)

    def test_rejects_empty_and_malformed_task_ids(self) -> None:
        for task_id in (
            "",
            "-starts-wrong",
            ".starts-wrong",
            "has space",
            "slash/name",
            "ends.",
            "has..dots",
            "branch.lock",
        ):
            with self.subTest(task_id=task_id):
                with self.assertRaises(RunnerError):
                    validate_task_id(task_id)


class TaskStateTests(unittest.TestCase):
    def test_valid_state_transitions(self) -> None:
        self.assertIs(transition_state(TaskState.READY, TaskState.RUNNING), TaskState.RUNNING)
        for terminal_state in (
            TaskState.REVIEW,
            TaskState.BLOCKED_CODEX,
            TaskState.BLOCKED_VALIDATION,
            TaskState.BLOCKED_RUNNER,
        ):
            with self.subTest(state=terminal_state):
                self.assertIs(
                    transition_state(TaskState.RUNNING, terminal_state), terminal_state
                )

    def test_invalid_state_transition_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            transition_state(TaskState.READY, TaskState.REVIEW)
        with self.assertRaises(ValueError):
            transition_state(TaskState.REVIEW, TaskState.RUNNING)


if __name__ == "__main__":
    unittest.main()
