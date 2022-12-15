# Design notes


## Lock hoisting
- Construct flow graph
- Prune "in" edges that don't have a matching post-dominator tree edge
- Pin at each root of the resulting forest
