#!/usr/bin/env python3

import argparse, platform
from dataclasses import dataclass
from pathlib import Path
from enum import Enum

from libbuild import Command, InputVariant, OutputVariant

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

class BuildPlatform(str,Enum):
    LINUX_AMD64="linux_amd64"
    LINUX_ARM64="linux_arm64"
    MACOS_AMD64="macos_amd64"
    MACOS_ARM64="macos_arm64"
    WINDOWS_AMD64="windows_amd64"
    WINDOWS_ARM64="windows_arm64"

match (platform.system(),platform.machine().lower()):
    case ("Linux","x86_64") | ("Linux","amd64"):
        default_build_platform=BuildPlatform.LINUX_AMD64
    case ("Linux","aarch64") | ("Linux","arm64"):
        default_build_platform=BuildPlatform.LINUX_ARM64
    case ("Darwin","x86_64") | ("Darwin","amd64"):
        default_build_platform=BuildPlatform.MACOS_AMD64
    case ("Darwin","arm64"):
        default_build_platform=BuildPlatform.MACOS_ARM64
    case ("Windows","amd64") | ("Windows","x86_64"):
        default_build_platform=BuildPlatform.WINDOWS_AMD64
    case ("Windows","arm64"):
        default_build_platform=BuildPlatform.WINDOWS_ARM64
    case (sys_os,sys_arch):
        raise RuntimeError(f"unsupported platform: os = {sys_os} , arch = {sys_arch}")

arg_parser.add_argument(
    "-p","--platform",
    choices=[b.value for b in BuildPlatform],
    default=default_build_platform.value,
    help="os/arch platform to build for",
    nargs="?")

arg_parser.add_argument(
    "--cc",
    default="clang",
    help="c compiler path"
)

@dataclass
class BuildModeFlags:
    compiler_flags:str
    linker_flags:str
    
build_mode_flags=dict(
    debug = BuildModeFlags("-O0 -g",""),
    debugRelease = BuildModeFlags("-O2 -g",""),
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

compiler:str=cli_args.cc
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

mkdir=Command(cmd="mkdir {output}",output=OutputVariant.Manual,overwrite=False,phony=True)

cc=Command(cmd=clang_comp_str,
    input=InputVariant.Manual,
    dependencies=[mkdir(output="build")],
    output=OutputVariant.Generated,
    output_generator=lambda i:f"build/{Path(i).stem}.o" if i else "build/ERROR_output_generator.o"
)
link=Command(cmd=clang_link_str,output=OutputVariant.Manual)

# get build dependencies from #inclues : clang -Iwhatever -std=c2x -M -MT main.exe -MF main.d main.c

if __name__=="__main__":
    try:
        obj_files=[cc(f) for f in [
            "src/util/array.c",
            "src/util/util.c",

            "src/parser/parser.c",
            "src/parser/statement.c",
            "src/parser/symbol.c",
            "src/parser/type.c",
            "src/parser/value.c",
            "src/parser/module.c",

            "src/preprocessor/preprocessor.c",

            "src/file.c",
            "src/tokenizer.c",
            "src/main.c",
        ]]

        app=link(depends=obj_files+[mkdir(output="bin")], output="bin/main")
        app.get()
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
