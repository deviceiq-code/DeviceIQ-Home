# scripts/mkdevpkg_post.py
import os, shutil, subprocess
from SCons.Script import Import
Import("env")

BUILD_DIR = env.subst("$BUILD_DIR")
PIOENV    = env.subst("$PIOENV")

fs_name = env.GetProjectOption("board_build.filesystem", "spiffs")
fw_bin  = os.path.join(BUILD_DIR, env.subst("${PROGNAME}.bin"))
fs_bin  = os.path.join(BUILD_DIR, f"{fs_name}.bin")

tool     = env.GetProjectOption("custom_mkdevpkg", "mkdevpkg")
tool_exe = shutil.which(tool) or tool

overwrite_flag = env.GetProjectOption("custom_mkdevpkg_overwrite", "no").lower() in ("1","true","yes","on")
out_name = env.GetProjectOption("custom_dpk_name", f"{PIOENV}.dpk")
out_dpk  = os.path.join(BUILD_DIR, env.subst(out_name))

def log(msg): print(f"[mkdevpkg] {msg}")

def try_build_dpk(target=None, source=None, env=None, **kwargs):
    print(f"")
    
    if not os.path.exists(fw_bin):
        log(f"Firmware not found: {fw_bin}")
        return
    if not os.path.exists(fs_bin):
        log(f"Filesystem not found: {fs_bin}")
        return

    cmd = [tool_exe, fw_bin, fs_bin, out_dpk]
    if overwrite_flag:
        cmd.append("--rewrite")

    log(f"Generating {out_dpk}")
    subprocess.check_call(cmd)
    log("Ok!")

env.AddPostAction(fw_bin, try_build_dpk)
env.AddPostAction(fs_bin, try_build_dpk)
