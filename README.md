# neutron

## Building
CMake and a LLVM build are needed. You need to provide the path to the 
**llvm-config[.exe]** binary via **-DLLVM_CONFIG_PATH=...** in the command line. 
It usually lives in llvm-root-dir/bin
```
git clone https://github.com/alexvitkov/neutron
mkdir neutron/build
cd    neutron/build

cmake -DLLVM_CONFIG_PATH=/path/to/llvm/bin/llvm-config ..
cmake --build .
```

Precompiled LLVM binaries are provided for Windows 10 x64. The only target this build supports is X86.

<https://drive.google.com/file/d/13MR7SfBGOgTd3C6sdp9iWaL_lQ6T_S7D/view?usp=sharing> (1.4G)

On Linux you can probably get away with whatever LLVM your distro's package manager gives you.
I've only tested LLVM 11.0.0 but older recent-ish versions should be OK.
If you're using the package manager's LLVM, then llvm-config should be in your $PATH,
this should get you going:
```Bash
cmake -DLLVM_CONFIG_PATH=$(which llvm-config) ..
```

## Command line flags
-   -a - print out the AST
-   -t - print out the TIR
-   -l - print out the LLVM IR
-   -j - print out debug info about the jobs
-   -e - execute the main function's bytecode
-   -o - output filename. if it ends in '.o', no linking will be performed
