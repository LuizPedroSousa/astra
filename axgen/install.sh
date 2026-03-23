#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/builds/axgen/debug"
SYMLINK_DIR="${HOME}/.local/bin"
STREAMS_FORMATS="${ASTRALIX_STREAMS_FORMATS:-Json}"

print_usage() {
	cat <<EOF
Usage: $(basename "$0") [options]

Install the axgen CLI tool.

Options:
  --release       Build in Release mode (default: Debug)
  --formats <list>
                  Semicolon-delimited stream formats for the engine build
                  (default: \$ASTRALIX_STREAMS_FORMATS or Json)
  --prefix <dir>  Wrapper destination directory (default: ~/.local/bin)
  -h, --help      Show this help message
EOF
}

BUILD_TYPE="Debug"

while [[ $# -gt 0 ]]; do
	case "$1" in
	--release)
		BUILD_TYPE="Release"
		BUILD_DIR="${PROJECT_ROOT}/builds/axgen/release"
		shift
		;;
	--formats)
		if [[ $# -lt 2 ]]; then
			echo "Error: --formats requires a value" >&2
			print_usage
			exit 1
		fi
		STREAMS_FORMATS="$2"
		shift 2
		;;
	--prefix)
		if [[ $# -lt 2 ]]; then
			echo "Error: --prefix requires a value" >&2
			print_usage
			exit 1
		fi
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

echo "Building axgen (${BUILD_TYPE})..."
echo "Using stream formats: ${STREAMS_FORMATS}"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
	-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
	-DASTRA_ENABLE_TESTS=OFF \
	-DASTRALIX_STREAMS_FORMATS="${STREAMS_FORMATS}" \
	>/dev/null

cmake --build "${BUILD_DIR}" --target axgen -j"$(nproc)"

AXGEN_BIN="${BUILD_DIR}/axgen/axgen"

if [[ ! -f "${AXGEN_BIN}" ]]; then
	echo "Error: axgen binary not found at ${AXGEN_BIN} after build" >&2
	exit 1
fi

PHYSX_VARIANT="debug"
if [[ "${BUILD_TYPE}" == "Release" ]]; then
	PHYSX_VARIANT="release"
fi
PHYSX_LIB_DIR="${PROJECT_ROOT}/external/PhysX/physx/bin/linux.x86_64/${PHYSX_VARIANT}"

mkdir -p "${SYMLINK_DIR}"
rm -f "${SYMLINK_DIR}/axgen"
cat >"${SYMLINK_DIR}/axgen" <<WRAPPER
#!/usr/bin/env bash
export LD_LIBRARY_PATH="${PHYSX_LIB_DIR}\${LD_LIBRARY_PATH:+:\${LD_LIBRARY_PATH}}"
exec "${AXGEN_BIN}" "\$@"
WRAPPER
chmod +x "${SYMLINK_DIR}/axgen"

echo "axgen installed successfully."
echo "  Binary:  ${AXGEN_BIN}"
echo "  Wrapper: ${SYMLINK_DIR}/axgen"

if ! echo "${PATH}" | tr ':' '\n' | grep -qx "${SYMLINK_DIR}"; then
	echo ""
	echo "Note: ${SYMLINK_DIR} is not in your PATH."
	echo "Add it with: export PATH=\"${SYMLINK_DIR}:\${PATH}\""
fi
