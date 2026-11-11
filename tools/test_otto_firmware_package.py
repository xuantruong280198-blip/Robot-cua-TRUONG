import json
from pathlib import Path
import tempfile
import unittest

from tools.verify_otto_firmware_package import EXPECTED_FLASH_FILES, verify_package


class OttoFirmwarePackageTests(unittest.TestCase):
    def make_package(self, root: Path) -> None:
        files = {
            "bootloader/bootloader.bin": b"\xE9bootloader",
            "partition_table/partition-table.bin": b"partition-table",
            "ota_data_initial.bin": b"ota-data",
            "xiaozhi.bin": b"\xE9application",
            "generated_assets.bin": b"generated-assets",
        }
        for name, data in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

        flash_files = {
            hex(offset): name for offset, name in EXPECTED_FLASH_FILES.items()
        }
        (root / "flasher_args.json").write_text(json.dumps({
            "flash_settings": {
                "flash_size": "16MB",
                "flash_mode": "dio",
                "flash_freq": "80m",
            },
            "flash_files": flash_files,
        }), encoding="utf-8")
        (root / "BUILD-MANIFEST.txt").write_text(
            "target=esp32s3\n"
            "flash_size=16MB\n"
            "flash_offset=0x0\n"
            "partition_table=partitions/v2/16m.csv\n"
            "merged_image=otto-desk-merged.bin\n"
            "hardware_validation=not-performed\n"
            "gpio_assignments=unconfigured_by_default\n",
            encoding="utf-8",
        )
        merged = bytearray(0x800000 + len(files["generated_assets.bin"]))
        for offset, name in EXPECTED_FLASH_FILES.items():
            merged[offset:offset + len(files[name])] = files[name]
        (root / "otto-desk-merged.bin").write_bytes(merged)

    def test_accepts_exact_merged_flash_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_package(root)
            self.assertEqual(len(verify_package(root)), 5)

    def test_rejects_wrong_flash_offset(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_package(root)
            flasher = json.loads((root / "flasher_args.json").read_text())
            flasher["flash_files"]["0x20000"] = "generated_assets.bin"
            (root / "flasher_args.json").write_text(json.dumps(flasher))
            with self.assertRaisesRegex(ValueError, "flash map mismatch"):
                verify_package(root)

    def test_rejects_corrupted_merged_component(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_package(root)
            merged_path = root / "otto-desk-merged.bin"
            with merged_path.open("r+b") as merged:
                merged.seek(0x20000)
                merged.write(b"X")
            with self.assertRaisesRegex(ValueError, "merged image mismatch"):
                verify_package(root)

    def test_rejects_wrong_flash_size(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_package(root)
            flasher = json.loads((root / "flasher_args.json").read_text())
            flasher["flash_settings"]["flash_size"] = "8MB"
            (root / "flasher_args.json").write_text(json.dumps(flasher))
            with self.assertRaisesRegex(ValueError, "16MB flash"):
                verify_package(root)


if __name__ == "__main__":
    unittest.main()
