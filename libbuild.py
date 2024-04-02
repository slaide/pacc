if __name__=="__main__":
    print("error - this file is not meant to be run directly")
    exit(1)

import subprocess as sp, typing as tp
import shutil, shlex, json
from pathlib import Path
from enum import Enum

build_database_filepath=Path(".build")
if build_database_filepath.exists():
    build_database=json.parse(build_database_filepath.open("r"))
else:
    build_database={}

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
        dependencies:tp.List[CommandInstance]=[],
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
        depends:tp.List[CommandInstance]=[]
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
        depends:tp.List[CommandInstance]=[]
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
        exec_args=shlex.split(exec_str)
        print(shlex.join(exec_args))

        # actually run the command
        sp_res=sp.run(exec_args,capture_output=True)

        # if command fails, print what the command printed to stdout and stderr
        if sp_res.returncode!=0:
            print("exec failed with",sp_res.returncode)
            if len(sp_res.stdout)>0:
                print("---- stdout begin ----","\n",sp_res.stdout.decode("utf-8"),"---- stdout end --",sep="")
            if len(sp_res.stderr)>0:
                print("---- stderr begin ----","\n",sp_res.stderr.decode("utf-8"),"---- stderr end -- ",sep="")
            print(f"exec failed with {sp_res.returncode}")
            exit(sp_res.returncode)
           
        return self.get_output(output)

    def get_output(self,output:tp.Optional[str])->tp.Optional[str]:
        if self.phony:
            return None

        return output
