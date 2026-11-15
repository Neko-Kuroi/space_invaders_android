#!/usr/bin/env python3
"""Parse a Gradle --offline log for missing artifacts and curl them into
the local m2repo. Usage: fetch_missing.py <gradle-log> ; prints how many
files it fetched. Exit 0 if it fetched at least one file, 3 if none."""
import os
import re
import subprocess
import sys

REPO = os.path.expanduser("~/workspace/space_invaders_android/m2repo")
GOOGLE = "https://dl.google.com/dl/android/maven2"
CENTRAL = "https://repo.maven.apache.org/maven2"
CENTRAL2 = "https://repo1.maven.org/maven2"


def remotes_for(group):
    if group.startswith(("com.android", "com.google", "androidx")):
        return [GOOGLE, CENTRAL, CENTRAL2]
    return [CENTRAL, CENTRAL2, GOOGLE]


def fetch(rel_path, group):
    dest = os.path.join(REPO, rel_path)
    if os.path.exists(dest):
        return False
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    for base in remotes_for(group):
        url = base + "/" + rel_path
        r = subprocess.run(
            ["curl", "-sfSL", "-o", dest, "--max-time", "60", url],
            capture_output=True,
        )
        if r.returncode == 0:
            return True
        if os.path.exists(dest):
            os.remove(dest)
    return False


def main():
    log = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    wanted = {}  # rel_path -> group

    # Exact file paths Gradle tried in the local repo.
    for m in re.finditer(r"m2repo/([^'\s]+\.(?:pom|jar|module|aar))", log):
        rel = m.group(1)
        parts = rel.split("/")
        if len(parts) >= 4:
            wanted[rel] = ".".join(parts[:-3])

    # "Could not find group:artifact:version" and the offline-mode form
    # "No cached version of group:artifact:version available for offline mode"
    for m in re.finditer(
        r"(?:Could not find|No cached version of) ([\w.\-]+):([\w.\-]+):([\w.\-]+)",
        log,
    ):
        g, a, v = m.groups()
        base = "/".join(g.split(".") + [a, v])
        for ext in ("pom", "jar", "module"):
            wanted[f"{base}/{a}-{v}.{ext}"] = g

    # "Could not find filename.jar (group:artifact:version)"
    for m in re.finditer(
        r"Could not find (\S+\.(?:jar|aar|pom)) \(([\w.\-]+):([\w.\-]+):([\w.\-]+)\)",
        log,
    ):
        fname, g, a, v = m.groups()
        base = "/".join(g.split(".") + [a, v])
        wanted[f"{base}/{fname}"] = g
        wanted[f"{base}/{a}-{v}.pom"] = g

    got = 0
    for rel, g in sorted(wanted.items()):
        if fetch(rel, g):
            got += 1
            print("fetched", rel)
    print(f"total fetched: {got} / wanted {len(wanted)}")
    return 0 if got else 3


if __name__ == "__main__":
    sys.exit(main())
