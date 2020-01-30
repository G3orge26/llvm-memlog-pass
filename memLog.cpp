#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <stdio.h>

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

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

  memLog() : ModulePass(ID) {}

void prepareStructs(LLVMContext &ctx, Module &M) {
  // Set up structs for FP
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

  StructType *ioMark = StructType::create(M.getContext(), "struct._IO_marker");
  StructType *ioCode = StructType::create(M.getContext(), "struct._IO_codecvt");
  StructType *ioWide =
      StructType::create(M.getContext(), "struct._IO_wide_data");
  Type *ioMarkTy = PointerType::getUnqual(ioMark);
  Type *ioCodeTy = PointerType::getUnqual(ioCode);
  Type *ioWideTy = PointerType::getUnqual(ioWide);

  StructType *fpStruct = StructType::create(M.getContext(), "struct._IO_FILE");
  Type *fpStructTy = PointerType::getUnqual(fpStruct);

  io_struct_body.push_back(ioMarkTy);
  io_struct_body.push_back(fpStructTy);
  io_struct_body.push_back(Type::getInt32PtrTy(ctx));
  io_struct_body.push_back(Type::getInt32PtrTy(ctx));
  io_struct_body.push_back(Type::getInt64Ty(ctx));
  io_struct_body.push_back(Type::getInt16Ty(ctx));
  io_struct_body.push_back(Type::getInt8Ty(ctx));

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

  fpStruct->setBody(io_struct_body, false);

  StructType *fp_Ty = M.getTypeByName("struct._IO_FILE");
  Constant *fp = M.getOrInsertGlobal("struct._IO_FILE", fp_Ty);
}

  void prepareGlobals(LLVMContext &ctx, Module &M) {
    // Set Up strings for fprintf usage
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

    // Set Up strings for Fopen
    Constant *fp_filename = ConstantDataArray::getString(ctx, "log.txt");
    Constant *fp_filenameVar =
        M.getOrInsertGlobal("_filename", fp_filename->getType());
    GlobalVariable *fp_FilenameVar(dyn_cast<GlobalVariable>(fp_filenameVar));
    fp_FilenameVar->setInitializer(fp_filename);
    fp_FilenameVar->setAlignment(1);
    fp_FilenameVar->setVisibility(GlobalValue::VisibilityTypes::DefaultVisibility);
    fp_FilenameVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    fp_FilenameVar->setConstant(true);
    fp_FilenameVar->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);

    Constant *fp_perms = ConstantDataArray::getString(ctx, "w");
    Constant *fp_permsVar =
        M.getOrInsertGlobal("_fopen_perms", fp_perms->getType());
    GlobalVariable *fp_PermsVar(dyn_cast<GlobalVariable>(fp_permsVar));
    fp_PermsVar->setInitializer(fp_perms);
    fp_PermsVar->setAlignment(1);
    fp_PermsVar->setVisibility(GlobalValue::VisibilityTypes::DefaultVisibility);
    fp_PermsVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    fp_PermsVar->setConstant(true);
    fp_PermsVar->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);
    //=========

    this->filename = fp_filenameVar;
    this->perms = fp_permsVar;

    
    //================
  }

  void prepareFunctions(LLVMContext &ctx, Module &M){

    //prepare fopen function
    StructType *FILE_Struct = M.getTypeByName("struct._IO_FILE");
    Type *FILE_Struct_Ty = PointerType::getUnqual(FILE_Struct);

    Constant *f_open =
        M.getOrInsertFunction("fopen", FILE_Struct_Ty, Type::getInt8PtrTy(ctx),
                              Type::getInt8PtrTy(ctx));
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
    // prepare fclose function
    Constant *f_close =
        M.getOrInsertFunction("fclose", Type::getInt32Ty(ctx), FILE_Struct_Ty);
    Function *F_close = dyn_cast<Function>(f_close);
    F_close->setUnnamedAddr(GlobalValue::UnnamedAddr::Local);
    F_close->addParamAttr(0, Attribute::NoCapture);

    this->F_close = F_close;

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

  bool runOnModule(Module &M) override {
    LLVMContext &ctx = M.getContext();
    prepareStructs(ctx, M);
    prepareGlobals(ctx, M);
    prepareFunctions(ctx, M);
    

    for (auto &F : M) {
      if (F.getName() == "main") {
        IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());
        Value *filenameStr = Builder.CreatePointerCast(
            this->filename, Type::getInt8PtrTy(ctx), "filenameStr");
        Value *permsStr = Builder.CreatePointerCast(
            this->perms, Type::getInt8PtrTy(ctx), "perms");
        std::vector<Value *> vec;
        vec.push_back(filenameStr);
        vec.push_back(permsStr);
        ArrayRef<Value *> args(vec);
        auto inserted_call = Builder.CreateCall(this->F_open, args);
        this->fp = inserted_call;
      }

      for (auto &B : F) {
        for (auto &I : B) {
          // whenever we encounter a return in main function add an fclose above
          // it even though the OS would close any left open file handles on
          // close.
          if (auto *curr_instr = dyn_cast<ReturnInst>(&I)) {
           
            if (F.getName() == "main") {
              std::vector<Value *> f_close_args;
              IRBuilder<> Builder(curr_instr);
              f_close_args.push_back(this->fp);
              ArrayRef<Value *> args(f_close_args);
              Builder.CreateCall(this->F_close, args);
            }
          }
          // if we encounter malloc add an fprintf below it 
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
              logFun->moveAfter(curr_instr);
            }
          // if we encounter free add an fprintf below it
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
    errs() << "MemLog Pass Finished\n";
    return true;
  }
};
} // namespace
char memLog::ID = 0;
static RegisterPass<memLog> X("MemLog", "Logging dynamic memory allocation", false, false);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new memLog());
                                });
