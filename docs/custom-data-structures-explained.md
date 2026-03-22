# Custom Data Structures In MyGit

This document is about the project-specific data structures you defined, not generic C concepts.

The scope here is intentionally narrow:

- `node`
- `file_data`
- `checkout_entry`

These are the custom structs implemented in `src/data_structures/`.

The goal of this guide is to explain:

- what each struct represents in the system
- why it exists
- where it is used
- how ownership works
- how the structs relate to each other

## The Big Picture

These three structs live at different abstraction levels.

### `file_data`

This is the lightest-weight structure.
It represents:

"I found this file in the working tree, and here is its content hash."

It is mainly used in the **add / snapshot collection** side of the code.

### `checkout_entry`

This is a more repo-aware structure.
It represents:

"This tracked path should map to this blob object in `.mygit/objects`."

It is mainly used in **checkout / reset / merge** because those commands care about reconstructing tracked state from stored objects.

### `node`

This is the structural tree representation.
It represents:

"This file or directory exists in an in-memory tree hierarchy."

It is mainly used in **tree construction** and **tree parsing**:

- building trees from the index
- walking tree objects from commits
- computing and printing repository trees

So a useful way to think about them is:

- `file_data` = working-tree snapshot unit
- `checkout_entry` = tracked-file application unit
- `node` = hierarchical tree-model unit

## `file_data`

Defined in [file_data.h](/home/frost/Desktop/MyGit/include/file_data.h) and implemented in [file_data.c](/home/frost/Desktop/MyGit/src/data_structures/file_data.c).

```c
typedef struct file_data {
    char *path;
    char *hash;
} file_data;
```

### What it means

`file_data` is the project’s simplest "file record."

It stores:

- `path`: repo-relative path like `src/main.c`
- `hash`: SHA-1 hash of the file contents

That is all.

There is no object path, no tree linkage, no children, no parent pointer.

This tells you immediately what the struct is for:

it is meant for **collecting file facts**, not for reconstructing repository objects.

### Why it exists

When `add .` walks the working tree, it needs an in-memory way to say:

- I found this file
- I hashed it
- here is the result

That is exactly what `file_data` is for.

It is used before the code starts thinking in terms of blobs and commits.

In other words:

- `file_data` belongs to the "discover what exists in the working tree" stage
- not the "rebuild tracked state from stored objects" stage

### Why it is intentionally small

This struct does not store more than it needs.
That is good.

If it also stored object paths or parent directories, it would start mixing several concerns:

- working tree discovery
- object storage layout
- tree structure

By keeping it small, the code can use `file_data` as a disposable snapshot unit.

### Ownership model

`file_data_create()` copies both strings.
So the struct owns:

- `path`
- `hash`

And `file_data_destroy()` frees both.

That means callers do not need to keep the original input strings alive after creation.

### Where it is used

Its main role is in the add flow:

- traversal discovers files
- hashes are computed
- `file_data` entries are collected
- unchanged files are filtered out
- blobs/index are then written from that list

So `file_data` is the bridge between filesystem traversal and staging.

## `checkout_entry`

Defined in [checkout_entry.h](/home/frost/Desktop/MyGit/include/checkout_entry.h) and implemented in [checkout_entry.c](/home/frost/Desktop/MyGit/src/data_structures/checkout_entry.c).

```c
typedef struct s_checkout_entry {
    char *relative_path;
    char *blob_hash;
    char *object_path;
} checkout_entry;
```

### What it means

`checkout_entry` is the structure the repository uses when it already knows about tracked objects.

It stores:

- `relative_path`: where the file belongs in the repo
- `blob_hash`: which blob object contains the file contents
- `object_path`: where that blob lives on disk under `.mygit/objects`

That makes it much more operational than `file_data`.

This struct does not say "I found a file in the working tree."
It says:

"I know exactly which stored object should back this tracked path."

### Why it exists separately from `file_data`

At first glance, `file_data` and `checkout_entry` may look similar because both represent a file plus a hash.

But they are used in different worlds.

`file_data` belongs to the **discovery/staging** world.

`checkout_entry` belongs to the **application/restoration** world.

That distinction matters because checkout/reset/merge need one more thing:

they must be able to materialize a file from object storage.

That is why `object_path` exists.

Without it, every checkout-style operation would have to rebuild object paths over and over again at the call site.

By precomputing `object_path`, the struct becomes the exact unit needed for:

- validating that a blob exists
- copying it into the working tree
- comparing tracked states

### Why this struct is central to merge

Merge in this project operates on arrays of `checkout_entry`, not on raw commit objects.

That choice is very important.

Once commits are flattened into `checkout_entry` arrays, merge no longer cares about tree recursion or branch refs.
It only cares about:

- path identity
- blob identity

That makes the merge engine much simpler.

It is not merging tree objects.
It is merging lists of tracked file entries.

### The hidden design benefit of `object_path`

`object_path` may seem like a convenience field, but it is doing more than just saving a string concatenation.

It turns `checkout_entry` into a ready-to-apply record.

That means code like `checkout_materialize_target_entries()` can operate directly on entries without asking:

- where does this object live?
- how do I derive the blob file path?

The answer is already carried inside the struct.

So `checkout_entry` is not merely descriptive.
It is **actionable**.

### Comparison semantics

`checkout_entry_compare()` compares:

- `relative_path`
- `blob_hash`

That means two entries are considered the same only if:

- they refer to the same tracked path
- and they point to the same file contents

This is exactly what merge and checkout need.

If one field differs, the entry represents a different tracked state.

That is why this struct works so well as the unit for:

- surviving-file detection
- merge conflict detection
- path-level equality checks

