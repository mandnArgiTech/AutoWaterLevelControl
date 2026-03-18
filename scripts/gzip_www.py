#!/usr/bin/env python3
"""Gzip index.html for LittleFS (smaller flash, faster transfer)."""
import gzip
from pathlib import Path

def main():
    root = Path("data")
    src = root / "index.html"
    if not src.exists():
        print("gzip_www: no data/index.html, skip")
        return
    dst = root / "index.html.gz"
    raw = src.read_bytes()
    with gzip.open(dst, "wb", compresslevel=9) as f:
        f.write(raw)
    out_sz = dst.stat().st_size
    print(f"gzip_www: index.html {len(raw)} -> index.html.gz {out_sz} bytes")

if __name__ == "__main__":
    main()
