Курсов проект по Реализация на езици за програмиране на Александър Витков 

## features / roadmap
crossed out entries are in the roadmap, the rest just about works
- primitive types
	- unsigned integers
	- ~~signed integers, floats~~
	- math:
		- operator +, -, *
		- ~~every other operator~~
- variables
	- local
	- global
    - pointers to variables
- functions
	- variadic functions
		- extern variadic functions
		- ~~declaring variadic functions~~
	- extern functions
	- ~~overloading~~
- pointers
	- pointer math
		- pointer + integer, pointer[integer]
		- ~~pointer - pointer~~
- casts
	- implicit casting
		- u8 -> u16 -> u32 -> u64
		- u8 -> i16, u16 -> u32, u32 -> i64
	- explicit casting
		- casting pointer type A* to pointer type B*
		- ~~downcasting (u32 -> u16)~~
- ~~structs~~
- compiler metafeatures
    - spitting .o files
	- linking a callable binary
		- windows with msvc linker on x64, target windows x64
			- link against windows SDK's libc (windows 10 SDK needed)
			- ~~link directly against kernel32.dll~~
		- linux with GNU linker/LLD, target x64 glibc linux
		- ~~any other platform~~


## Building
The build system is CMake and a LLVM build is needed. You need to provide the path to the **llvm-config[.exe]** binary via **-DLLVM_CONFIG_PATH=...** in the command line. llvm-config usually lives in llvm-root-dir/bin
```
git clone https://github.com/alexvitkov/neutron
mkdir neutron/build
cd    neutron/build

cmake -DLLVM_CONFIG_PATH=C:\LLVM\bin\llvm-config.exe ..
cmake --build .
```

## LLVM

### Precompiled LLVM binaries
Precompiled LLVM binaries are provided for Windows 10 x64. The only target this build supports is X86.

https://drive.google.com/file/d/13MR7SfBGOgTd3C6sdp9iWaL_lQ6T_S7D/view?usp=sharing (1.4G)


### Compiling LLVM from on Windows
You will need MSVC. Download and extract https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/llvm-11.0.0.src.tar.xz and create a 'build' subdirectory
#### Using Visual Studio / MSBuild
From inside the 'build' directory initialize the CMake project:
```
cmake -DLLVM_TARGETS=X86 ..
```
This will generate a Visual Studio solution for building LLVM.

#### Using Ninja
You will have to source one of the `vcvarsall` batch files that come with MSVC to initialize the environment variables needed for MSVC. 
Search Start Menu for `x64 Native Tools Command Prompt`. It will open a cmd prompt, from which you can navigate to the LLVM 'build' subdirectory.

If you want to use clang instead of MSVC for the build, set the following. You still need MSVC for the linker & libraries:
```
set  CC=C:\Program Files\LLVM\bin\clang-cl.exe
set CXX=C:\Program Files\LLVM\bin\clang-cl.exe
```
Then initialize the CMake project and build it:
```
cmake -DLLVM_TARGETS=X86 -G Ninja ..
cmake --build .
```

### Compiling LLVM from source on *nix
```bash
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/llvm-11.0.0.src.tar.xz
tar xf llvm-11.0.0.src.tar.xz
cd llvm-11.0.0.src
mkdir build
cd    build
```
If you want to use clang instead of GCC, set CC and CXX:
```
export  CC=$(which clang)
export CXX=$(which clang++)
```
Then build:
```
cmake -DLLVM_TARGETS=X86 -G Ninja ..
cmake --build .
```



