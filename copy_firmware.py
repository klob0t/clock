import shutil
import os

Import("env")

def after_build(source, target, env):
    bin_source = str(target[0])

    file_name = "firmware.hex"
    target_path = os.path.join(env.subst("$PROJECT_DIR"), file_name)

    shutil.copyfile(bin_source, target_path)
    print(f"-----------------------------------------")
    print(f"SUCCESS: Firmware copied to {target_path}")
    print(f"-----------------------------------------")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", after_build)