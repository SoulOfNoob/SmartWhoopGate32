from shutil import copyfile
# copyfile('.pio/build/esp32doit-devkit-v1/firmware.bin', 'compiled/firmware.bin')

Import("env", "projenv")

# access to global build environment
print(env)

# access to project build environment (is used source files in "src" folder)
print(projenv)

#
# Dump build environment (for debug purpose)
# print(env.Dump())
#

#
# Change build flags in runtime
#
env.ProcessUnFlags("-DVECT_TAB_ADDR")
env.Append(CPPDEFINES=("VECT_TAB_ADDR", 0x123456789))

# copy compiled files to ota folder for webdeploy
def copy_bin(source, target, env):
    print("after_build")
    print("copy: .pio/build/esp32doit-devkit-v1/firmware.bin TO ota/esp32/firmware.bin")
    copyfile(".pio/build/esp32doit-devkit-v1/firmware.bin", "ota/esp32/firmware.bin")
    copyfile(".pio/build/esp32doit-devkit-v1/firmware.elf", "ota/esp32/firmware.elf")

env.AddPostAction("buildprog", copy_bin)
env.AddPostAction("upload", copy_bin)