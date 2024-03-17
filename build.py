import subprocess as sp, typing as tp
from pathlib import Path

class CommandInstance:
    def __init__(self,cmd:"Command",input:str=None,output:str=None,depends:["Self"]=[]):
        self.cmd=cmd
        self.input=input
        if self.cmd.has_output and output is None:
            raise ValueError("output missing")
        elif not self.cmd.has_output and output is not None:
            raise ValueError("output erroneously provided")
        self.output=output
        self.depends=depends
        self.output=output
        self.executed=False

    def get(self):
        if not self.executed:
            self.output=self.cmd.run(input=self.input,output=self.output,depends=self.depends)
            
        return self.output


class Command:
    def __init__(self,cmd:str,has_input:bool,output_generator:callable=None):
        self.cmd_str=cmd
        self.has_input=has_input

        self.output_generator=output_generator
        self.has_output=self.output_generator is None

    def __call__(self,input=None,output=None,depends:[CommandInstance]=[])->CommandInstance:
        if input is None and self.has_input:
            raise RuntimeError("input missing")

        return CommandInstance(self,input=input,output=output,depends=depends)

    def run(self,input=None,output=None,depends:[CommandInstance]=[]):
        inputs=[str(cmd.get()) for cmd in depends]

        exec_str=self.cmd_str
        input_str=" ".join(inputs)
        exec_str_fmt_args={}
        output_gen_args=[]
        if self.has_input is not None and self.has_input:
            input=str(input)
            
            input_str+=" "+input
            output_gen_args.append(input)

        if not self.has_output:
            output=self.output_generator(*output_gen_args)

        output=str(output)
        exec_str_fmt_args["output"]=output

        exec_str_fmt_args["input"]=input_str
        exec_str=exec_str.format(**exec_str_fmt_args)
            
        print("running",exec_str)

        sp_res=sp.run(exec_str,shell=True,capture_output=True)

        if sp_res.returncode!=0:
            print("exec failed with",sp_res.returncode)
            if len(sp_res.stdout)>0:
                print("---- stdout begin ----","\n",sp_res.stdout.decode("utf-8"),"---- stdout end --",sep="")
            if len(sp_res.stderr)>0:
                print("---- stderr begin ----","\n",sp_res.stderr.decode("utf-8"),"---- stderr end -- ",sep="")
            raise RuntimeError(f"exec failed with {sp_res.returncode}")


        return output

clang_comp_str= \
    "clang " \
    "-Wall -Wpedantic -Wextra " \
    "-Wno-sign-compare -Wno-incompatible-pointer-types-discards-qualifiers " \
    "-std=gnu2x -I. -DDEVELOP -Isrc " \
    "{input} " \
    "-c " \
    "-o {output}" \


clang_link_str= "clang {input} -o {output}"
    
cc=Command(cmd=clang_comp_str,
    has_input=True,
    output_generator=lambda i:f"build/{Path(i).stem}.o"
)
link=Command(cmd=clang_link_str,has_input=False)


def main():
    obj_files=[cc(f) for f in [
        "src/parser.c",
        "src/file.c",
        "src/tokenizer.c",
        "src/main.c"
    ]]

    app=link(depends=obj_files,output="bin/main")
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
