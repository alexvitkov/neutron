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
The build system is CMake, and a LLVM build is needed. You need to provide the path to the **llvm-config[.exe]** binary via **-DLLVM_CONFIG_PATH=...** in the command line. llvm-config usually lives in llvm-root-dir/bin
```
git clone https://github.com/alexvitkov/neutron
mkdir neutron/build
cd    neutron/build

cmake -DLLVM_CONFIG_PATH=C:\LLVM\bin\llvm-config.exe ..
cmake --build .
```

Precompiled LLVM binaries are provided for Windows 10 x64. The only target this build supports is X86.

<https://drive.google.com/file/d/13MR7SfBGOgTd3C6sdp9iWaL_lQ6T_S7D/view?usp=sharing> (1.4G)

On Linux you can probably get away with whatever LLVM your distro's package manager gives you, I've only tested with 11.0.0, but older recent-ish versions should be OK.

## Command line flags
-   -d - print debug information (AST, our IR, LLVM IR)
-   -o - output filename. if it ends in '.o', no linking will be performed
