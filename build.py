#!/usr/bin/env python3

import argparse, platform
from dataclasses import dataclass
from pathlib import Path

from libbuild import Command

arg_parser=argparse.ArgumentParser(
    prog="build.py",
    description="build script for the pacc c23 compiler",
)

build_targets=dict(
    all=None,
)
default_build_target="all"

arg_parser.add_argument(
    "target",
    choices=build_targets.keys(),
    default=default_build_target,
    help="target to build",
    nargs="?")

build_platforms=dict(
    linux_amd64=None,
    macOS_arm64=None,
)
if platform.system()=="Linux":
    assert platform.machine()=="x86_64", f"unsupported architecture {platform.machine()}"
    default_build_platform="linux_amd64"
elif platform.system()=="Darwin":
    assert platform.machine()=="arm64", f"unsupported architecture {platform.machine()}"
    default_build_platform="macOS_arm64"
else:
    raise RuntimeError(f"unsupported platform: os = {platform.system()} , arch = {platform.machine()}")

arg_parser.add_argument(
    "-p","--platform",
    choices=build_platforms.keys(),
    default=default_build_platform,
    help="os/arch platform to build for",
    nargs="?")

@dataclass
class BuildModeFlags:
    compiler_flags:str
    linker_flags:str
    
build_mode_flags=dict(
    debug = BuildModeFlags("-O0 -g","-g"),
    debugRelease = BuildModeFlags("-O2 -g","-g"),
    release = BuildModeFlags("-O3","-s -flto=full"),
)
default_build_mode="debug"

arg_parser.add_argument(
    "-m","--mode",
    choices=build_mode_flags.keys(),
    default=default_build_mode,
    help="build mode (i.e. optimization level)",
    nargs="?")

cli_args,unknown_args=arg_parser.parse_known_args()

assert len(unknown_args)==0, f"unknown arguments passed: {unknown_args}"

flags=build_mode_flags[cli_args.mode]

compiler:str="clang"
warn_flags=" ".join([f" -W{wf} " for wf in [
    "all",
    "pedantic",
    "extra",
    "no-sign-compare",
    "no-incompatible-pointer-types-discards-qualifiers",
]])
clang_comp_str=f"""
    {compiler}
    {warn_flags}
    -std=gnu2x
    -Iinclude
    -DDEVELOP
    {flags.compiler_flags}
    -c
    -o {{output}}
    {{input}}
""".replace("\n"," ")

clang_link_str = f"""
    {compiler}
    {flags.linker_flags}
    -o {{output}}
    {{input}}
""".replace("\n"," ")

mkdir=Command(cmd="mkdir {output}",output="manual",overwrite=False,phony=True)

cc=Command(cmd=clang_comp_str,
    input="manual",
    dependencies=[mkdir(output="build")],
    output="generated",
    output_generator=lambda i:f"build/{Path(i).stem}.o"
)
link=Command(cmd=clang_link_str,output="manual")


def main():
    obj_files=[cc(f) for f in [
        "src/util/array.c",
        "src/util/util.c",
        "src/parser.c",
        "src/file.c",
        "src/tokenizer.c",
        "src/main.c",
    ]]

    app=link(depends=obj_files+[mkdir(output="bin")], output="bin/main")
    app.get()

try:
    main()
except Exception as e:
    print(e)
    print("building failed. aborting.")

"""

.PHONY: all
all: bin/main

.PHONY: run
run: all
	./bin/main src/main.c

.PHONY: clean
clean:
	rm -f bin/main build/*

.PHONY: test
test: all
	@bash test/run.sh
"""
