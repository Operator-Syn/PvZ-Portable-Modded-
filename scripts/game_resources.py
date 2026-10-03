#!/usr/bin/env python3
"""Pack local game resources, or restore missing files from their ZIP bundle."""

import argparse
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]


def resource_path(name):
    parts = name.rstrip("/").split("/")
    if any(part in ("", ".", "..") or ":" in part for part in parts) or "\\" in name:
        raise ValueError(f"Unsafe archive path: {name}")
    if name != "main.pak" and parts[0] != "properties":
        raise ValueError(f"Unexpected resource path: {name}")
    return Path(*PurePosixPath(name).parts)


def pack(root, archive):
    if not (root / "main.pak").is_file() or not (root / "properties").is_dir():
        raise ValueError("Packing requires main.pak and properties/ in the resource root")
    files = [root / "main.pak"]
    for path in sorted((root / "properties").rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Resource symlinks are not supported: {path}")
        if path.is_file():
            files.append(path)
    if (root / "main.pak").is_symlink() or (root / "properties").is_symlink():
        raise ValueError("Resource roots must not be symlinks")
    archive.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".pvz-pack-", dir=archive.parent) as work:
        temporary = Path(work) / "resources.zip"
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
            for path in files:
                if path.resolve() == archive.resolve():
                    raise ValueError("The archive must be outside the resource files being packed")
                bundle.write(path, path.relative_to(root).as_posix())
        os.replace(temporary, archive)
    print(f"Packed {len(files)} files into {archive} ({archive.stat().st_size / 1048576:.2f} MiB)")


def unpack(root, archive):
    if not archive.is_file():
        raise ValueError(f"Resource archive does not exist: {archive}")
    root.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        entries = []
        seen = set()
        for info in bundle.infolist():
            relative = resource_path(info.filename)
            if relative in seen:
                raise ValueError(f"Duplicate resource path: {info.filename}")
            seen.add(relative)
            kind = stat.S_IFMT(info.external_attr >> 16)
            if kind not in (0, stat.S_IFREG, stat.S_IFDIR):
                raise ValueError(f"Unsupported archive entry: {info.filename}")
            target = root / relative
            # Reject symlink parents, including links that point within the root.
            if any(path.is_symlink() for path in (target, *target.parents) if path == root or root in path.parents):
                raise ValueError(f"Symlink in extraction path: {info.filename}")
            if info.is_dir():
                continue
            if target.exists() and not target.is_file():
                raise ValueError(f"Resource path is not a regular file: {target}")
            entries.append((info, relative))
        if not any(relative == Path("main.pak") for _, relative in entries) or not any(
            relative.parts[0] == "properties" for _, relative in entries
        ):
            raise ValueError("The ZIP must contain main.pak and properties/ at its root")
        missing = [(info, relative) for info, relative in entries if not (root / relative).exists()]
        if not missing:
            return
        # Read and verify all missing files before installing any of them.
        with tempfile.TemporaryDirectory(prefix=".pvz-unpack-", dir=root) as work:
            staged = Path(work)
            for info, relative in missing:
                target = staged / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(info) as source, target.open("wb") as output:
                    shutil.copyfileobj(source, output)
            for _, relative in missing:
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                # Exclusive creation preserves files that appeared since validation.
                try:
                    output = target.open("xb")
                except FileExistsError:
                    continue
                try:
                    with output, (staged / relative).open("rb") as source:
                        shutil.copyfileobj(source, output)
                except BaseException:
                    target.unlink()
                    raise
        print(f"Restored {len(missing)} missing resource files into {root}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("pack", "unpack"))
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--archive", type=Path, default=ROOT / "game-resources.zip")
    args = parser.parse_args()
    try:
        (pack if args.action == "pack" else unpack)(args.root.resolve(), args.archive.resolve())
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        parser.exit(1, f"Resource bundle error: {error}\n")


if __name__ == "__main__":
    main()
