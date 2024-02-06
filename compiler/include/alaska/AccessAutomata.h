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
   *
   */
  class AccessAutomata {
   protected:
    struct Edge;

    // A State is a node along the graph of this Automata. Nominally, each State
    // is assocated with an llvm Instruction, and the in/out edges represent
    // control flow edges which are labelled with field access information.
    //
    // The complexity
    struct State {
      llvm::Instruction *inst;
      std::set<Edge*> in;
      std::set<Edge*> out;

      void add_in(const Edge &);
      void add_out(const Edge &);
    };

    // An edge represents either zero or one field access of the
    // object we are interested in. Edges without fields are removed
    // later when the Automata is undergoing reduction.
    struct Edge {
      // This edge may be an access to a field.
      std::optional<int> field_index;
      // an edge is directed `from` one node `to` another.
      State *from, *to;
    };


   public:
    // Construct an Access Automata for a given object given an
    // optimistic type analysis.
    AccessAutomata(llvm::Value *object, alaska::OptimisticTypes &OT);

    void dump(void);


   private:
    State *get_state(llvm::Instruction &inst);

    std::unordered_map<llvm::Instruction *, std::unique_ptr<State>> states;
  };



  inline void AccessAutomata::State::add_in(const AccessAutomata::Edge &e) {
    // in.insert(&e);
    // e.from->out.insert(&e);
  }

  inline void AccessAutomata::State::add_out(const AccessAutomata::Edge &e) {}
}  // namespace alaska
