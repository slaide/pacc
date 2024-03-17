import subprocess as sp, typing as tp
import shutil, argparse, platform
from pathlib import Path
from enum import Enum
from dataclasses import dataclass

def remove(path:tp.Union[str,Path]):
    """ remove file/symlink/directory from filesystem"""
    path=Path(path)
    
    if path.is_file() or path.is_symlink():
        # print(f"removing file {str(path)}")
        path.unlink() # remove file/symlink
    elif path.is_dir():
        # print(f"removing directory {str(path)}")
        shutil.rmtree(path) # remove dir and all contains
    else:
        raise ValueError(f"file {str(path)} is not a file/symlink or dir")

class InputVariant(str,Enum):
    None_="none",
    Manual="manual",
class OutputVariant(str,Enum):
    None_="none",
    Manual="manual",
    Generated="generated",

class CommandInstance:
    def __init__(self,
        cmd:"Command",
        
        input:tp.Optional[str]=None,
        output:tp.Optional[str]=None,
        
        depends:["Self"]=[]
    ):
        self.cmd=cmd
        
        self.input=input
        self.output=output
        
        self.depends=depends
        
        self.executed=False

    def get(self):
        """
            execute this command (and run all its dependencies before the command itself is run)
        """
        
        if not self.executed:
            self.cmd.run(input=self.input,output=self.output,depends=self.depends)
            self.executed=True
            
        return self.cmd.get_output(self.output)


class Command:
    def __init__(self,
        cmd:str,
        input:InputVariant="none",
        output:OutputVariant="none",
        output_generator:tp.Optional[callable]=None,
        dependencies:[CommandInstance]=[],
        overwrite:bool=True,
        phony:bool=False,
    ):
        """
            create command template

            arguments:
                cmd:
                    command template, in shell syntax. will have inputs substituted for {input} and output for {output}
                    
                input:
                    define input variant this command accepts. can be
                        - none : no input argument
                        - manual : input argument needs to be provided in instantiation
                output:
                    define output variant for this command. can be
                        - none : no output
                        - manual : specify output manually in instatiation
                        - generated : generate output through function applied to input

                output_generator:
                    function that takes input as argument and generates output

                dependencies:
                    command dependencies, which all instances of this command depend on

                overwrite:
                    overwrite output file if it already exists (does NOT check file contents)

                phony:
                    pretend this command does not have an output
                
        """
        
        self.cmd_str=cmd
        
        self.input=input
        self.output=output
        
        self.output_generator=output_generator

        if self.output=="generated" and self.output_generator is None:
            raise ValueError("output is configured to be generated, yet no output generator was provided")

        self.overwrite=overwrite
        self.phony=phony
        
        self.dependencies=dependencies

    def __call__(self,
        input:tp.Optional[str]=None,
        output:tp.Optional[str]=None,
        depends:[CommandInstance]=[]
    )->CommandInstance:
        """
            instantiate command

            arguments:
                input:
                    provide input value

                output:
                    output path
                    is used as override when command has an output path generator

                depends:
                    list of command instances required for this command instance to be able to run

            returns:
                handle to command instance which can be requested for later execution
        """

        # verify input
        
        if input is None and self.input!="none":
            raise RuntimeError("input missing")

        # verify output

        if self.output=="none":
            if output is not None:
                raise ValueError("command has no output, yet an output was provided")

        elif self.output=="manual":
            if output is None:
                raise ValueError("command requires output argument, yet no output was provided")

        elif self.output=="generated":
            output=self.output_generator(input)

        return CommandInstance(self,input=input,output=output,depends=depends)

    def run(self,
        input:tp.Optional[str]=None,
        output:tp.Optional[str]=None,
        depends:[CommandInstance]=[]
    )->tp.Optional[str]:
        """
            execute an instance of this command

            arguments:
                input:
                    input value of a command instance

                output:
                    output value of a command instance

                depends:
                    dependencies of the command instance

            returns:
                path of output, None if the command has no output
        """
        
        inputs=[cmd.get() for cmd in self.dependencies+depends]
        inputs=[str(i) for i in inputs if i is not None]
        
        exec_str_fmt_args={}
        
        if self.input!="none":
            assert input is not None, repr(input)
            input=str(input)
            inputs.append(input)
        
        input_str=" ".join(inputs)

        if self.output!="none":
            output=str(output)
            exec_str_fmt_args["output"]=output

        exec_str_fmt_args["input"]=input_str
        exec_str=self.cmd_str.format(**exec_str_fmt_args)
        
        if output and Path(output).exists():
            if self.overwrite:
                remove(output)
            else:
                return self.get_output(output)

        # print info about running command
        print("running",exec_str)

        # actually run the command
        sp_res=sp.run(exec_str,shell=True,capture_output=True)

        # if command fails, print what the command printed to stdout and stderr
        if sp_res.returncode!=0:
            print("exec failed with",sp_res.returncode)
            if len(sp_res.stdout)>0:
                print("---- stdout begin ----","\n",sp_res.stdout.decode("utf-8"),"---- stdout end --",sep="")
            if len(sp_res.stderr)>0:
                print("---- stderr begin ----","\n",sp_res.stderr.decode("utf-8"),"---- stderr end -- ",sep="")
            raise RuntimeError(f"exec failed with {sp_res.returncode}")
           
        return self.get_output(output)

    def get_output(self,output:tp.Optional[str])->tp.Optional[str]:
        if self.phony:
            return None

        return output

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

clang_comp_str= \
    " clang " \
    " -Wall -Wpedantic -Wextra " \
    " -Wno-sign-compare -Wno-incompatible-pointer-types-discards-qualifiers " \
    " -std=gnu2x -I. -DDEVELOP -Isrc " \
    f" {flags.compiler_flags} " \
    " {input} " \
    " -c " \
    " -o {output}" \

clang_link_str = "clang " \
    " {input} " \
    " -o {output} " \
    f" {flags.linker_flags} "

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
        "src/parser.c",
        "src/file.c",
        "src/tokenizer.c",
        "src/main.c"
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
