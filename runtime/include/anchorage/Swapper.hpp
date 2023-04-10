/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#pragma once


#include <alaska.h>
#include <alaska/internal.h>
#include <memory>
#include <unordered_set>

namespace anchorage {

  // Someone else can ask us to swap a hande in our out
  void swap_in(alaska::Mapping &m);
  void swap_out(alaska::Mapping &m);

  class SwapDevice {
   public:
    virtual ~SwapDevice() = default;

    // Force a mapping to be swapped out. Both functions return true or false depending on success.
    // swap_in: given a mapping, swap the value in using anchorage::alloc after loading the value. If this function
    // returns true, m.ptr *must* be non-null.
    virtual bool swap_in(alaska::Mapping &m) = 0;
    virtual bool swap_out(alaska::Mapping &m) = 0;
  };


  // MMAPSwapDevice - Let the kernel manage it.
  class MMAPSwapDevice : public anchorage::SwapDevice {
   public:
    ~MMAPSwapDevice() override;

    // Force a mapping to be swapped out
    bool swap_in(alaska::Mapping &m) override;
    bool swap_out(alaska::Mapping &m) override;
  };




  class Swapper {
   public:
    Swapper(std::unique_ptr<SwapDevice> &&dev)
        : m_dev(std::move(dev)){};


   private:
    std::unique_ptr<SwapDevice> m_dev;
  };

}  // namespace anchorage