# Building LLVM and creating / running our own Pass

## __General Overview__
This repository is essentially an in depth guide on how to create an LLVM Pass and also how to build LLVM as well as your pass in your machine. The pass in this guide is run on LLVM version 6, however it should work in later and up to date versions such as 9 or 10 as well. Getting familiar with LLVM internals and Passes can be a quite tedious task so I decided to create this guide presenting to the best of my ability and in as much detail as possible all of the things I learned trying to create MemLog Pass. MemLog was done as part of an assignment on [Type Systems and Programming Languages](https://www.csd.uoc.gr/~hy546/) course of University of Crete, Computer Science Department.

## __Pass Details__
The Readme you are currently viewing contains mostly instructions on how to build LLVM from scratch. If you are interested on the Pass code, please refer to this: [Memlog Pass](llvm/lib/MemLog/) which contains precise instructions on how MemLog pass works and useful tips and tricks on how to write a Pass in general.

## __Section A: Building LLVM__

### __Setting up the Environment__
For this project, LLVM 6 version was used, cloned from : _https://github.com/llvm/llvm-project.git_ .
Building LLVM can be a really memory intensive procedure so I had to create a swapfile since the 8GB physical memory that my laptop has was not enough.

```
fallocate -l 8G /swapfile
sudo chmod 600 swapfile
sudo mkswap swapfile
sudo swapon swapfile
```
Initial build with default ld failed due to massive RAM hogging on linking, thus ld.gold was used successfully.

An easy and fast way to switch ld and only for this build is by creating a dummy file, symlinking it to ld.gold and prepending it to your PATH:

```
touch ld
ln -s /usr/bin/ld.gold ld
PATH=`pwd`:$PATH
```
 __*Important note*__ : If you build LLVM with ld.gold then when developing your pass and you have to run make again for it to be recompiled you have to use ld.gold again or else be prepared to rebuild LLVM from ground up.


### __Downloading and Building LLVM__
For the rest of the guide lets assume that we are have folder called "Workspace" in:  __/home/$USER__

```
cd
mkdir workspace
cd workspace
```

Go ahead and clone LLVM:

```
git clone https://github.com/llvm/llvm-project.git -b release/6.x
```
_Note:_ " 6.x " at the end does NOT mean any " 6. version " but it is in fact a release ID.

Once LLVM has been cloned we can then build it:
```
cd llvm-project
mkdir build
cd build
cmake -DLLVM_ENABLE_PROJECTS=clang -G "Unix Makefiles" ../llvm
make -j<X>
```
_Note:_ cmake also runs configure automatically. __X__ can be anything you want , in my 4 Core machine I used 4 threads for building. Initial build is going to take quite sometime.

## Section B: Your own Pass
### __Preparing the enviroment for your own Pass__

First of all we need to place somewhere our Pass code. For this guide lets place it inside llvm sources. 
From our previous directory lets head back to llvm sources.

```
cd ..
cd llvm
```
Since our pass name is MemLog we shall place our Pass implementation files in /lib/MemLog directory.

```
cd lib
mkdir MemLog
cd MemLog
```
You can go ahead now and place your own C++ file in MemLog dir.
Our files are in the LLVM sources directory but is not yet included in LLVM's build process, lets change that. Firstly, create a file called "CMakeLists.txt" and enter the following:
```
add_llvm_library( LLVMMemLog MODULE
  MemLog.cpp

  PLUGIN_TOOL
  opt
  )
```
_Note:_ Remember to change "MemLog" to your own Pass name.

Afterwards, we need to also include this build module to the rest. For that:
```
cd ..
```
and insert the following in CMakeLists.txt there as well:
```
add_subdirectory(LogMem)
```

### __Running your Pass:__

Once you have written your Pass it is time now to run it. 

<sup>(ok, ok  you might need to run it during development for debugging as well)</sup>

Your code is located in _"LogMem.cpp"_ inside _"lib/LogMem"_. In order for it to be compiled to an .so ready to be run by LLVM you have to head back to build folder and run make:

```
cd /home/$USER/llvm-project/build
make -J<X>
```
This will rerun build on whole LLVM but since we didn't touch anything else it will only detect and compile our pass.

Once that is complete and to make our life easier lets include LLVM's /bin directory containing clang, to our path as well:

```
cd bin
PATH=`pwd`:$PATH
```

Our pass is ready to be run. Head to any directory you would like and containing some good' ol plain C code that you want to log and type the following:
```
cd 
cd my_awesome_c_project
clang -emit-llvm myProgram.c -c -o myProgram.bc
opt -load /home/$USER/workspace/llvm-project/build/lib/LLVMMemLog.so -MemLog < myProgram.bc > myProgram2.bc
clang myProgram2.bc -o myProgram
```
You can now execute your beautiful instrumented executable.

Now there is a way to actually make Clang call your pass without having to go through that whole procedure above where you run manually opt etc. Unfortunately, as much as I have tried to make it work it just won't budge so I went on with this method.

![](https://thumbs.gfycat.com/AdorableGrizzledAdmiralbutterfly-max-1mb.gif)
