#pragma once

#include <alaska/PointerFlowGraph.h>
#include <memory>
#include <vector>
#include <alaska/Translations.h>
#include <config.h>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"

namespace alaska {

  /**
   * TranslationBounds
   *
   * The result of a TranslationForest analysis is the locations
   * where to place calls to `translate` and `release`.
   */
  struct TranslationBounds {
    unsigned id;
    // The value that is being translated
    llvm::Value *pointer;
    // The result of the translation once it has been inserted
    llvm::Instruction *translated;
    // The location to call alaska_translation
    llvm::Instruction *translateBefore;
    // The locations to alaska_release
    std::set<llvm::Instruction *> releaseBefore;
  };

  struct TranslationForest {
    struct Node {
      Node *share_translation_with = NULL;
      Node *parent;
      // which of the TranslationBounds does this node use?
      unsigned translation_id = UINT_MAX;
      std::vector<std::unique_ptr<Node>> children;

      // which siblings does this node dominates and post dominates in the cfg.
      // This is used to share translations among multiple subtrees in the forest.
      std::set<Node *> dominates;
      std::set<Node *> postdominates;


      llvm::Value *val;
      // If this node performs a translation on incoming data, this is where it is located.
      llvm::Instruction *incoming = nullptr;
      llvm::Instruction *translated = nullptr;  // filled in by the flow forest transformation

      Node(alaska::FlowNode *val, Node *parent = nullptr);
      int depth(void);
      Node *compute_shared_translation(void);
      llvm::Instruction *effective_instruction(void);

      void set_translation_id(unsigned id) {
        translation_id = id;
        for (auto &c : children)
          c->set_translation_id(id);
      }
    };

    TranslationForest(llvm::Function &F);

    // Insert translate/release, returning them as a vector
    std::vector<std::unique_ptr<alaska::Translation>> apply(void);
    void apply(Node &n);

    std::vector<std::unique_ptr<Node>> roots;

    // Nodes in the flow forest are assigned a translation location, which after analysis is
    // completed determine the location of alaska_translate and alaska_release calls. They are
    // simply a mapping from
    std::map<unsigned, std::unique_ptr<TranslationBounds>> translations;

    // get a translation bounds by id
    TranslationBounds &get_translation_bounds(unsigned id);

    llvm::Function &func;

   private:
    // allocate a new translation bounds
    TranslationBounds &get_translation_bounds(void);
    unsigned next_translation_id = 0;
  };

};  // namespace alaska
