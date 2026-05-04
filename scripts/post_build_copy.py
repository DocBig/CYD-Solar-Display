from SCons.Script import DefaultEnvironment
import os
import shutil

env = DefaultEnvironment()

def copy_bins(target, source, env):
    project_dir = env['PROJECT_DIR']
    # Ensure BUILD_DIR variables are expanded (avoid literal $BUILD_DIR/$PIOENV)
    build_dir = env.subst("$BUILD_DIR")
    dst = os.path.join(project_dir, "docs")
    os.makedirs(dst, exist_ok=True)

    # Copy any .bin files produced in the build directory (robust against naming differences)
    import glob
    bin_files = glob.glob(os.path.join(build_dir, "*.bin"))
    if bin_files:
        for src in bin_files:
            if os.path.isfile(src):
                shutil.copy2(src, dst)
                print("[post-build] Copied {} -> {}".format(src, dst))
    else:
        print("[post-build] No firmware files found to copy in {}".format(build_dir))

# Run after the final firmware binary is written
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_bins)
