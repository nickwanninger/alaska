#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>

using namespace llvm;

llvm::Function *versionFunction(llvm::Function &F, std::set<llvm::Argument *> handle_arguments) {
  // Get the module.
  auto *module = F.getParent();
  auto &ctx = module->getContext();

  // Get information from the old function type.
  auto *old_func_type = F.getFunctionType();
  ALASKA_SANITY((old_func_type != nullptr), "Couldn't determine the old function type");
  auto *return_type = old_func_type->getReturnType();
  auto param_types = old_func_type->params().vec();
  auto is_var_arg = old_func_type->isVarArg();

  // Get the opaque pointer type.
  auto *ptr_type = PointerType::get(ctx, 0);

  // For each handle argument, add an extra argument.
  // NOTE: Reverse iterate so we don't have to mess with running offsets.
  unsigned arg_offset = 0;
  std::map<unsigned, unsigned> argument_index_mapping;
  for (auto &arg : F.args()) {
    // Get the index for the handle argument.
    auto arg_index = arg.getArgNo();

    // Check if this isn't a handle argument.
    if (handle_arguments.find(&arg) == handle_arguments.end()) {
      argument_index_mapping[arg_index] = arg_index + arg_offset;
    }

    // Insert an opaque pointer there.
    auto param_it = param_types.begin() + arg_index + arg_offset + 1;
    param_types.insert(param_it, ptr_type);

    // Save the index mapping.
    argument_index_mapping[arg_index] = arg_index + arg_offset;

    // Update the argument offset.
    arg_offset++;
  }

  // Construct the new function type.
  auto *new_func_type = FunctionType::get(return_type, param_types, is_var_arg);

  // Create an empty function to clone into.
  auto *new_function =
      Function::Create(new_func_type, F.getLinkage(), F.getName() + ".alaska", module);

  // Map the arguments.
  ValueToValueMapTy vmap;
  for (auto const &[old_index, new_index] : argument_index_mapping) {
    auto *old_arg = F.getArg(old_index);
    auto *new_arg = new_function->getArg(new_index);
    vmap[old_arg] = new_arg;
  }

  // Version the function.
  llvm::SmallVector<llvm::ReturnInst *, 8> returns;

  llvm::CloneFunctionInto(
      new_function, &F, vmap, llvm::CloneFunctionChangeType::LocalChangesOnly, returns);

  // Remap the function.
  ValueMapper mapper(vmap);
  for (auto &old_arg : F.args()) {
    auto *new_arg = new_function->arg_begin() + old_arg.getArgNo();
    for (auto &BB : *new_function) {
      for (auto &I : BB) {
        I.replaceUsesOfWith(&old_arg, new_arg);
      }
    }
  }

  return new_function;
}

llvm::PreservedAnalyses RedundantArgumentPinElisionPass::run(Module &M, ModuleAnalysisManager &AM) {
  return PreservedAnalyses::all();
  // Which arguments does each function lock internally? (Array of true and false for each index of
  // the args)
  std::map<llvm::Function *, std::vector<bool>> locked_arguments;

  for (auto &F : M) {
    if (F.empty()) continue;
    if (F.getName() == "main") continue;

    // Which arguments does this function lock?
    auto locks = alaska::extractTranslations(F);
    std::vector<bool> locked;
    locked.reserve(F.arg_size());
    for (unsigned i = 0; i < F.arg_size(); i++) {
      locked.push_back(false);
    }

    for (auto &l : locks) {
      auto locked_allocation = l->getRootAllocation();
      for (unsigned i = 0; i < F.arg_size(); i++) {
        auto *arg = F.getArg(i);
        if (arg == locked_allocation) {
          locked[i] = true;
        }
      }
    }

    locked_arguments[&F] = std::move(locked);
  }

  // Version the function.
  std::map<llvm::Function *, std::vector<llvm::Argument *>> versioned_arguments;
  for (auto &[func, locked] : locked_arguments) {
    continue;
    // Get the handle arguments.
    std::set<llvm::Argument *> handle_arguments;
    for (unsigned i = 0; i < locked.size(); i++) {
      if (locked[i]) {
        handle_arguments.insert(func->getArg(i));
      }
    }

    // If there are no handle arguments, continue.
    if (handle_arguments.size() == 0) {
      continue;
    }

    // Version the function.
    auto *versioned_function = versionFunction(*func, handle_arguments);
    if (versioned_function == nullptr) {
      alaska::println("Failed to version function!");
    }

    // alaska::println(*versioned_function);

    // Memoize the handle arguments.
    unsigned offset = 0;
    for (auto *handle_arg : handle_arguments) {
      auto handle_arg_index = handle_arg->getArgNo();
      auto versioned_arg = versioned_function->getArg(handle_arg_index + offset);
      offset++;
      versioned_arguments[versioned_function].push_back(versioned_arg);
    }
  }
  /*
  // Patch arguments where handle is already locked.
  for (auto &[func, locked] : locked_arguments) {
    alaska::println("function: ", func->getName());
    alaska::print("\targs:");
    for (auto b : locked) {
      alaska::print(" ", b);
    }
    alaska::println();

    for (auto *user : func->users()) {
      if (auto *call = dyn_cast<CallInst>(user)) {
        if (call->getCalledFunction() != func) continue;

        auto *called_function = call->getFunction();

        auto fLocks = alaska::extractTranslations(*called_function);
        alaska::print("\t use:");

        for (unsigned i = 0; i < locked.size(); i++) {
          bool alreadyLocked = false;

          auto argRoot = call->getRootAllocation();

          // alaska::println(i, "lock ", *argRoot);

          for (auto &fl : fLocks) {
            auto *flRoot = fl->getRootAllocation();
            if (flRoot == argRoot) {
              if (fl->liveInstructions.count(call) != 0) {
                alreadyLocked = true;
                break;
              }
            }
          }

          alaska::print(" ", alreadyLocked);
        }
        alaska::println();
      }
    }
  }
  */

  return PreservedAnalyses::none();
}