## `node`

Defined in [node.h](/home/frost/Desktop/MyGit/include/node.h) and implemented in [node.c](/home/frost/Desktop/MyGit/src/data_structures/node.c).

```c
typedef struct s_node {
    char *name;
    char *hash;
    node_type type;
    struct s_node *parent;
    int children_count;
    int children_capacity;
    struct s_node **children;
} node;
```

### What it means

`node` is the in-memory tree representation for repository structure.

Unlike `file_data` and `checkout_entry`, which are both flat records, `node` models hierarchy.

It can represent:

- the root
- a directory
- a file
- a temporarily unresolved node while building the tree

This is why it has:

- `parent`
- `children`
- `children_count`
- `children_capacity`

This is not just "a file with metadata."
This is the project’s main tree-building structure.

### Why `node` exists

Commits in MyGit point to tree objects, and tree objects represent directories containing blobs and subtrees.

So any time the code needs to:

- build a tree from the index
- rebuild a tree from stored tree objects
- compute directory hashes bottom-up
- print a readable tree

it needs a hierarchical structure, not a flat list.

That is what `node` provides.

### Why `NODE_UNKNOWN` exists

One of the more subtle design choices is the `NODE_UNKNOWN` type.

This is not just a decorative enum member.
It supports incremental tree construction.

When the code inserts a path like:

`src/merge/engine.c`

it may create intermediate nodes before it fully knows whether a path segment is a file or directory in the finished tree.

So `NODE_UNKNOWN` acts as:

"I know this path segment exists in the tree, but I have not finalized its role yet."

Then, as more of the path is processed, the node may later become a directory or file.

That is a practical construction-state marker.

### Why parent pointers matter

The `parent` pointer is not only there for tree navigation.
It also enables `get_nodes_path()`.

That helper walks upward from a node to reconstruct the repo-relative path.

This means the tree can answer both:

- downward questions: what children does this directory contain?
- upward questions: what full path does this file belong to?

That is especially helpful when tree parsing produces file nodes deep in the hierarchy and you later need to flatten them into `checkout_entry`.

### Why children are stored in a growable array

The implementation uses:

- `children_count`
- `children_capacity`
- `children` as a heap array

This is a standard dynamic-array design.

Why that choice makes sense here:

- trees are built incrementally
- the number of children is not known in advance
- appending children is common

The code grows capacity geometrically in `node_add_child()`, which avoids reallocating on every single insert.

So even though this is a simple educational project, the structure still uses a sensible amortized-growth strategy.

### What `node->hash` means

The meaning of `hash` depends on node type:

- for file nodes, it is the blob hash
- for directory/root nodes, it becomes the tree hash after the tree is finalized
- during construction, it may temporarily be `NULL`

That makes `hash` a polymorphic field:

it always means "the object hash representing this node," but the object kind depends on whether the node is a file or tree.

That is why the combination of `type + hash` matters more than `hash` by itself.

### Why `node` is the right structure for commit building

When building a commit tree from the index, the code needs to transform flat paths like:

- `src/main.c`
- `src/helpers/file_io.c`
- `README.md`

into nested directories and files.

A flat array would be awkward for:

- computing tree hashes per directory
- emitting nested tree objects
- printing/debugging the structure

But a `node` tree handles this naturally.

So `node` is the right structure whenever the problem is:

"I need to understand or produce directory hierarchy."

## How These Three Structures Relate

A nice way to see the architecture is as a pipeline:

### Working tree discovery

`file_data`

The code walks the actual filesystem and records what exists plus each file’s content hash.

### Tree construction or parsing

`node`

The code turns flat file information into a hierarchy, or reconstructs a hierarchy from stored tree objects.

### Tracked-state application

`checkout_entry`

The code flattens tracked tree state back into ready-to-apply file records tied to object storage.

So they are not competing structures.
They are complementary structures for different phases of repository work.

## Why This Split Is Good Design

If the project tried to use one "mega struct" for everything, it would become messy fast.

Imagine one struct that tried to represent:

- discovered working-tree files
- tracked object references
- tree hierarchy
- parent/child relationships
- object storage paths

That would mix too many concerns.

By splitting the roles:

- `file_data` stays simple
- `checkout_entry` stays operational
- `node` stays structural

the codebase remains easier to reason about.

That is one of the better design choices in this repository.

## Ownership And Lifetime Summary

This is the practical memory model for these structs:

### `file_data`

- created with copied strings
- destroyed with `file_data_destroy()`
- usually owned by temporary snapshot arrays

### `checkout_entry`

- created with copied strings and a derived object path
- destroyed with `checkout_entry_destroy()`
- often owned by arrays freed with `checkout_destroy_entries()`

### `node`

- created as independent heap nodes
- ownership usually transfers to a parent through `node_add_child()`
- entire trees are freed recursively with `node_destroy()`

That ownership model is one reason the project is manageable.
Each struct has a clear destroy path.

## If You Are Reading The Code Next

The best reading order is:

1. [file_data.c](/home/frost/Desktop/MyGit/src/data_structures/file_data.c)
2. [checkout_entry.c](/home/frost/Desktop/MyGit/src/data_structures/checkout_entry.c)
3. [node.c](/home/frost/Desktop/MyGit/src/data_structures/node.c)

That order goes from simplest record to richest structure.

If you then want to see them in action, read:

1. `src/add/snapshot.c` for `file_data`
2. `src/checkout/tree.c` and `src/checkout/apply.c` for `checkout_entry`
3. `src/helpers/commit_tree.c` for `node`

That gives you both the definition and the real job each struct performs in the system.
