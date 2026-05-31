import json
import os
import tempfile
import unittest

from secflow_automation.ide_controller import IdeController, IdeRunRequest
from secflow_automation.security import (
    SecurityValidationError,
    normalize_swarm_models,
    validate_drow_name,
    validate_pane_models,
    validate_role_models,
    validate_share_sources,
    validate_swarm_size,
)


class SecurityAndControllerTests(unittest.TestCase):
    def test_swarm_size_bounds(self):
        self.assertEqual(validate_swarm_size(1), 1)
        self.assertEqual(validate_swarm_size(64), 64)
        with self.assertRaises(SecurityValidationError):
            validate_swarm_size(0)
        with self.assertRaises(SecurityValidationError):
            validate_swarm_size(65)

    def test_model_map_normalization(self):
        models = normalize_swarm_models(4, {2: "codestral22b.gguf"}, "phi3-mini-q4.gguf")
        self.assertEqual(len(models), 4)
        self.assertEqual(models[1], "codestral22b.gguf")

    def test_role_pane_drow_and_share_validators(self):
        role_models = validate_role_models({"ask": "phi3-mini-q4.gguf", "plan": "codestral22b.gguf"})
        self.assertEqual(role_models["ask"], "phi3-mini-q4.gguf")
        pane_models = validate_pane_models({"left": "phi3-mini-q4.gguf", "right": "codestral22b.gguf"})
        self.assertEqual(pane_models["right"], "codestral22b.gguf")
        self.assertEqual(validate_drow_name("project-drow-01"), "project-drow-01")
        self.assertEqual(validate_share_sources(["ide", "desktop"]), ["ide", "desktop"])

        with self.assertRaises(SecurityValidationError):
            validate_role_models({"bad-role": "phi3-mini-q4.gguf"})
        with self.assertRaises(SecurityValidationError):
            validate_pane_models({"../bad": "phi3-mini-q4.gguf"})
        with self.assertRaises(SecurityValidationError):
            validate_drow_name("x")
        with self.assertRaises(SecurityValidationError):
            validate_share_sources(["ide", "desktop", "desktop"])

    def test_cli_gui_shared_controller_path(self):
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        controller = IdeController(repo_root)
        workflow = os.path.join(repo_root, "secflow_automation", "workflows", "local_model_unstale_session.json")
        incident = os.path.join(repo_root, "incident.sample.json")

        req = IdeRunRequest(
            workflow_path=workflow,
            incident_path=incident,
            operator="controller-test",
            approvals=[],
            dry_run=True,
            swarm_size=16,
            model_by_agent={1: "phi3-mini-q4.gguf", 2: "codestral22b.gguf"},
            default_model="phi3-mini-q4.gguf",
            role_models={"ask": "phi3-mini-q4.gguf", "plan": "codestral22b.gguf", "agent": "phi3-mini-q4.gguf", "support": "codestral22b.gguf"},
            pane_models={"left": "phi3-mini-q4.gguf", "right": "codestral22b.gguf"},
            drow_name="controller-test-drow",
            parallel_swarm=True,
            share_sources=["ide", "desktop"],
        )

        payload = controller.run(req)
        self.assertEqual(payload["status"], "completed")
        self.assertEqual(payload["swarm"]["size"], 16)
        self.assertEqual(len(payload["swarm"]["models"]), 16)
        self.assertEqual(payload["drow"]["name"], "controller-test-drow")
        self.assertIn("left", payload["panes"])
        self.assertIn("right", payload["panes"])
        self.assertEqual(len(payload["screen_shares"]), 2)
        self.assertIn("swarm_runtime", payload)
        self.assertEqual(payload["swarm_runtime"]["swarm_size"], 16)


if __name__ == "__main__":
    unittest.main()
