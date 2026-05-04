from SCons.Script import DefaultEnvironment
import os
import shutil

env = DefaultEnvironment()

def copy_bins(target, source, env):
    project_dir = env['PROJECT_DIR']
    build_dir = env['BUILD_DIR']
    dst = os.path.join(project_dir, "docs")
    os.makedirs(dst, exist_ok=True)
    files = ["firmware.bin", "bootloader.bin", "partitions.bin"]
    copied = False
    for name in files:
        src = os.path.join(build_dir, name)
        if os.path.isfile(src):
            shutil.copy2(src, dst)
            print("[post-build] Copied {} -> {}".format(src, dst))
            copied = True
    if not copied:
        print("[post-build] No firmware files found to copy in {}".format(build_dir))

# Run after the final firmware binary is written
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_bins)
