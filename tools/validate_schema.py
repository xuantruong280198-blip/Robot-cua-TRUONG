#!/usr/bin/env python3
"""Validate the canonical hardware JSON against its published JSON Schema."""

import json
import pathlib
import sys

try:
    from jsonschema import Draft202012Validator
except ImportError as exc:
    print(f"ERROR: jsonschema dependency is required: {exc}", file=sys.stderr)
    raise SystemExit(2)

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCHEMA_PATH = ROOT / "config" / "schema-v1.schema.json"
CONFIG_PATH = ROOT / "config" / "default.json"


def main() -> None:
    try:
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"ERROR: cannot load schema/config: {exc}", file=sys.stderr)
        raise SystemExit(1)

    validator = Draft202012Validator(schema)
    errors = sorted(validator.iter_errors(config), key=lambda error: list(map(str, error.absolute_path)))
    if errors:
        for error in errors:
            path = ".".join(str(part) for part in error.absolute_path) or "root"
            print(f"ERROR: {path}: {error.message}", file=sys.stderr)
        raise SystemExit(1)

    print("JSON Schema validation passed (Draft 2020-12).")


if __name__ == "__main__":
    main()
