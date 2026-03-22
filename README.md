# MyGit

A small Git-like version control project written in C, built to make the core ideas feel visible instead of magical.

## What This Is

MyGit is a learning-oriented local VCS.

It is not trying to compete with Git.
It is trying to make the moving parts easier to see:

- staging through an index
- blob storage
- tree construction
- commit objects
- branch refs
- checkout-style state transitions
- merge as snapshot logic

If Git sometimes feels like a black box, this project is the opposite spirit.
The box is open.

## Why This Exists

Modern version control tools are powerful, but that power can hide the underlying model.

This project exists to answer questions like:

- What does a branch really need to be?
- What does the index actually do?
- How does a commit become a tree plus metadata?
- What has to happen before checkout or reset is safe?
- How can merge work without starting from line-by-line diff magic?

The idea is simple:
build a smaller system, keep the representation readable, and learn by tracing the real code.

## Who This Is For

This repo is a good fit if you are:

- learning C and want a project with real structure
- learning version control internals
- curious about how Git-like tools are modeled under the hood
- building your own toy VCS and want reference ideas
- trying to connect high-level VCS concepts to concrete code

This repo is probably not for you if you want:

- full Git compatibility
- production-grade performance
- a polished end-user replacement for Git

## Project Spirit

The project tries to stay light:

- small enough to hold in your head
- serious enough to teach real architectural ideas
- simple enough that the storage model is inspectable by hand

That means the code usually prefers:

- clarity over feature count
- readable object formats over compact ones
- explicit state transitions over hidden abstractions

## Current Commands

MyGit currently includes:

- `-help` / `--help`
- `init`
- `add .`
- `commit -m "message"`
- `log`
- `branch`
- `checkout`
- `reset`
- `merge`

The implementation is intentionally narrower than Git, but the core ideas are there.

## Install / Build

There is no installer yet.
You build it from source.

### Requirements

- `gcc`
- `make`
- OpenSSL `libcrypto` available for linking

### Build

```bash
make
```

This produces:

```text
out/MyGit.out
```

### Clean build artifacts

```bash
make clean
make fclean
```

## Quick Start

```bash
./out/MyGit.out --help
./out/MyGit.out init
./out/MyGit.out add .
./out/MyGit.out commit -m "first commit"
./out/MyGit.out log
```

If you want a quick built-in command guide at any time:

```bash
./out/MyGit.out -help
```

## What The Repo Model Looks Like

After `init`, MyGit creates:

- `.mygit/objects/`
- `.mygit/refs/heads/`
- `.mygit/index`
- `.mygit/HEAD`

The important idea is that the storage model is intentionally plain:

- the index is a flat staged snapshot
- objects live in `.mygit/objects`
- branches are plain text ref files
- `HEAD` points to the current branch ref path

That simplicity is a feature of the project.

## How To Read This Repo

There are two good ways to use this codebase.

### 1. Use it like a small project

Build it, run commands, inspect `.mygit/`, and trace the state changes.

### 2. Use it like a guided systems-study repo

Read the docs in `docs/` alongside the code.
That is the best route if your goal is understanding architecture and logic rather than just compiling it once.

## Guide Map

The docs folder contains focused guided explanations:

- [init-explained.md](docs/init-explained.md)
- [add-explained.md](docs/add-explained.md)
- [commit-explained.md](docs/commit-explained.md)
- [branch-explained.md](docs/branch-explained.md)
- [checkout-explained.md](docs/checkout-explained.md)
- [reset-explained.md](docs/reset-explained.md)
- [merge-explained.md](docs/merge-explained.md)
- [log-explained.md](docs/log-explained.md)
- [custom-data-structures-explained.md](docs/custom-data-structures-explained.md)
- [system-architecture-explained.md](docs/system-architecture-explained.md)

## How To Use The Guides

If you want the smoothest reading path:

1. Start with [system-architecture-explained.md](docs/system-architecture-explained.md) for the big picture.
2. Read [custom-data-structures-explained.md](docs/custom-data-structures-explained.md) so `node`, `file_data`, and `checkout_entry` feel familiar.
3. Then follow the command flow:
   `init` -> `add` -> `commit` -> `branch` -> `checkout` -> `reset` -> `merge` -> `log`

If you prefer learning by feature, just open the guide for the command you are currently reading in `src/`.

## What This Project Optimizes For

MyGit is optimized for:

- understanding
- inspectability
- architectural clarity
- concrete practice with C code organization

It is not optimized for:

- Git parity
- huge repositories
- advanced merge behavior
- every possible edge case

## A Good Way To Explore

One nice way to use the repo is:

1. run a command
2. inspect `.mygit/`
3. read the matching guide
4. trace the source files behind it

That loop tends to make the architecture click pretty quickly.

## Repository Layout

At a high level:

- `src/commands/` holds top-level command orchestration
- `src/add/` holds staging logic
- `src/checkout/` holds tracked-state reconstruction and apply logic
- `src/merge/` holds merge setup and merge decision logic
- `src/helpers/` holds commit/tree/file helpers
- `src/core/` holds lower-level utilities like hashing, paths, and ignore logic
- `src/data_structures/` holds the project-specific in-memory models
- `docs/` holds the guided explanations

## Final Note

This is a repo for people who like seeing how things work.

If you want a compact project where refs are real files, objects are readable, and the architecture is small enough to study without getting lost, that is exactly what MyGit is trying to be.
