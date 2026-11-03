#!/usr/bin/env python3
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ANDROID = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID)

manifest_path = Path(sys.argv[1])
tree = ET.parse(manifest_path)
root = tree.getroot()
application = root.find("application")
if application is None:
    raise SystemExit("AndroidManifest.xml has no application element")

perm = "android.permission.SYSTEM_ALERT_WINDOW"
if not any(x.get(f"{{{ANDROID}}}name") == perm for x in root.findall("uses-permission")):
    node = ET.Element("uses-permission", {f"{{{ANDROID}}}name": perm})
    root.insert(list(root).index(application), node)

components = [
    ("activity", "com.snakemod.PermissionActivity"),
    ("service", "com.snakemod.OverlayService"),
]
for tag, name in components:
    found = any(x.get(f"{{{ANDROID}}}name") == name for x in application.findall(tag))
    if not found:
        ET.SubElement(application, tag, {
            f"{{{ANDROID}}}name": name,
            f"{{{ANDROID}}}exported": "false",
        })

tree.write(manifest_path, encoding="utf-8", xml_declaration=True)
