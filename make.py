#!/usr/bin/env python3

import concurrent.futures as fut
import typing as tp
import sys

from libbuild import *

argparser=ArgParser("build the pacc compiler")

argparser.add(name="--target",short="-t",help="target to build",key="build_target",arg_store_op=ArgStore.store_value,default="all",options=["all","clean","test_target"])
argparser.add(name="--num-threads",short="-j",help="number of compilation threads",key="num_threads",arg_store_op=ArgStore.store_value,default=1,type=int)
argparser.add(name="--cc",help="compiler to use",key="cc",default="clang-17",arg_store_op=ArgStore.store_value)
argparser.add(name="--print-cmds",help="print commands that are run",key="show_cmds",default=False,arg_store_op=ArgStore.presence_flag)
argparser.add(name="--force-rebuild",short="-f",help="force rebuild, i.e. do not read cache, but still build it",key="force_rebuild",arg_store_op=ArgStore.presence_flag)

argparser.add(name="--help",short="-h",help="Prints this help message",key="show_help",arg_store_op=ArgStore.presence_flag)

args=argparser.parse(sys.argv[1:])

if args.get("show_help",False):
    argparser.print_help()
    exit(0)

file_paths=[
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
]

CC_CMD=f"{args.get('cc')} -g -O0 -std=c2x -I./include -lvulkan -lxcb -lxcb-util -lxcb-randr"

class CompileFile(Command):
    " compile a single file "
    def __init__(self,file:str,out:str):
        self.file=file
        self.out=out
        super().__init__(cmd=f"{CC_CMD} -c -o {self.out} {self.file}",in_files=[file],out_files=[out])

    # override return argument for compatibility with Link
    def depends(self,*other_args:"Command")->"CompileFile":
        super().depends(*other_args)
        return self

class Link(Command):
    " link a set of object files "
    def __init__(self,out:str,objs:tp.List[CompileFile]=[]):
        obj_out_files=[d.out for d in objs]
        super().__init__(cmd="",in_files=obj_out_files,out_files=[out])

        self.out=out
        for d in objs:
            self.depends(d)

        deps_str=" ".join(obj_out_files)
        self.cmd=f"{CC_CMD} -o {self.out} {deps_str}"

class Mkdir(Command):
    " create a directory "
    def __init__(self,dirname:str):
        super().__init__(cmd=f"mkdir -p {dirname}",phony=True)

class CompileShader(Command):
    " compile a glsl shader to spirv "
    def __init__(self,shader:str,output:str):
        super().__init__(cmd=f"glslangValidator -V {shader} -o {output}",in_files=[shader],out_files=[output])

class Nop(Command):
    " does nothing by itself, can be used to create a dependency chain "

    def __init__(self):
        super().__init__(cmd="",phony=True)

class Remove(Command):
    " remove a file "
    def __init__(self,file:str):
        super().__init__(cmd=f"rm -rf {file}",phony=True)

class AnyCmd(Command):
    " run any command "
    def __init__(self,cmd:str):
        super().__init__(cmd=cmd,phony=True,shell=True)

Command.pool=fut.ThreadPoolExecutor(max_workers=args.get("num_threads"))
Command.show_cmds=args.get("show_cmds") # type: ignore
Command.cmd_cache=CacheManager(rebuild=args.get("force_rebuild")) # type: ignore

if __name__=="__main__":
    build_dir=Mkdir("build")
    bin_dir=Mkdir("bin")

    cmd_all=Nop()

    objs=[
        CompileFile(f,f"build/{f.replace('/','__').replace('.c','.o')}") \
            .depends(build_dir)
        for f in file_paths
    ]
    final_bin=Link("bin/main",objs=objs).depends(bin_dir)

    cmd_all.depends(final_bin,CompileShader("src/shader.frag","frag.spv"))

    clean_target=Nop().depends(Remove("build"),Remove("bin"))

    build_target=args.get("build_target")
    print(f"running target: {build_target}")
    match build_target:
        case "all":
            Command.build(cmd_all)
        case "clean":
            Command.build(clean_target) 
        # for debugging purposes, build only the test target (which may be changed to any other file)
        # this mostly serves to store the command somewhere
        case "test_target":
            Command.build(AnyCmd("bin/main -I$(pwd)/musl/include test/test014.c -p ; exit 1").depends(final_bin))

    print("done")
