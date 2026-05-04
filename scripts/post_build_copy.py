from SCons.Script import DefaultEnvironment
import os
import shutil
import glob

env = DefaultEnvironment()


def _create_combined_image(build_dir, dst, mapping):
    """Write a single firmware.bin at dst with parts placed at given offsets.
    mapping: list of (filename, offset) relative to build_dir
    Returns path to created file or None.
    """
    parts = []
    max_end = 0
    for name, off in mapping:
        src = os.path.join(build_dir, name)
        if os.path.isfile(src):
            data = open(src, "rb").read()
            parts.append((off, data))
            max_end = max(max_end, off + len(data))
        else:
            return None

    os.makedirs(dst, exist_ok=True)
    out = os.path.join(dst, "firmware.bin")
    # Create sparse file of required size and write parts
    with open(out, "wb") as f:
        f.truncate(max_end)
    with open(out, "r+b") as f:
        for off, data in parts:
            f.seek(off)
            f.write(data)
    print("[post-build] Wrote combined image {} size={}".format(out, os.path.getsize(out)))
    return out


def copy_bins(target, source, env):
    project_dir = env['PROJECT_DIR']
    build_dir = env.subst("$BUILD_DIR")
    dst = os.path.join(project_dir, "docs")
    os.makedirs(dst, exist_ok=True)

    # Prefer creating a combined 0x0 image (bootloader/partitions/app)
    mapping = [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000), ("firmware.bin", 0x10000)]
    combined = _create_combined_image(build_dir, dst, mapping)
    if combined:
        # Combined image written as docs/firmware.bin; do not copy other .bin files
        return

    # Fallback: if only firmware.bin exists, copy it (still named firmware.bin in docs)
    src_fw = os.path.join(build_dir, "firmware.bin")
    if os.path.isfile(src_fw):
        shutil.copy2(src_fw, dst)
        print("[post-build] Copied firmware {} -> {}".format(src_fw, dst))
        return

    print("[post-build] No firmware files found to copy in {}".format(build_dir))


# Run after the final firmware binary is written
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_bins)
