import concurrent.futures as fut
import typing as tp
import sys

from libbuild import *

argparser=ArgParser("build the pacc compiler")
argparser.add(name="--help",short="-h",help="Prints this help message",key="show_help",arg_store_op=ArgStore.presence_flag)
argparser.add(name="--num-threads",short="-j",help="number of compilation threads",key="num_threads",arg_store_op=ArgStore.store_value,default=get_num_cores(),type=int)
argparser.add(name="--cc",help="compiler to use",key="cc",default="clang-17",arg_store_op=ArgStore.store_value)
argparser.add(name="--show-cmds",help="show commands that are run",key="show_cmds",default=False,arg_store_op=ArgStore.presence_flag)
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

CC_CMD=f"{args.get('cc')} -std=c2x -I./include"

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

Command.pool=fut.ThreadPoolExecutor(max_workers=args.get("num_threads"))
Command.show_cmds=args.get("show_cmds") # type: ignore
Command.cmd_cache=CacheManager()

if __name__=="__main__":
    build_dir=Mkdir("build")
    bin_dir=Mkdir("bin")

    cmd_all=Nop()

    final_bin=Link("bin/main",objs=[CompileFile(f,f"build/{f.replace('/','__').replace('.c','.o')}").depends(build_dir) for f in file_paths])
    final_bin.depends(bin_dir)

    cmd_all.depends(final_bin)
    cmd_all.depends(CompileShader("src/shader.frag","frag.spv"))

    Command.build(cmd_all)

    clean_target=Nop().depends(Remove("build")).depends(Remove("bin"))

Command.pool.shutdown()

if Command.cmd_cache is not None:
    Command.cmd_cache.flush()
