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


  // Representation of 'regular expressions'
  namespace re {

    enum ExprTy { ExprStarTy, ExprSeqTy, ExprAltTy, ExprTokTy };
    template <typename T>
    class Expr {
     public:
      ExprTy getType(void) const { return type; }
      Expr(ExprTy type)
          : type(type) {}

      virtual ~Expr(){};

      inline friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Expr<T> &t) {
        t.print(os);
        return os;
      }

      virtual void print(llvm::raw_ostream &os){};
      virtual void flatten(void) {}

     private:
      const ExprTy type;
    };

    template <typename T>
    using ExprPtr = std::shared_ptr<Expr<T>>;

    template <typename T>
    class Star : public Expr<T> {
     public:
      Star(ExprPtr<T> &&body)
          : Expr<T>(ExprStarTy)
          , body(body) {}
      virtual ~Star() {}
      static bool classof(const Expr<T> *s) { return s->getType() == ExprStarTy; }

      virtual void print(llvm::raw_ostream &os) { os << "(" << *body << ")*"; }

      ExprPtr<T> body;
    };

    template <typename T>
    class Seq : public Expr<T> {
     public:
      Seq(std::vector<ExprPtr<T>> items = {})
          : Expr<T>(ExprSeqTy)
          , items(items) {}
      virtual ~Seq() {}
      static bool classof(const Expr<T> *s) { return s->getType() == ExprSeqTy; }
      virtual void print(llvm::raw_ostream &os) {
        // os << "(";
        for (size_t i = 0; i < items.size(); i++) {
          if (i != 0) os << " ";
          os << *items[i];
        }
        // os << ")";
      }


      virtual void flatten(void) {
        std::vector<ExprPtr<T>> newItems;
        for (auto &item : this->items) {
          item->flatten();
          // if the item is a sequence, add it's contents to the new list instead of it.
          if (auto alt = dyn_cast<Seq<T>>(item.get())) {
            for (auto &altI : alt->items) {
              newItems.push_back(altI);
            }
          } else {
            newItems.push_back(item);
          }
        }

        this->items = newItems;
      }

      std::vector<ExprPtr<T>> items;
    };


    template <typename T>
    class Alt : public Expr<T> {
     public:
      Alt(std::vector<ExprPtr<T>> items = {})
          : Expr<T>(ExprAltTy)
          , items(items) {}
      virtual ~Alt() {}
      static bool classof(const Expr<T> *s) { return s->getType() == ExprAltTy; }
      virtual void print(llvm::raw_ostream &os) {
        os << "{";
        for (size_t i = 0; i < items.size(); i++) {
          if (i != 0) os << "|";
          os << *items[i];
        }
        os << "}";
      }

      virtual void flatten(void) {
        std::vector<ExprPtr<T>> newItems;
        for (auto &item : this->items) {
          item->flatten();
          // if the item is an alternative, add it's contents to the new list instead of it.
          if (auto alt = dyn_cast<Alt<T>>(item.get())) {
            for (auto &altI : alt->items) {
              newItems.push_back(altI);
            }
          } else {
            newItems.push_back(item);
          }
        }

        this->items = newItems;
      }

      std::vector<ExprPtr<T>> items;
    };

    template <typename T>
    class Token : public Expr<T> {
     public:
      Token(T val)
          : Expr<T>(ExprTokTy)
          , val(val) {}
      virtual ~Token() {}
      static bool classof(const Expr<T> *s) { return s->getType() == ExprTokTy; }

      virtual void print(llvm::raw_ostream &os) { os << val; }

      T val;
    };


    // Construct a token from a value
    template <typename T>
    ExprPtr<T> tok(T &&val) {
      return std::make_shared<Token<T>>(std::move(val));
    }

    template <typename T>
    ExprPtr<T> seq(std::vector<ExprPtr<T>> &&v) {
      auto s = std::make_shared<Seq<T>>(std::move(v));
      s->flatten();
      return s;
    }


    template <typename T>
    ExprPtr<T> alt(std::vector<ExprPtr<T>> &&v) {
      auto a = std::make_shared<Alt<T>>(std::move(v));
      a->flatten();
      return a;
    }


    template <typename T>
    ExprPtr<T> star(ExprPtr<T> &&v) {
      return std::make_shared<Star<T>>(std::move(v));
    }


    template <typename T>
    ExprPtr<T> simplify(ExprPtr<T> v) {
      // TODO:
      return v;
    }
  };  // namespace re


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


    // this is the properties located at each node in the graph. It's not the
    struct NodeProp {
      int id = 0;
    };


    struct Edge {
      llvm::Value *accessedValue = nullptr;
      inline operator bool(void) { return accessedValue != nullptr; }
      inline bool operator==(Edge &other) { return this->accessedValue == other.accessedValue; }

      inline friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Edge &t) {
        if (t.accessedValue != nullptr) {
          t.accessedValue->printAsOperand(os, false);
        } else {
          os << "nil";
        }
        return os;
      }
    };

   private:
    // This graph provides an embedding of an access automata in the control
    // flow graph of LLVM instructions. This graph is initially way too large,
    // but is quickly reduced down to only the edges and instructions that are
    // actually important for analysis.
    alaska::DirectedGraph<llvm::Instruction *, re::ExprPtr<Edge>, NodeProp> graph;

    // Helper function to return the start and end node in the graph
    std::pair<llvm::Instruction *, llvm::Instruction *> get_start_end(void);
  };
}  // namespace alaska
