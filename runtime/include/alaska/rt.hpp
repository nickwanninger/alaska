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


// This file defines the shared functionality between the runtime bindings, outside of the core
// library. This includes things like the C api to halloc/hfree, as well as the management of
// a centralized instance of `alaska::Runtime` for a running application.