#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AXSLC_BUILD_DIR="${SCRIPT_DIR}/build"
SYMLINK_DIR="${HOME}/.local/bin"

print_usage() {
	cat <<EOF
Usage: $(basename "$0") [options]

Install the axslc CLI tool.

Options:
  --release       Build in Release mode (default: Debug)
  --prefix <dir>  Symlink destination directory (default: ~/.local/bin)
  -h, --help      Show this help message
EOF
}

BUILD_TYPE="Debug"

while [[ $# -gt 0 ]]; do
	case "$1" in
	--release)
		BUILD_TYPE="Release"
		shift
		;;
	--prefix)
		SYMLINK_DIR="$2"
		shift 2
		;;
	-h | --help)
		print_usage
		exit 0
		;;
	*)
		echo "Unknown option: $1" >&2
		print_usage
		exit 1
		;;
	esac
done

echo "Building axslc (${BUILD_TYPE})..."

cmake -S "${SCRIPT_DIR}" -B "${AXSLC_BUILD_DIR}" \
	-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
	>/dev/null 2>&1

cmake --build "${AXSLC_BUILD_DIR}" --target axslc -j"$(nproc)"

AXSLC_BIN="${AXSLC_BUILD_DIR}/axslc"

if [[ ! -f "${AXSLC_BIN}" ]]; then
	echo "Error: axslc binary not found at ${AXSLC_BIN} after build" >&2
	exit 1
fi

mkdir -p "${SYMLINK_DIR}"
rm -f "${SYMLINK_DIR}/axslc"
ln -sf "${AXSLC_BIN}" "${SYMLINK_DIR}/axslc"

echo "axslc installed successfully."
echo "  Binary:  ${AXSLC_BIN}"
echo "  Symlink: ${SYMLINK_DIR}/axslc"

if ! echo "${PATH}" | tr ':' '\n' | grep -qx "${SYMLINK_DIR}"; then
	echo ""
	echo "Note: ${SYMLINK_DIR} is not in your PATH."
	echo "Add it with: export PATH=\"${SYMLINK_DIR}:\${PATH}\""
fi
