/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#pragma once

#include "llvm/IR/Value.h"

#include <alaska/Graph.h>
#include <alaska/OptimisticTypes.h>
#include <alaska/LatticePoint.h>
#include <alaska/Utils.h>

namespace alaska {


  /**
   * This class represents an access pattern to a object of type T* by encoding it as an Automata,
   * which then has reductions performed on it. The idea for this code comes from Jeon et. al.
   * (https://dl.acm.org/doi/10.5555/1759937.1759954), in which a control flow graph of a program
   * is transformed into a use-def chain where edges are annotated with field access information.
   *
   * For example, the following program:
   *
   *    char *search(int k, struct node *h) {
   *      while (h != NULL) {
   *        if (h->key == k) return h->data;
   *        h = h->next;
   *      }
   *      return NULL;
   *    }
   *
   * Would eventually be transformed into the following automata through reduction:
   *
   *    (key,next)*  (key,data + ε)
   *
   * Which indicates that this program accesses the object `h` by first acessing it's key field,
   * then it's next field. This is encoded through the use of the Klene star, which encodes "zero or
   * more repetitions". Then `h` is accessed through field key, and the data field.
   *
   * Importantly, this encodes all the possible behaviors of the function:
   *
   *    - (key next)* (key data): the function successfully finds the specific key
   *    - (key next)*: the search of the matching key failed, or the first while condition check
   *                   fails due to the null-valued head of the linked list
   *
   * Using this information, the paper interprets these 'regular expressions' of access patterns
   * to identify beneficial structures for pool allocation. In Alaska, we interpret this information
   * at *runtime* using memory object mobility
   *
   *
   * This class constructs such an Automata from some root LLVM Value, then iterates over each of
   * it's uses and constructs nodes and edges.
   */
  class AccessAutomata {
   public:
    // Construct an Access Automata for a given object given an
    // optimistic type analysis.
    AccessAutomata(llvm::Value *object, alaska::OptimisticTypes &OT);

    void dump(void);


   private:

    // this is the properties located at each node in the graph. It's not the
    struct NodeProp {
      int depth = 0;
    };


    struct Edge {
      llvm::Value *accessedValue = nullptr;
      inline operator bool(void) { return accessedValue != nullptr; }
    };

    // This graph provides an embedding of an access automata in the control
    // flow graph of LLVM instructions. This graph is initially way too large,
    // but is quickly reduced down to only the edges and instructions that are
    // actually important for analysis.
    alaska::DirectedGraph<llvm::Instruction *, Edge, NodeProp> graph;

    // Helper function to return the start and end node in the graph
    std::pair<llvm::Instruction *, llvm::Instruction *> get_start_end(void);
  };
}  // namespace alaska
