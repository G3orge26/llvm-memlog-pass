# __Writing Your Pass__

## __MemLog Pass__
MemLog Pass is a relative "simple" pass that inject the appropriate code on a C binary that:
1. Adds an fopen call at the beginning of main function.
2. Iterates over the whole IR code and injects an fprintf function after every malloc and free call.
3. Adds an fclose call before any return instructions in the main function.

## __Disclaimer:__
Under no circumstances am I an expert on LLVM, and there might be other and better ways of accomplishing the same task, however since LLVM can be quite overwhelming for someone who hasn't had any previous experience on the subject, I decided to go ahead and present in this guide any knowledge I have acquired.

## __First Steps__

If you are a complete beginner on LLVM Passes I strongly recommend you open a new tab on your browser and follow the [Writing an LLVM Pass](http://llvm.org/docs/WritingAnLLVMPass.html) guide from the official documentation of LLVM. Go on, I will be waiting. Ok so now that you wrote that LLVM Pass (at least I hope so), you should have a .cpp file with the following code inside:

<details> 
<summary> Show Code </summary>

```cpp

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct Hello : public FunctionPass {
  static char ID;
  Hello() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    errs() << "Hello: ";
    errs().write_escaped(F.getName()) << '\n';
    return false;
  }
}; // end of struct Hello
}  // end of anonymous namespace

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new Hello()); });

```
</details>

### Theory break 
Before proceeding further, lets take a look at how things work behind the scenes. As you may already have seen on [Writing an LLVM Pass](http://llvm.org/docs/WritingAnLLVMPass.html#the-modulepass-class),
in LLVM context, a module is essentially a whole file, that contains one or more functions (for example in C, C++ and Java we know we will probably have at least the main function), where each function contains BasicBlocks filled with Instructions. You can see a visual representation of this on the following figure.

![](https://habrastorage.org/webt/4k/-g/nl/4k-gnlzr7e6573zeobjdcp91x5q.jpeg)

Now, LLVM provides us with a two handy methods when writing a pass:
```cpp
virtual bool runOnModule(Module &M) = 0;
virtual bool runOnFunction(Function &F) = 0;
```
<sup>_Note:_ LLVM actually provides more methods, but they are for a bit more specialized work. You can take a more in-depth look at writing an LLVM pass if you want.</sup>

As you can guess, runOnModule function, runs on the whole file you are currently processing, while runOnFunction runs on each Function in a module. Each function is provided by their respective superclass(ModulePass for runOnModule, FunctionPass for runOnFunction), and these Pass superclasses have some designated restrictions on what your Pass is allowed or not to do. For example:
```
To be explicit, FunctionPass subclasses are not allowed to:

1. Inspect or modify a Function other than the one currently being processed.
2. Add or remove Functions from the current Module.
3. Add or remove global variables from the current Module.
4. Maintain state across invocations of runOnFunction (including global data).
```
<sup>Taken from [Writing an LLVM Pass#FunctionPass](http://llvm.org/docs/WritingAnLLVMPass.html#the-functionpass-class) .</sup>

As such, for our MemLog pass, lets retrofit the tutorial pass, changing FunctionPass to Module pass and stripping code, preparing for our own Pass.
Change your .cpp to look like the following:

```cpp
/* Memlog.cpp File */

/* Includes */
#include "llvm/Pass.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <stdio.h>

using namespace llvm;

namespace { /*anonymous namespace begin*/
  struct memLog : public ModulePass {
    static char ID;
    memLog() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {

      return true;
    }

  };
} /* anonymous namespace end*/

char memLog::ID = 0;
static RegisterPass<memLog> X("MemLog", "Logging dynamic memory allocation", false, false);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new memLog());
                                });
```
__Note:__ not all these includes are required but since we will need them eventually let them sit there.




## __Pass Internals__

We are now ready to start tinkering around slowly building our Pass to what we want it to be. 

Before going in to make this specific MemLog Pass of this guide, you might wonder "This pass is good and all, but it doesn't do what my project/assignment requires, how can I know what instructions and how I'll have to use?"
Worry not you rascal. Lets say that you want to make a different Pass that inserts some other library call that you have absolutely no idea what type of arguments it receives or what it's signature will look like in LLVM IR code, and you are lost. In that case the most easy way of overcoming this big boulder of a problem is simple. Create a dummy C program that calls the said functions, emit it's IR code and inspect it. 

If you run this:

```
clang -S -emit-llvm my-dummy-proggy.c 
```
a file called  my-dummy-proggy.ll will be created which you can then inspect with any text editor and see what IR code is produced.

Lets see what IR is produced by a hello world program.
```c
#include <stdio.h>
int main(int argc, char *argv[]){
    printf("Hello World\n");
}
```
IR Code Generated:
```
; ModuleID = 'hello.c'
source_filename = "hello.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"Hello World\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  store i32 0, i32* %retval, align 4
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str, i32 0, i32 0))
  ret i32 0
}

declare i32 @printf(i8*, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 6.0.1 "}

```

<sup>
__Note:__ This might not look 100% identical to your machine but what will be different most likely are the return values such as _%retval_ or _%argc.addr_ .
</sup>

Now you don't have to be an assembly expert(and you don't need to since this isn't actual assembly) to study this. A brief inspection is enough to show that:

```
; Function Attrs: noinline nounwind optnone uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  store i32 0, i32* %retval, align 4
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str, i32 0, i32 0))
  ret i32 0
}
```
This is the main function with the attributes set on it, the return type, it's name, the argument types and last but not least the actual instructions it contains. You may also notice that printf is in there but without a body and one strange component on its arguments:
```
declare i32 @printf(i8*, ...) #1
```
This is perfectly normal since, even though in our source we use printf, the function code lives in a library source file somewhere in our system, and thanks to dynamic linking, that occurs after IR generation and before we have our binary, will know where the printf symbol is in order to retrieve its code. And what about those "..." present in its declaration ? Remember that printf can take a variable number of arguments thus those 3 dots represent that exact thing: that this function can receive a variable number of arguments.

Now another question you should have is:

 _"Ok, now I have the means to study the code that I will have to insert to complete my work, but what do all these things I am seeing mean?"_ 
 
 Fret not dear reader, for LLVM has you covered once again. You can now start using another very valuable asset provided by LLVM: The [LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html). It can be a daunting task initially, but once you know where to look, it will become your best friend when dealing with LLVM. If you are still wondering what all these Attributes mean above our main function give it a go and head over to [LLVM Language Reference Manual#functions](https://llvm.org/docs/LangRef.html#functions). Now that you are back, you should be able to read that line with ease.


## __Details on MemLog Pass__

As I said on the beginning, MemLog needs to insert 3 functions:
1. fopen
2. fprintf
3. fclose

Following the above methodology lets make a hello.c :
```c
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char *argv[]){
	FILE *fp;
	int num = 2020;
	fp = fopen("file.txt", "w");
        fprintf(fp,"Hello World from %d\n",num);
	fclose(fp);
	return 0;
}
```
which produces the following IR code:
<details>
<summary> Show Code!</summary>

```
; ModuleID = 'hello.c'
source_filename = "hello.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, %struct._IO_codecvt*, %struct._IO_wide_data*, %struct._IO_FILE*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type opaque
%struct._IO_codecvt = type opaque
%struct._IO_wide_data = type opaque

@.str = private unnamed_addr constant [9 x i8] c"file.txt\00", align 1
@.str.1 = private unnamed_addr constant [2 x i8] c"w\00", align 1
@.str.2 = private unnamed_addr constant [21 x i8] c"Hello World from %d\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %fp = alloca %struct._IO_FILE*, align 8
  %num = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  store i32 2020, i32* %num, align 4
  %call = call %struct._IO_FILE* @fopen(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i32 0, i32 0), i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.1, i32 0, i32 0))
  store %struct._IO_FILE* %call, %struct._IO_FILE** %fp, align 8
  %0 = load %struct._IO_FILE*, %struct._IO_FILE** %fp, align 8
  %1 = load i32, i32* %num, align 4
  %call1 = call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %0, i8* getelementptr inbounds ([21 x i8], [21 x i8]* @.str.2, i32 0, i32 0), i32 %1)
  %2 = load %struct._IO_FILE*, %struct._IO_FILE** %fp, align 8
  %call2 = call i32 @fclose(%struct._IO_FILE* %2)
  ret i32 0
}

declare %struct._IO_FILE* @fopen(i8*, i8*) #1

declare i32 @fprintf(%struct._IO_FILE*, i8*, ...) #1

declare i32 @fclose(%struct._IO_FILE*) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 6.0.1 "}
```
</details>

That quite some code but lets keep only the interesting bits, shall we? From top to bottom first off we spot this:
```
%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, %struct._IO_codecvt*, %struct._IO_wide_data*, %struct._IO_FILE*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type opaque
%struct._IO_codecvt = type opaque
%struct._IO_wide_data = type opaque
```
Once again a quick turn at [LLVM Language Reference Manual#structure-types](https://llvm.org/docs/LangRef.html#structure-types) provides us with all the info that we might need. From this we can deduce that we have 4 Structures defined, _IO_FILE, _IO_Marker, _IO_codecvt and _IO_wide_data respectfully, from which, _IO_FILE is declared and defined containing all those things presented on the IR code, and the rest are forward declared but not defined, meaning that their type is not available at this moment.

Moving on we also have:
```
@.str = private unnamed_addr constant [9 x i8] c"file.txt\00", align 1
@.str.1 = private unnamed_addr constant [2 x i8] c"w\00", align 1
@.str.2 = private unnamed_addr constant [21 x i8] c"Hello World from %d\0A\00", align 1
```
Now, you may or may not know this, but all global variables in a program are defined in _initialized data segment_ and it remains there throughout the programs execution, including strings that our prints have. Keep in mind though that these strings are constant, meaning they cannot be altered at runtime. 
Lastly we have the functions we need to inject with our Pass:
```
declare %struct._IO_FILE* @fopen(i8*, i8*) #1

declare i32 @fprintf(%struct._IO_FILE*, i8*, ...) #1

declare i32 @fclose(%struct._IO_FILE*) #1
```

As you may have guessed, without setting properly all of the above in the source that our Pass is modifying, we can't begin doing anything 

## __Setting up the Globals on our Pass__

Without further a due lets proceed to actually code our Pass. 

For this part, if you want an extremely in-depth explanation of how things work you are going to have to browse the [doxygen](https://llvm.org/doxygen/) pages containing the documentation of LLVM (similar to javadocs provided for Java), and you will also have to refer to [LLVM Programmer's Manual](http://llvm.org/docs/ProgrammersManual.html)

Assuming we pick up where we left off earlier on our LogMem.cpp file lets go ahead and create a method where we are going to set up our global variables. Our _prepareGlobals_ method will require 2 parameters an LLVMContext variable and a Module variable, so that we can access their methods. You should also add some variables in our Pass class as we will want to keep some references to things that will happen inside our function. For that reason add the following variables:
```cpp
namespace {
struct memLog : public ModulePass {
  static char ID;

  Value *fp;
  Value *filename;
  Value *perms;
  Value *retval;
  Value *malloc_str;
  Value *free_str;
  Function *F_close, *F_open, *F_printf;
  .
  .
  .
  .
```
 Further instructions inside comments on the code segment bellow.

```cpp
/* 
Fopen as you may recall requires 2 string arguments to function, one containing to name of the file we want to open, and one containing the permissions with which we ll open the file.
*/
void prepareGlobals(LLVMContext &ctx, Module &M) {
//We need a Constant Variable with the actual string.
    Constant *fp_filename = ConstantDataArray::getString(ctx, "log.txt");
/*
This is an important part. Below, we call the getOrInsertGlobal from the Module M var. This method searches in the global section and if it doesn't find a global variable named "_filename" with type fp_filename->getType(), it inserts it. 
Alternatively, if it locates it, with a different type, returns it with the right type, or if it exists and has the same type returns that one. 
*/
    Constant *fp_filenameVar =
        M.getOrInsertGlobal("_filename", fp_filename->getType());
/*
We want cast our new var, for easy access, and then set several attributes for it.
More on that later on.
*/
    GlobalVariable *fp_FilenameVar(dyn_cast<GlobalVariable>(fp_filenameVar));
    fp_FilenameVar->setInitializer(fp_filename);
    fp_FilenameVar->setAlignment(1);
    fp_FilenameVar->setVisibility(GlobalValue::VisibilityTypes::DefaultVisibility);
    fp_FilenameVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    fp_FilenameVar->setConstant(true);
    fp_FilenameVar->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);
/*
At this point, we have created a global variable named _filename, and set up its attributes, so lets keep a reference to it, since we will need it later on.
*/
    this->filename = fp_filenameVar;

/*
Doing the same things for Malloc message and free messages.
Keep in mind though that you should have already decided what you want to print with your fprintf since as we said earlier these strings are constant.
*/
    Constant *str_for_malloc =
        ConstantDataArray::getString(ctx, "Malloc memory at %p size: %lu\n");
    Constant *str_for_mallocVar =
        M.getOrInsertGlobal("_mal_str", str_for_malloc->getType());

    GlobalVariable *str_mallocVar( dyn_cast<GlobalVariable>(str_for_mallocVar));
    str_mallocVar->setInitializer(str_for_malloc);
    str_mallocVar->setAlignment(1);
    str_mallocVar->setVisibility(GlobalValue::VisibilityTypes::DefaultVisibility);
    str_mallocVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    str_mallocVar->setConstant(true);
    str_mallocVar->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);

    this->malloc_str = str_for_mallocVar;

    Constant *str_for_free =
        ConstantDataArray::getString(ctx, "Freed memory at %p\n");
    Constant *str_for_freeVar =
        M.getOrInsertGlobal("_free_str", str_for_free->getType());

    GlobalVariable *str_freeVar(dyn_cast<GlobalVariable>(str_for_freeVar));
    str_freeVar->setInitializer(str_for_free);
    str_freeVar->setAlignment(1);
    str_freeVar->setVisibility(GlobalValue::VisibilityTypes::DefaultVisibility);
    str_freeVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    str_freeVar->setConstant(true);
    str_freeVar->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);

    this->free_str = str_for_freeVar;
    
}
```

At this point we have successfully constructed and inserted the Strings we will need further on as arguments to our calls. However you should be wondering how can we know what kind of attributes are required for this string. And given how, no, I am not a wizard Hagrid, as we said before, you will have to search in the my_dummy_proggy.ll in junction with [LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html). As for all the setter and miscellaneous functions used above (and the rest we will use further on) and where to find them, you can search the [LLVM Programmer's Manual](http://llvm.org/docs/ProgrammersManual.html) but chances are you wont find everything in there. At that point you will have to go ahead an browse through the doxygen pages, scanning the classes of interest to you and their methods as well as their inherited methods. 

Lets go ahead and create another method _prepareStructs_ with the same arguments that will set up for us the structs representing and required by FILE, so that we can accommodate the return type of fopen, and also pass it later on to fprintfs and fclose.
Again detailed instructions will be available inside the code segment:
```cpp
void prepareStructs(LLVMContext &ctx, Module &M) {
/*
First of all we make a vector where we are going to store all of the fields, or more precicely the types of the fields, that our struct will have. If you take a look at _IO_FILE_ struct, you can see that it has one 32bit integer first followed by 11 pointers to Ints in the beginning.
*/

  std::vector<Type *> io_struct_body;
  io_struct_body.push_back(Type::getInt32Ty(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));

/*
It alse includes 3 structs which we define below. Here, the create method inserts this Type as a structure in the Module M (remember a Module represents our whole source file), thus making it visible to any instructions later on.
*/
  StructType *ioMark = StructType::create(M.getContext(), "struct._IO_marker");
  StructType *ioCode = StructType::create(M.getContext(), "struct._IO_codecvt");
  StructType *ioWide =
      StructType::create(M.getContext(), "struct._IO_wide_data");

/*
The _IO_FILE struct contains pointers to three other structs, or better, a field which has type of Pointer to Struct "x". Below we use the getUnqual method from the PointerType class to 
get our three Type variables, each representing a pointer to each different struct.
*/
  Type *ioMarkTy = PointerType::getUnqual(ioMark);
  Type *ioCodeTy = PointerType::getUnqual(ioCode);
  Type *ioWideTy = PointerType::getUnqual(ioWide);

  StructType *fpStruct = StructType::create(M.getContext(), "struct._IO_FILE");
  Type *fpStructTy = PointerType::getUnqual(fpStruct);

/*
More fields from _IO_FILE struct
*/

  io_struct_body.push_back(ioMarkTy);
  io_struct_body.push_back(fpStructTy);
  io_struct_body.push_back(Type::getInt32PtrTy(ctx));
  io_struct_body.push_back(Type::getInt32PtrTy(ctx));
  io_struct_body.push_back(Type::getInt64Ty(ctx));
  io_struct_body.push_back(Type::getInt16Ty(ctx));
  io_struct_body.push_back(Type::getInt8Ty(ctx));

/*
If you take a look at _IO_FILE it alos has a couple arrays so we need those too..
*/

  ArrayType *internal1 = ArrayType::get(Type::getInt8Ty(ctx), 1);
  io_struct_body.push_back(internal1);
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt64Ty(ctx));
  io_struct_body.push_back(ioCodeTy);
  io_struct_body.push_back(ioWideTy);
  io_struct_body.push_back(fpStructTy);
  io_struct_body.push_back(Type::getInt8PtrTy(ctx));
  io_struct_body.push_back(Type::getInt64Ty(ctx));
  io_struct_body.push_back(Type::getInt32PtrTy(ctx));
  ArrayType *internal2 = ArrayType::get(Type::getInt8Ty(ctx), 20);
  io_struct_body.push_back(internal2);


/*
So far we have been pushing the fields types, and now we have a vector containing all of _IO_FILE structs elements types, so lets go ahead and set that vector as the body of our _IO_FILE struct
*/
  fpStruct->setBody(io_struct_body, false);

  StructType *fp_Ty = M.getTypeByName("struct._IO_FILE");
  Constant *fp = M.getOrInsertGlobal("struct._IO_FILE", fp_Ty);
}
```
That wasn't so bad now was it? We have set up our global variables and ours structs so lets go ahead and introduce the functions that are going to actually use them! Once again lets make a _prepareFunctions_ method with the same arguments.

```cpp
void prepareFunctions(LLVMContext &ctx, Module &M){

/*
Retrieving our beloved Structs that spend so much time creating them previously.
*/
    StructType *FILE_Struct = M.getTypeByName("struct._IO_FILE");
    Type *FILE_Struct_Ty = PointerType::getUnqual(FILE_Struct);
/*
Here we call another variant of getOrInsert, the getOrInsertFunction, which we supply with the name, the return type and its parameter types. Keep in mind, the function parameters is a variantic argument meaning you can keep adding as many arguments as your function has. Once again, getOrInsertFunction will look up whether a function called "fopen" has been already defined in our module. If it has it returns that one, with the correct parameters in case we have entered them incorrectly, or it simply defines it and inserts it on the module.
fopen requires 2 pointers, for the two strings.
*/
    Constant *f_open =
        M.getOrInsertFunction("fopen", FILE_Struct_Ty, Type::getInt8PtrTy(ctx),
                              Type::getInt8PtrTy(ctx));

/*
Here we cast f_open constant to a Function class so that we can set some attributes to it (That we have scouted from the .ll file).
*/                              
    Function *F_open = dyn_cast<Function>(f_open);
    F_open->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
    F_open->setUnnamedAddr(GlobalValue::UnnamedAddr::Local);
    F_open->setReturnDoesNotAlias();
    F_open->addParamAttr(0, Attribute::NoCapture);
    F_open->addParamAttr(0, Attribute::ReadOnly);
    F_open->addParamAttr(1, Attribute::NoCapture);
    F_open->addParamAttr(1, Attribute::ReadOnly);
    F_open->setCallingConv(CallingConv::C);

    this->F_open = F_open;
/*
Same workflow for fclose.
*/
    Constant *f_close =
        M.getOrInsertFunction("fclose", Type::getInt32Ty(ctx), FILE_Struct_Ty);
    Function *F_close = dyn_cast<Function>(f_close);
    F_close->setUnnamedAddr(GlobalValue::UnnamedAddr::Local);
    F_close->addParamAttr(0, Attribute::NoCapture);

    this->F_close = F_close;
/*
Here, fprintf has a few extra steps. Remember that fprintf receives a variantic number of arguments, something we want to add to its definition. To do so
we will need to use another instance of getOrInsertFunction which accepts 2 arguments, a name and a function type. A FunctionType object describes the function in total in a more compact way. As you can see at the ctor we provide the return type, a vector with the argument types and also a boolean flag. That flag defines whether or not that function accepts variantic arguments.
*/
    std::vector<Type *> f_printf_args;
    f_printf_args.push_back(FILE_Struct_Ty);
    f_printf_args.push_back(Type::getInt8PtrTy(ctx));
    ArrayRef<Type *> F_printf_args(f_printf_args);
    FunctionType *fprintf_ty =
        FunctionType::get(Type::getInt32Ty(ctx), F_printf_args, true);
    Constant *f_printf = M.getOrInsertFunction("fprintf", fprintf_ty);
    Function *F_printf = dyn_cast<Function>(f_printf);
    F_printf->setUnnamedAddr(GlobalValue::UnnamedAddr::Local);
    F_printf->addParamAttr(0, Attribute::NoCapture);
    F_printf->addParamAttr(1, Attribute::NoCapture);
    F_printf->addParamAttr(1, Attribute::ReadOnly);

    this->F_printf = F_printf;
    return;
  }

```
<sup>_Note:_ Since FunctionType::get method requires the second argument to be an ArrayRef object, we leverage one of its constructors which construct such an object from a good 'ol plain vector.</sup>

Now we are all set up to proceed with the Logic of the pass, which is something more straightforward than what we were doing so far.
Remember that runOnModule function we talked about earlier ? Lets go implement it now:
```cpp
/*
First things first, this functions requires a reference to a module.
*/
bool runOnModule(Module &M) override {
/*
We are also going to need a reference to this LLVMContext so lets keep that somewhere handy, and since we spent so much time making our prepare functions lets go ahead and call them up as well.
*/
    LLVMContext &ctx = M.getContext();
    prepareStructs(ctx, M);
    prepareGlobals(ctx, M);
    prepareFunctions(ctx, M);
/*
As you will notice soon, our work takes place in three nested for loops where:
1. for(auto &F : M) iterates through every Function in the Module.
2.  for(auto &B : F) iterates through every Basic Block in a Function.
3.   for(auto &I : B) iterates through every Instruction in a Basic Block
*/

    for (auto &F : M) {

/*
We iterate every function looking for main, once that is located we need to insert our fopen call. To that end, we use the IRBuilder, a factory class for creating Instructions. By supplying it an instruction (in this instance getFirstInsertionPt()) we specify that the instruction it will emit will be inserted right before it.
*/
      if (F.getName() == "main") {
        IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());
/*
Recall that, our strings live on the global segment, but a string is an Array, and fopen expects two pointers to char (aka, string) as arguments. In order to pass our strings we need to get a pointer that points on the first element of the forementioned Arrays, and we do so with the help of CreatePointerCast, where we supply the Array, and what type of pointer we want to receive back, and an optional identifier("filenameStr").
*/
        Value *filenameStr = Builder.CreatePointerCast(
            this->filename, Type::getInt8PtrTy(ctx), "filenameStr");
        Value *permsStr = Builder.CreatePointerCast(
            this->perms, Type::getInt8PtrTy(ctx), "perms");
        std::vector<Value *> vec;
        vec.push_back(filenameStr);
        vec.push_back(permsStr);
        ArrayRef<Value *> args(vec);
/*
Calling the CreateCall method will insert our fopen Call Instruction at the point specified at IRBuilder. CreateCall in this case will return an Object of CallInstr, which inherits the class Value so "this->fp = inserted_call" is a valid assignment. But why do we need to keep track of that ? In LLVM IR every Instruction be it an ADD instruction or in this case a CallInstruction, also serves as the Value returned by it.Thus here, we want to keep track of the _IO_FILE struct that fopen will return since we will need to supply it to fprintf and fclose as well in future calls.
*/
        auto inserted_call = Builder.CreateCall(this->F_open, args);
        this->fp = inserted_call;
      }

/*
At this point we set up our fopen in main only thanks to the above if check. Now the code below will run for any function. Recall that at this point, we want to locate any malloc or free call, and log it with an fprintf instruction, as well as locate any return instructions in main and properly close our file descriptor.
*/

      for (auto &B : F) {
        for (auto &I : B) {
/*
Iterating through every instruction on a function, if we spot a ReturnInst and we are at the main, insert an fclose right before it.
*/
          if (auto *curr_instr = dyn_cast<ReturnInst>(&I)) {
            if (F.getName() == "main") {
              std::vector<Value *> f_close_args;
              IRBuilder<> Builder(curr_instr);
              f_close_args.push_back(this->fp);
              ArrayRef<Value *> args(f_close_args);
              Builder.CreateCall(this->F_close, args);
            }
          }
/*
If we encounter a CallInstr, check whether its a malloc or a free. If it is insert an fprintf instruction right below it.
*/

          if (auto *curr_instr = dyn_cast<CallInst>(&I)) {
            if (curr_instr->getCalledFunction()->getName() == "malloc") {
              IRBuilder<> Builder(curr_instr);
              Value *malloc_return = curr_instr;
              Value *malloc_size = curr_instr->getArgOperand(0);
              Value *fprintf_format_str = Builder.CreatePointerCast(
                  this->malloc_str, Type::getInt8PtrTy(ctx),
                  "fprintf_malloc_format_str");
              std::vector<Value *> f_printf_args;
              f_printf_args.push_back(this->fp);
              f_printf_args.push_back(fprintf_format_str);
              f_printf_args.push_back(malloc_return);
              f_printf_args.push_back(malloc_size);
              
              ArrayRef<Value *> args(f_printf_args);
              auto *logFun = Builder.CreateCall(this->F_printf, args);
/*
This is an important line. Remember that IRBuilder as we are using it, inserts our calls right before the Instruction we are iterating. Since we need the return value of malloc, we can't call fprintf before the malloc, thus, after inserting it, we also want to move it right after curr_instr.
*/
              logFun->moveAfter(curr_instr);
            }
/*
Similar code for our free function. In this case since we don't need its return argument, we could place fprintf right before it, but for the sake of consistency and logic, lets not do that.
*/
            if (curr_instr->getCalledFunction()->getName() == "free") {
              IRBuilder<> Builder(curr_instr);
              Value *free_location = curr_instr->getArgOperand(0);
              Value *fprintf_format_str = Builder.CreatePointerCast(
                  this->free_str, Type::getInt8PtrTy(ctx),
                  "fprintf_free_format_str");
              std::vector<Value *> f_printf_args;
              f_printf_args.push_back(this->fp);
              f_printf_args.push_back(fprintf_format_str);
              f_printf_args.push_back(free_location);
              ArrayRef<Value *> args(f_printf_args);
              auto *logFun = Builder.CreateCall(this->F_printf, args);
              logFun->moveAfter(curr_instr);
            }
          }
        }
      }
    }

/*
Remember that runOnModule function returns a boolean. That boolean represents whether or not runOnModule changes something inside the module or simply inspected it. So in this case we need to return true.
*/
    errs() << "MemLog Pass Finished\n";
    return true;
  }
```

With that, this guide is concluded. If you stayed with it through the end I would like to thank you. At this point you should have enough background/theory knowledge as well as enough confidence to browse the manuals and doxygen pages and find fast and efficiently what you want. 

I hope I could give you a more friendly introduction to LLVM from what I experienced (which is the reason for writing all these). To me LLVM looked extremely difficult, to the point where I disliked it since at first glance the manuals provided by the LLVM creators are huge and expect you to understand a lot of things not clear to a newbie let alone that stackoverflow and github projects on LLVM are very limited and usually extremely introductory or outdated. However, once I started getting the hang of it and browsing through the manuals and doxygen knowing where and what to look for I have to admit it is a very interesting work and it allows, no-compiler-programmers such as myself to make a lot of interested things with Passes.

