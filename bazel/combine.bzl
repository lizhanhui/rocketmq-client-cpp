load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")
 
def _combine_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)    
    target_list = []
    for dep_target in ctx.attr.deps:        
        # CcInfo, InstrumentedFilesInfo, OutputGroupInfo   
        # https://docs.bazel.build/versions/3.3.0/skylark/lib/LinkingContext.html   
        cc_info_linker_inputs = dep_target[CcInfo].linking_context.linker_inputs
        target_dirname_list = []
        local_repo_path = "src/main/cpp"
        for linker_in in cc_info_linker_inputs.to_list():            
            for linker_in_lib in linker_in.libraries:                
                if linker_in_lib.pic_static_library != None:
                    if (linker_in_lib.pic_static_library.short_path.startswith(local_repo_path)):
                        target_list.append(linker_in_lib.pic_static_library)                  
                if linker_in_lib.static_library != None:
                    if (linker_in_lib.pic_static_library.short_path.startswith(local_repo_path)):
                        target_list.append(linker_in_lib.static_library)
    
    output = ctx.outputs.output
    if ctx.attr.link_static:
        cp_command  = ""       
        processed_list = []
        processed_path_list = []
        for dep in target_list:
            cp_command += "cp -a " + dep.path + " " + output.dirname + "/ && "
            print("Static library: %s" % dep.path)
            processed = ctx.actions.declare_file(dep.basename)
            processed_list.append(processed)
            processed_path_list.append(dep.path)
        cp_command += "echo 'starting to run shell'"
        processed_path_list.append(output.path)
  
        ctx.actions.run_shell(
            outputs = processed_list,
            inputs = target_list,
            command = cp_command,
        )
 
        command = "cd {} && ar -x {} {}".format(
                output.dirname,
                " && ar -x ".join([dep.basename for dep in target_list]),
                " && ar -rcs {} *.o".format(output.basename)
            )
        #print("command = '{}'".format(command))
        ctx.actions.run_shell(
            outputs = [output],
            inputs = processed_list,
            command = command,
        )
    else:
        #TODO: Mac OS does not support -Wl,--whole-archive option
        # Mac: -Wl,-all_load {} -Wl,-noall_load 
        # Linux: -Wl,--whole-archive {} -Wl,--no-whole-archive 
        command = "export PATH=$PATH:{} && {} -shared -fPIC -Wl,--whole-archive {} -Wl,--no-whole-archive -Wl,-soname -o {}".format(
            cc_toolchain.ld_executable,
            cc_toolchain.compiler_executable,
            " ".join([dep.path for dep in target_list]),
            output.path)
        #print("command = {}".format(command))
        ctx.actions.run_shell(
            outputs = [output],
            inputs = target_list,
            command = command,
        )
 
cc_combine = rule(
    implementation = _combine_impl,
    attrs = {
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
        "link_static" : attr.bool(default = False),
        "deps": attr.label_list(allow_files = [".a"]),
        "output": attr.output()
    },
)