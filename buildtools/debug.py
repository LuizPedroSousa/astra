#!/usr/bin/env python3

import sys
import time
import urllib.request
from pathlib import Path


CHUNK_SIZE = 256 * 1024


def format_rate(bytes_per_second: float) -> str:
    units = ["B/s", "KB/s", "MB/s", "GB/s", "TB/s"]
    rate = max(bytes_per_second, 0.0)
    unit_index = 0

    while rate >= 1024.0 and unit_index < len(units) - 1:
        rate /= 1024.0
        unit_index += 1

    if unit_index == 0:
        return f"{rate:0.0f} {units[unit_index]}"

    return f"{rate:0.1f} {units[unit_index]}"


def format_progress(downloaded: int, total: int | None, bytes_per_second: float) -> str:
    speed = format_rate(bytes_per_second)

    if total is None or total <= 0:
        return f"Downloaded {downloaded:,} bytes at {speed}"

    percent = (downloaded / total) * 100.0
    return f"Downloaded {downloaded:,} / {total:,} bytes ({percent:0.1f}%) at {speed}"


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: debug.py <url> <output>", file=sys.stderr)
        return 2

    url = sys.argv[1]
    output_path = Path(sys.argv[2])
    temp_path = output_path.with_suffix(output_path.suffix + ".part")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path.unlink(missing_ok=True)

    try:
        request = urllib.request.Request(
            url,
            headers={"User-Agent": "astra-sandbox-assets/1.0"},
        )

        with urllib.request.urlopen(request) as response, temp_path.open("wb") as output:
            total_header = response.headers.get("Content-Length")
            total_bytes = int(total_header) if total_header and total_header.isdigit() else None
            downloaded_bytes = 0
            started_at = time.monotonic()

            if total_bytes is not None:
                print(f"Remote size: {total_bytes:,} bytes", flush=True)

            while True:
                chunk = response.read(CHUNK_SIZE)
                if not chunk:
                    break

                output.write(chunk)
                downloaded_bytes += len(chunk)
                elapsed = max(time.monotonic() - started_at, 1e-9)
                sys.stdout.write(
                    "\r" + format_progress(downloaded_bytes, total_bytes, downloaded_bytes / elapsed)
                )
                sys.stdout.flush()

            if total_bytes is not None and downloaded_bytes != total_bytes:
                raise RuntimeError(
                    f"download size mismatch: got {downloaded_bytes} bytes, expected {total_bytes}"
                )

        output_path.unlink(missing_ok=True)
        temp_path.replace(output_path)
    except Exception as exc:
        temp_path.unlink(missing_ok=True)
        print(f"\nDownload failed: {exc}", file=sys.stderr)
        return 1

    sys.stdout.write("\n")
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
