from __future__ import annotations

import argparse
import json
import os
import tkinter as tk
from tkinter import messagebox, scrolledtext
from typing import Dict

from .ide_controller import IdeController, IdeRunRequest


def _parse_model_map(raw: str) -> Dict[int, str]:
    # Format: "1:model-a,2:model-b"
    raw = raw.strip()
    if not raw:
        return {}
    out: Dict[int, str] = {}
    for part in raw.split(","):
        seg = part.strip()
        if not seg:
            continue
        idx_txt, model = seg.split(":", 1)
        out[int(idx_txt.strip())] = model.strip()
    return out


def _parse_role_model_map(raw: str) -> Dict[str, str]:
    # Format: "ask:model-a,plan:model-b"
    raw = raw.strip()
    if not raw:
        return {}
    out: Dict[str, str] = {}
    for part in raw.split(","):
        seg = part.strip()
        if not seg:
            continue
        role, model = seg.split(":", 1)
        out[role.strip().lower()] = model.strip()
    return out


def _parse_pane_model_map(raw: str) -> Dict[str, str]:
    # Format: "left:model-a,right:model-b"
    raw = raw.strip()
    if not raw:
        return {}
    out: Dict[str, str] = {}
    for part in raw.split(","):
        seg = part.strip()
        if not seg:
            continue
        pane, model = seg.split(":", 1)
        out[pane.strip()] = model.strip()
    return out


def _parse_share_sources(raw: str) -> list[str]:
    raw = raw.strip()
    if not raw:
        return []
    return [p.strip().lower() for p in raw.split(",") if p.strip()]


class SecFlowGuiApp:
    def __init__(self, root: tk.Tk, repo_root: str) -> None:
        self.root = root
        self.controller = IdeController(repo_root)
        self.root.title("SecFlow IDE GUI")
        self._build()

    def _build(self) -> None:
        self.workflow_var = tk.StringVar(value="secflow_automation/workflows/local_model_unstale_session.json")
        self.incident_var = tk.StringVar(value="incident.sample.json")
        self.operator_var = tk.StringVar(value="gui-operator")
        self.swarm_size_var = tk.StringVar(value="8")
        self.default_model_var = tk.StringVar(value="phi3-mini-q4.gguf")
        self.model_map_var = tk.StringVar(value="1:phi3-mini-q4.gguf,2:codestral22b.gguf")
        self.role_model_var = tk.StringVar(value="ask:phi3-mini-q4.gguf,plan:codestral22b.gguf,agent:phi3-mini-q4.gguf,support:codestral22b.gguf")
        self.pane_model_var = tk.StringVar(value="left:phi3-mini-q4.gguf,right:codestral22b.gguf")
        self.drow_var = tk.StringVar(value="gui-drow")
        self.parallel_var = tk.BooleanVar(value=True)
        self.share_sources_var = tk.StringVar(value="ide,desktop")
        self.dry_run_var = tk.BooleanVar(value=True)

        frame = tk.Frame(self.root)
        frame.pack(fill="both", expand=True, padx=10, pady=10)

        row = 0
        for label, var in [
            ("Workflow", self.workflow_var),
            ("Incident", self.incident_var),
            ("Operator", self.operator_var),
            ("Swarm Size (1-64)", self.swarm_size_var),
            ("Default Model", self.default_model_var),
            ("Model Map idx:model", self.model_map_var),
            ("Role Models role:model", self.role_model_var),
            ("Pane Models pane:model", self.pane_model_var),
            ("Drow Name", self.drow_var),
            ("Share Sources (ide,desktop)", self.share_sources_var),
        ]:
            tk.Label(frame, text=label).grid(row=row, column=0, sticky="w")
            tk.Entry(frame, textvariable=var, width=80).grid(row=row, column=1, sticky="we", padx=8, pady=2)
            row += 1

        tk.Checkbutton(frame, text="Dry Run", variable=self.dry_run_var).grid(row=row, column=1, sticky="w")
        row += 1
        tk.Checkbutton(frame, text="Parallel Swarm", variable=self.parallel_var).grid(row=row, column=1, sticky="w")
        row += 1

        tk.Button(frame, text="Run", command=self.run_workflow).grid(row=row, column=1, sticky="w", pady=6)
        row += 1

        self.output = scrolledtext.ScrolledText(frame, width=120, height=28)
        self.output.grid(row=row, column=0, columnspan=2, sticky="nsew")

        frame.grid_columnconfigure(1, weight=1)
        frame.grid_rowconfigure(row, weight=1)

    def run_workflow(self) -> None:
        try:
            req = IdeRunRequest(
                workflow_path=self.workflow_var.get(),
                incident_path=self.incident_var.get() or None,
                operator=self.operator_var.get() or "gui-operator",
                approvals=[],
                dry_run=self.dry_run_var.get(),
                swarm_size=int(self.swarm_size_var.get()),
                model_by_agent=_parse_model_map(self.model_map_var.get()),
                default_model=self.default_model_var.get(),
                role_models=_parse_role_model_map(self.role_model_var.get()),
                pane_models=_parse_pane_model_map(self.pane_model_var.get()),
                drow_name=self.drow_var.get() or "gui-drow",
                parallel_swarm=self.parallel_var.get(),
                share_sources=_parse_share_sources(self.share_sources_var.get()),
            )
            payload = self.controller.run(req)
            self.output.delete("1.0", tk.END)
            self.output.insert(tk.END, json.dumps(payload, indent=2))
        except Exception as e:
            messagebox.showerror("SecFlow GUI Error", str(e))


def main() -> None:
    parser = argparse.ArgumentParser(description="SecFlow GUI")
    parser.add_argument("--repo-root", default=os.getcwd())
    parser.add_argument("--smoke", action="store_true", help="Headless smoke run of GUI wiring")
    args = parser.parse_args()

    if args.smoke:
        controller = IdeController(args.repo_root)
        req = IdeRunRequest(
            workflow_path=os.path.join(args.repo_root, "secflow_automation/workflows/local_model_unstale_session.json"),
            incident_path=os.path.join(args.repo_root, "incident.sample.json"),
            operator="gui-smoke",
            approvals=[],
            dry_run=True,
            swarm_size=8,
            model_by_agent={1: "phi3-mini-q4.gguf", 2: "codestral22b.gguf"},
            default_model="phi3-mini-q4.gguf",
            role_models={"ask": "phi3-mini-q4.gguf", "plan": "codestral22b.gguf"},
            pane_models={"left": "phi3-mini-q4.gguf", "right": "codestral22b.gguf"},
            drow_name="gui-smoke-drow",
            parallel_swarm=True,
            share_sources=["ide", "desktop"],
        )
        payload = controller.run(req)
        print(json.dumps({"gui_smoke": True, "status": payload.get("status"), "swarm": payload.get("swarm")}, indent=2))
        return

    root = tk.Tk()
    app = SecFlowGuiApp(root, args.repo_root)
    root.geometry("1200x800")
    root.mainloop()


if __name__ == "__main__":
    main()
