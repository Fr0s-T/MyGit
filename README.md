# MyGit

A small learning-oriented version control system written in C.

## Overview

`MyGit` is a local educational project built to explore the core ideas behind version control systems by implementing a compact subset of Git-like behavior from scratch.

The project is not trying to replace Git. The goal is to understand the moving parts:

- repository initialization
- staging through an index
- blob storage
- tree construction
- commit creation
- branch reference updates

## Problem Statement

Modern version control tools are extremely powerful, but that power can hide the underlying model. This project exists to make the internals visible and concrete.

Instead of treating version control as a black box, `MyGit` exposes the core object flow directly:

1. files are hashed into blob objects
2. directories are assembled into tree objects
3. commits point to a root tree
4. branch files point to the latest commit

That makes the architecture easier to study and reason about.

## Project Goals

- build a minimal but working local version control tool
- learn how object stores and refs work
- keep the implementation readable
- prefer clarity over perfect Git compatibility

## Non-Goals

- full Git compatibility
- production-grade safety for every edge case
- large-scale performance optimization
- complete feature parity with Git

## Current Features

### `init`

Creates:

- `.mygit/`
- `.mygit/objects/`
- `.mygit/refs/`
- `.mygit/refs/heads/`
- `.mygit/refs/heads/main`
- `.mygit/index`
- `.mygit/HEAD`

`HEAD` stores a plain-text reference to:

```text
.mygit/refs/heads/main
```

### `add .`

- walks the working tree
- ignores internal directories such as `.mygit`, `.git`, `out`, and `.vscode`
- hashes regular files
- compares each file hash against the current index
- stages only changed or new files
- prints how many files were actually added
- prints `nothing changed` if there is nothing to stage

### `commit -m "message"`

- reads the index
- rebuilds an in-memory tree
- writes tree objects into `.mygit/objects`
- compares the new root tree hash to the previous commit
- aborts cleanly if nothing changed
- writes a commit object
- updates the current branch ref

## Repository Structure

```text
.
├── data_struct/
│   ├── file_data.c
│   └── node.c
├── include/
│   ├── helpers/
│   ├── add.h
│   ├── add_creating_blob_and_indexing.h
│   ├── add_traversal.h
│   ├── colors.h
│   ├── commit.h
│   ├── file_data.h
│   ├── hash.h
│   ├── init.h
│   ├── my_includes.h
│   ├── node.h
│   └── services.h
├── src/
│   ├── helpers/
│   ├── add.c
│   ├── add_creating_blob_and_indexing.c
│   ├── add_traversal.c
│   ├── commit.c
│   ├── hash.c
│   ├── init.c
│   ├── main.c
│   └── services.c
└── .mygit/
```

## Design Choices

### Split Commit Responsibilities

The commit path was separated into smaller modules:

- `src/commit.c`
  - command-level orchestration
- `src/helpers/commit_tree.c`
  - index parsing
  - tree reconstruction
  - tree object writing
- `src/helpers/commit_object.c`
  - HEAD/ref resolution
  - previous commit comparison
  - commit object creation
  - branch update

This keeps the top-level command easier to read and makes each file more focused.

### Decoupled Headers

Source files now include more specific headers instead of relying entirely on the umbrella header. That reduces hidden dependencies and makes each file’s needs clearer.

`my_includes.h` is still kept in the project as a convenience umbrella for future work.

### Simple Object Formats

Object formats are intentionally human-readable.

Tree objects use lines like:

```text
blob Makefile 3a26cc204ebc57520db5ae7efed82d7367295c5d
tree src 5750f49efa6c138b522adb27495af5dd1150f6c9
```

Commit objects use:

```text
tree <root-tree-hash>
branch <branch-name>
time <unix-timestamp>
parent <parent-hash-or-NULL>

<commit-message>
```

This is easier to inspect during development than a more compact binary or Git-compatible format.

### Local Scope First

The project is designed for local learning and experimentation. That allows some tradeoffs:

- simpler reference format
- lighter validation
- less concern for scaling
- focus on working behavior over strict compatibility

## How Staging Works

The index stores entries like:

```text
path/to/file<TAB>sha1hash
```

When `add .` runs:

1. the working tree is traversed
2. each file is hashed
3. the current index is checked for an existing hash
4. unchanged files are skipped
5. changed files are written as blobs and refreshed in the index

This prevents `add .` from reporting success on unchanged files.

## Safety Notes

The implementation now avoids a risky index replacement pattern. The index is rewritten through a temp file and then swapped into place without deleting the old index first.

For the scope of this project, that is a good balance between simplicity and safety.

## Build

```bash
make
```

Output:

```text
out/MyGit.out
```

## Example Usage

```bash
./out/MyGit.out init
./out/MyGit.out add .
./out/MyGit.out commit -m "first commit"
```

## What Is Still Missing

Reasonable future additions:

- a `log` command
- deletion tracking
- checkout-like behavior
- better malformed-object handling

## Why This Project Is Useful

This codebase helps connect high-level version control concepts to concrete implementation details:

- hashing
- object storage
- tree construction
- refs
- commit chaining

For a learning project, that is the main success criterion, and the project is already delivering on it.
