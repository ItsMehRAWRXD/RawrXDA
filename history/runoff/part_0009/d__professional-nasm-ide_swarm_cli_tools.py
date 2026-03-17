"""Small CLI wrapper for model_registry functions."""
from __future__ import annotations
import argparse
import json
from model_registry import introspect_model, list_reports, get_report, clone_model, apply_overlay


def main(argv=None):
    parser = argparse.ArgumentParser(prog="model-cli")
    sub = parser.add_subparsers(dest="cmd")

    p_inspect = sub.add_parser("inspect")
    p_inspect.add_argument("path")
    p_inspect.add_argument("--register", action="store_true")

    p_list = sub.add_parser("list")
    p_list.add_argument("--limit", type=int, default=20)

    p_get = sub.add_parser("get")
    p_get.add_argument("id", type=int)

    p_clone = sub.add_parser("clone")
    p_clone.add_argument("src")
    p_clone.add_argument("dst")
    p_clone.add_argument("--register", action="store_true")

    p_overlay = sub.add_parser("overlay")
    p_overlay.add_argument("model")
    p_overlay.add_argument("overlay_json")
    p_overlay.add_argument("--register", action="store_true")

    args = parser.parse_args(argv)
    if args.cmd == "inspect":
        r = introspect_model(args.path, register=args.register)
        print(json.dumps(r, indent=2))
    elif args.cmd == "list":
        print(json.dumps(list_reports(limit=args.limit), indent=2))
    elif args.cmd == "get":
        print(json.dumps(get_report(args.id), indent=2))
    elif args.cmd == "clone":
        path = clone_model(args.src, args.dst, register=args.register)
        print(path)
    elif args.cmd == "overlay":
        with open(args.overlay_json, "r", encoding="utf-8") as fh:
            overlay = json.load(fh)
        out = apply_overlay(args.model, overlay, register=args.register)
        print(out)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
