Курсов проект по Реализация на езици за програмиране на Александър Витков 


## Building neutron
You need CMake for building both neutron and LLVM.
```
git clone https://github.com/alexvitkov/neutron
cd neutron
mkdir build
cd build
cmake -DLLVM_CONFIG_PATH=C:\LLVM\bin\llvm-config.exe ..
cmake --build .
```
**Note the -DLLVM_CONFIG_PATH=...**
You need to provide the path to `llvm-config` which lives in the 'bin' directory of the LLVM build.


## Precompiled LLVM binaries
Precompiled LLVM binaries are provided for Windows 10 x64. 

The only target this LLVM build supports is X86 and .pdb files are removed to reduce size.

https://drive.google.com/file/d/1HgwBhW5KGRtVyhaSF5612nx_YbPguEtc/view?usp=sharing (1.4G)


## Compiling LLVM from source on Windows
You will need MSVC. The easiest way to get it is to install Visual Studio, and from the Visual Studio Installer select the 'Desktop Development with C++' workload.

Download and extract https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/llvm-11.0.0.src.tar.xz
Enter the 'llvm-11.0.0.src' directory and create a 'build' subdirectory.

### Using Visual Studio / MSBuild
From inside the 'build' directory initialize the CMake project:
```
cmake -DLLVM_TARGETS=X86 ..
```
This will generate a Visual Studio solution for building LLVM.

### Using Ninja
You will have to source one of the `vcvarsall` batch files that come with MSVC in order to initialize the compiler environment variables needed for MSVC. 
If you installed MSVC via Visual Studio, search in your Start Menu for `x64 Native Tools Command Prompt for Visual Studio 2019`. It will open a cmd prompt, from which you can navigate to the LLVM 'build' subdirectory you created earlier.

If you want to use clang for the build, set the following environment variables:
```
set  CC=C:\Program Files\LLVM\bin\clang-cl.exe
set CXX=C:\Program Files\LLVM\bin\clang-cl.exe
```
Then initialize the CMake project and build it:
```
cmake -DLLVM_TARGETS=X86 -G Ninja ..
cmake --build .
```

## Compiling LLVM from source on *nix
This is assuming Ninja is installed. If you want to use a Makefile instead, omit the `-G Ninja` from the cmake command, but the official documentation recommends using Ninja for building LLVM.
```bash
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/llvm-11.0.0.src.tar.xz
tar xf llvm-11.0.0.src.tar.xz
cd llvm-11.0.0.src
mkdir build
cd build
```
If you want to build LLVM with clang instead of GCC, set the CC and CXX environment variables:
```
export  CC=$(which clang)
export CXX=$(which clang++)
```
Then build:
```
cmake -DLLVM_TARGETS=X86 -G Ninja ..
cmake --build .
```


##  Installing LLVM from a Linux package manager
| Distro | Command to install LLVM | LLVM Version |
|--|--|--|
| Arch | `pacman -S llvm` | 11.0 |
| Debian | `apt-get install llvm` | stable - 7.0.0, testing - 11.0 |
| Fedora 32 | `yum install llvm` | 10.0 |





