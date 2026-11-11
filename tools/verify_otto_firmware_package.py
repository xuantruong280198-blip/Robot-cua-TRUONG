#!/usr/bin/env python3
"""Verify that the merged Otto Desk image contains every component at its flash offset."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

EXPECTED_FLASH_FILES = {
    0x0: "bootloader/bootloader.bin",
    0x8000: "partition_table/partition-table.bin",
    0xD000: "ota_data_initial.bin",
    0x20000: "xiaozhi.bin",
    0x800000: "generated_assets.bin",
}


def verify_package(build_dir: Path) -> list[str]:
    build_dir = build_dir.resolve()
    flasher_path = build_dir / "flasher_args.json"
    manifest_path = build_dir / "BUILD-MANIFEST.txt"
    merged_path = build_dir / "otto-desk-merged.bin"

    for path in (flasher_path, manifest_path, merged_path):
        if not path.is_file():
            raise ValueError(f"required package file is missing: {path.name}")

    flasher = json.loads(flasher_path.read_text(encoding="utf-8"))
    settings = flasher.get("flash_settings", {})
    if settings.get("flash_size") != "16MB":
        raise ValueError("flasher_args.json must target 16MB flash")
    if settings.get("flash_mode") != "dio" or settings.get("flash_freq") != "80m":
        raise ValueError("unexpected flash mode or frequency")

    actual_files = {
        int(offset, 0): name
        for offset, name in flasher.get("flash_files", {}).items()
    }
    if actual_files != EXPECTED_FLASH_FILES:
        raise ValueError(
            f"flash map mismatch: expected {EXPECTED_FLASH_FILES}, got {actual_files}"
        )

    manifest = {}
    for line in manifest_path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            manifest[key.strip()] = value.strip()
    required_manifest = {
        "target": "esp32s3",
        "flash_size": "16MB",
        "flash_offset": "0x0",
        "partition_table": "partitions/v2/16m.csv",
        "merged_image": "otto-desk-merged.bin",
        "hardware_validation": "not-performed",
        "gpio_assignments": "unconfigured_by_default",
    }
    for key, expected in required_manifest.items():
        if manifest.get(key) != expected:
            raise ValueError(
                f"build manifest mismatch for {key}: expected {expected!r}, "
                f"got {manifest.get(key)!r}"
            )

    merged_size = merged_path.stat().st_size
    if merged_size <= 0 or merged_size > 16 * 1024 * 1024:
        raise ValueError(f"merged image size is invalid for 16MB flash: {merged_size}")

    verified = []
    with merged_path.open("rb") as merged:
        for offset, relative_name in sorted(EXPECTED_FLASH_FILES.items()):
            component_path = (build_dir / relative_name).resolve()
            if build_dir not in component_path.parents or not component_path.is_file():
                raise ValueError(f"flash component is missing or unsafe: {relative_name}")
            component_size = component_path.stat().st_size
            if component_size <= 0 or offset + component_size > merged_size:
                raise ValueError(f"component does not fit in merged image: {relative_name}")
            merged.seek(offset)
            with component_path.open("rb") as component:
                remaining = component_size
                while remaining:
                    expected_chunk = component.read(min(1024 * 1024, remaining))
                    actual_chunk = merged.read(len(expected_chunk))
                    if not expected_chunk or actual_chunk != expected_chunk:
                        raise ValueError(
                            f"merged image mismatch at {relative_name} offset 0x{offset:x}"
                        )
                    remaining -= len(expected_chunk)
            verified.append(f"{relative_name}@0x{offset:x} ({component_size} bytes)")

    if (build_dir / "bootloader/bootloader.bin").read_bytes()[:1] != b"\xE9":
        raise ValueError("bootloader image does not have ESP image magic")
    if (build_dir / "xiaozhi.bin").read_bytes()[:1] != b"\xE9":
        raise ValueError("application image does not have ESP image magic")
    return verified


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir", type=Path)
    args = parser.parse_args()
    try:
        verified = verify_package(args.build_dir)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"PACKAGE VERIFICATION FAILED: {exc}", file=sys.stderr)
        return 1
    print("PACKAGE VERIFICATION PASSED")
    for item in verified:
        print(f"  {item}")
    print(f"  merged image ({(args.build_dir / 'otto-desk-merged.bin').stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
