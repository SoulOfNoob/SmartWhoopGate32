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

def after_build(source, target, env):
    print("after_build")
    print("copy: .pio/build/esp32doit-devkit-v1/firmware.bin TO compiled/esp32/firmware.bin")
    copyfile(".pio/build/esp32doit-devkit-v1/firmware.bin", "compiled/esp32/firmware.bin")
    copyfile(".pio/build/esp32doit-devkit-v1/firmware.elf", "compiled/esp32/firmware.elf")

env.AddPostAction("buildprog", after_build)