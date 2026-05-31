import json
import tempfile
import unittest

from secflow_automation.engine import WorkflowEngine
from secflow_automation.models import ExecutionContext


class WorkflowEngineTests(unittest.TestCase):
    def setUp(self) -> None:
        self.engine = WorkflowEngine()

    def _write_workflow(self, data):
        fd, path = tempfile.mkstemp(suffix=".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f)
        return path

    def test_blocks_high_impact_without_approval(self):
        wf = {
            "name": "test",
            "description": "approval check",
            "risk_tier": "high",
            "required_approvals": [],
            "steps": [
                {"id": "a", "action": "isolate_host", "params": {"host": "x"}}
            ],
        }
        path = self._write_workflow(wf)
        workflow = self.engine.load_workflow(path)
        ctx = ExecutionContext(operator="tester", incident={"severity": 9}, approvals=[], dry_run=True)
        report = self.engine.execute(workflow, ctx)

        self.assertEqual(report.status, "partial_failure")
        self.assertEqual(report.step_results[0].status, "blocked")

    def test_runs_when_approved(self):
        wf = {
            "name": "test-approved",
            "description": "approval pass",
            "risk_tier": "high",
            "required_approvals": ["a"],
            "steps": [
                {"id": "a", "action": "isolate_host", "params": {"host": "x"}}
            ],
        }
        path = self._write_workflow(wf)
        workflow = self.engine.load_workflow(path)
        ctx = ExecutionContext(operator="tester", incident={"severity": 9}, approvals=["a"], dry_run=True)
        report = self.engine.execute(workflow, ctx)

        self.assertEqual(report.status, "completed")
        self.assertEqual(report.step_results[0].status, "completed")

    def test_persists_step_output_for_conditions(self):
        wf = {
            "name": "vars-chain",
            "description": "step output persistence",
            "risk_tier": "high",
            "required_approvals": [],
            "steps": [
                {
                    "id": "hardware_benchmark",
                    "action": "benchmark_inference_backends",
                    "params": {"gpu_vendor": "amd", "candidates": ["amd_rocm", "cuda"]},
                },
                {
                    "id": "notify",
                    "action": "notify_channel",
                    "depends_on": ["hardware_benchmark"],
                    "condition": "vars.steps.hardware_benchmark.best_backend == amd_rocm",
                },
            ],
        }
        path = self._write_workflow(wf)
        workflow = self.engine.load_workflow(path)
        ctx = ExecutionContext(operator="tester", incident={"severity": 8}, approvals=[], dry_run=True)
        report = self.engine.execute(workflow, ctx)

        self.assertEqual(report.status, "completed")
        self.assertIn("steps", ctx.vars)
        self.assertEqual(ctx.vars["steps"]["hardware_benchmark"]["best_backend"], "amd_rocm")
        self.assertEqual(report.step_results[1].status, "completed")

    def test_local_session_fresh_swap_without_retry(self):
        wf = {
            "name": "local-session",
            "description": "fresh swap test",
            "risk_tier": "high",
            "required_approvals": [],
            "steps": [
                {
                    "id": "open_local_session",
                    "action": "open_local_model_session",
                    "params": {
                        "lane": "sharded_hot_swap",
                        "max_uses_before_stale": 1,
                        "allow_fresh_swap_no_retry": True,
                    },
                },
                {
                    "id": "inference_1",
                    "action": "run_local_model_inference",
                    "depends_on": ["open_local_session"],
                    "params": {"min_tps": 1000},
                },
                {
                    "id": "inference_2",
                    "action": "run_local_model_inference",
                    "depends_on": ["inference_1"],
                    "params": {"min_tps": 1000},
                },
            ],
        }

        path = self._write_workflow(wf)
        workflow = self.engine.load_workflow(path)
        ctx = ExecutionContext(operator="tester", incident={"severity": 8}, approvals=[], dry_run=True)
        report = self.engine.execute(workflow, ctx)

        self.assertEqual(report.status, "completed")
        self.assertEqual(report.step_results[2].status, "completed")
        self.assertTrue(report.step_results[2].details.get("fresh_swap_without_retry"))


if __name__ == "__main__":
    unittest.main()
