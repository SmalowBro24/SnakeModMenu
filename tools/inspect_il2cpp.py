#!/usr/bin/env python3
"""Read IL2CPP v31 metadata from an APK, split bundle, or extracted directory."""
from __future__ import annotations

import argparse
import json
import struct
import sys
import zipfile
from pathlib import Path
from typing import BinaryIO

MAGIC = 0xFAB11BAF
VERSION_SUPPORTED = 31


def find_metadata_in_zip(zf: zipfile.ZipFile) -> bytes | None:
    wanted = "assets/bin/data/managed/metadata/global-metadata.dat"
    for item in zf.infolist():
        if item.filename.lower() == wanted:
            return zf.read(item)
    # Split-APK set: metadata is normally in base.apk.
    for item in zf.infolist():
        if not item.filename.lower().endswith(".apk"):
            continue
        try:
            inner = zipfile.ZipFile(__import__("io").BytesIO(zf.read(item)))
            data = find_metadata_in_zip(inner)
            inner.close()
            if data is not None:
                return data
        except (zipfile.BadZipFile, OSError):
            continue
    return None


def load_metadata(path: Path) -> bytes:
    if path.is_dir():
        for candidate in path.rglob("global-metadata.dat"):
            return candidate.read_bytes()
        for candidate in path.rglob("*.apk"):
            try:
                with zipfile.ZipFile(candidate) as zf:
                    data = find_metadata_in_zip(zf)
                    if data is not None:
                        return data
            except zipfile.BadZipFile:
                continue
    elif path.is_file():
        try:
            with zipfile.ZipFile(path) as zf:
                data = find_metadata_in_zip(zf)
                if data is not None:
                    return data
        except zipfile.BadZipFile:
            pass
        # Also allow passing global-metadata.dat directly.
        raw = path.read_bytes()
        if len(raw) >= 8 and struct.unpack_from("<I", raw, 0)[0] == MAGIC:
            return raw
    raise FileNotFoundError("global-metadata.dat not found in input")


class Metadata:
    def __init__(self, data: bytes):
        if len(data) < 0xB0:
            raise ValueError("metadata file is too small")
        magic, self.version = struct.unpack_from("<II", data, 0)
        if magic != MAGIC:
            raise ValueError("invalid IL2CPP metadata magic")
        if self.version != VERSION_SUPPORTED:
            raise ValueError(f"script currently supports metadata v{VERSION_SUPPORTED}; found v{self.version}")
        self.data = data
        self.string_off, self.string_size = self.pair(0x18)
        self.method_off, self.method_size = self.pair(0x30)
        self.parameter_off, self.parameter_size = self.pair(0x58)
        self.field_off, self.field_size = self.pair(0x60)
        self.type_off, self.type_size = self.pair(0xA0)
        self.image_off, self.image_size = self.pair(0xA8)
        self.default_field_off, self.default_field_size = self.pair(0x40)
        self.default_data_off, self.default_data_size = self.pair(0x48)
        self.strings = data[self.string_off:self.string_off + self.string_size]

    def pair(self, offset: int) -> tuple[int, int]:
        return struct.unpack_from("<II", self.data, offset)

    def string(self, index: int) -> str:
        if index < 0 or index >= len(self.strings):
            return ""
        end = self.strings.find(b"\0", index)
        if end < 0:
            end = len(self.strings)
        return self.strings[index:end].decode("utf-8", "replace")

    def parse(self) -> dict:
        # v31: MethodDefinition = 0x24 bytes; TypeDefinition = 0x58 bytes.
        methods = []
        for i in range(self.method_size // 0x24):
            row = struct.unpack_from("<iiiiiiIHHHH", self.data, self.method_off + i * 0x24)
            name, declaring, return_type, return_param_token, param_start, generic_container, token, flags, iflags, slot, param_count = row
            methods.append({
                "name": self.string(name), "declaring_type": declaring,
                "return_type": return_type, "parameter_start": param_start,
                "parameter_count": param_count, "token": token,
                "flags": flags, "slot": slot,
            })

        parameters = []
        for i in range(self.parameter_size // 12):
            name, token, type_index = struct.unpack_from("<IIi", self.data, self.parameter_off + i * 12)
            parameters.append({"name": self.string(name), "token": token, "type_index": type_index})

        fields = []
        for i in range(self.field_size // 12):
            name, type_index, token = struct.unpack_from("<III", self.data, self.field_off + i * 12)
            fields.append({"name": self.string(name), "type_index": type_index, "token": token})

        types = []
        for i in range(self.type_size // 0x58):
            off = self.type_off + i * 0x58
            name, namespace = struct.unpack_from("<II", self.data, off)
            field_start, method_start = struct.unpack_from("<ii", self.data, off + 0x20)
            method_count, _property_count, field_count, _event_count = struct.unpack_from("<HHHH", self.data, off + 0x40)
            types.append({
                "index": i, "name": self.string(name), "namespace": self.string(namespace),
                "field_start": field_start, "method_start": method_start,
                "method_count": method_count, "field_count": field_count,
            })

        images = []
        for i in range(self.image_size // 0x28):
            off = self.image_off + i * 0x28
            name, _assembly, type_start, type_count = struct.unpack_from("<iiii", self.data, off)
            images.append({"name": self.string(name), "type_start": type_start, "type_count": type_count})

        targets = {
            "SnakeController", "PlayerSnakeController", "SnakeBoostController",
            "SpeedBoostTypeController", "SnakeStatePredictor",
        }
        report = {"metadata_version": self.version, "images": [], "targets": []}
        for image in images:
            start, end = image["type_start"], image["type_start"] + image["type_count"]
            target_types = [t for t in types[start:end] if t["name"] in targets or t["name"].startswith("SnakeStatePredictor")]
            if target_types:
                report["images"].append(image["name"])
            for typ in target_types:
                full_name = f'{typ["namespace"]}.{typ["name"]}' if typ["namespace"] else typ["name"]
                entry = {"image": image["name"], "type": full_name, "fields": [], "methods": []}
                if typ["field_start"] >= 0:
                    for field in fields[typ["field_start"]:typ["field_start"] + typ["field_count"]]:
                        entry["fields"].append(field)
                if typ["method_start"] >= 0:
                    for method in methods[typ["method_start"]:typ["method_start"] + typ["method_count"]]:
                        params = []
                        pstart = method["parameter_start"]
                        pcount = method["parameter_count"]
                        if pstart >= 0 and pstart + pcount <= len(parameters):
                            params = [p["name"] for p in parameters[pstart:pstart + pcount]]
                        entry["methods"].append({**method, "parameters": params})
                report["targets"].append(entry)
        return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="APK, split-APK bundle ZIP, directory, or global-metadata.dat")
    parser.add_argument("--json", action="store_true", help="emit JSON instead of a concise text report")
    args = parser.parse_args()
    try:
        metadata = Metadata(load_metadata(args.input))
        report = metadata.parse()
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f'IL2CPP metadata version: {report["metadata_version"]}')
        print("Assemblies: " + ", ".join(report["images"]))
        for target in report["targets"]:
            print(f'\n[{target["image"]}] {target["type"]}')
            for method in target["methods"]:
                params = ", ".join(method["parameters"])
                print(f'  0x{method["token"]:08X} {method["name"]}({params})')
            for field in target["fields"]:
                print(f'  field 0x{field["token"]:08X} {field["name"]}')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
