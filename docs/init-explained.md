# Init In MyGit

This document explains how `mygit init` works in this project, what repository structure it is actually creating, and why this small command matters more than its size suggests.

Like the other subsystem guides, this is not meant to be a formal API reference.
It is meant to give you a strong mental model of the implementation.

## Short Version

`mygit init` creates the `.mygit` repository skeleton, bootstraps the branch-ref namespace, creates an empty `main` branch file plus an empty index, and writes `HEAD` so the rest of the system already knows what the current branch is before the first commit exists.

## The Mental Model

The most important thing to understand about init is this:

It does not initialize history.
It initializes **the storage model**.

After `init`, the repository still has:

- no commits
- no tree objects
- no blob objects
- no staged files

But it does have the filesystem scaffolding that makes every later command possible.

That scaffolding is not generic.
It matches this repo’s design exactly:

- object files will live under `.mygit/objects`
- branch refs will live under `.mygit/refs/heads`
- the staging area will live at `.mygit/index`
- the current branch identity will be stored in `.mygit/HEAD`

So init is really answering this question:

"What minimum repository state must exist before add, commit, checkout, branch, merge, log, and reset can even make sense?"

That is the right way to read the code.

## The Big Architectural Split

The init subsystem is split across three small pieces:

- `src/commands/init.c`
- `src/core/services.c`
- `src/helpers/file_io.c`

### `src/commands/init.c`

This is the orchestrator.

Its job is:

- validate the command shape loosely through `argc`
- create the `.mygit` root
- resolve the repository path
- create the subdirectories
- create the empty files
- seed `HEAD` with the current-branch ref path

This file answers:

"What exact repository skeleton should exist after initialization?"

### `src/core/services.c`

This file provides the low-level filesystem helpers that init leans on:

- `create_directory()`
- `generate_path()`
- `create_empty_file()`

This is the path-and-filesystem utility layer.
It hides `mkdir`, string joining, and file creation so `init.c` can read like a sequence of repository steps instead of a pile of system-call details.

### `src/helpers/file_io.c`

For init, the relevant helper is `file_io_write_text()`.

That helper matters because `HEAD` is not just an empty file.
It must immediately contain:

```text
.mygit/refs/heads/main
```

So init is not only creating files.
It is also seeding one of them with meaningful repository metadata.

## End-To-End Flow

Here is the init flow in plain English.

### 1. The router recognizes `init`

In `src/main.c`, the router checks:

```c
else if (strcmp(argv[1], "init") == 0) {
    printf("\nInit has been called. Initilizing local git repo\n");
    return init(argc);
}
```

That tells you something small but important about the design:

- the command name is validated before `init()` runs
- `init()` only receives `argc`, not `argv`

So `init()` is written as a narrow command handler, not as a general parser.

### 2. Reject extra arguments

Inside `init()`, the first guard is:

```c
if (argc > 2) {
    printf("\nToo many args\n");
    return -1;
}
```

That means the supported shape is effectively:

```text
mygit init
```

with no extra mode flags or target path support.

### 3. Create the repository root

The first real operation is:

```c
if (create_directory(".mygit") == -1) {
    failed_msg_printer(".mygit");
    return -1;
}
```

This is the command’s first hard boundary.

If `.mygit` cannot be created, nothing else is attempted.
Repeated `init` runs therefore fail immediately with the repo root already present.

### 4. Convert the repo root into an absolute base path

After creating `.mygit`, the code does:

```c
char *repo_path = realpath(".mygit", NULL);
```

This is one of the most interesting design choices in the file.

The repo is *named* relative to the current working directory, but once it exists, init switches to an absolute path for internal construction.

That means later path creation uses concrete filesystem locations like:

```text
/tmp/example/.mygit/objects
```

rather than repeatedly joining relative segments.

### 5. Create the directory skeleton

Init then creates:

- `.mygit/objects`
- `.mygit/refs`
- `.mygit/refs/heads`

The code does this in two layers:

1. use `repo_path` to create `objects` and `refs`
2. create `refs_path = generate_path(repo_path, "refs")`
3. use `refs_path` to create `heads`

So the directory structure is not created by one recursive helper.
It is built step by step in a repository-shaped order.

### 6. Create the empty files that represent baseline state

Once the directories exist, init creates:

- `.mygit/refs/heads/main`
- `.mygit/index`

Both are created as empty files.

That emptiness is meaningful.

An empty `main` ref means:

- the branch exists
- but it does not point to any commit yet

An empty index means:

- the staging area exists
- but nothing is staged yet

So init is creating "valid but blank" repository state, not dummy placeholder content.

### 7. Seed `HEAD`

Finally, init builds the path to `.mygit/HEAD` and writes:

```text
.mygit/refs/heads/main
```

This is conceptually the most important write in the whole command.

After this step, the repository already has a current branch identity even though there is still no commit history.

That is why later commands can do branch-aware work immediately.

For example:

- `commit` can ask what branch it should advance
- `branch` can ask which branch is current
- `checkout` can compare the target against the current branch
- `log` can resolve where history should begin

## The Most Important Internal Logic Sections

### Init Builds A Repository Contract

The init command is small, but it establishes a contract that the rest of the code assumes everywhere.

After init succeeds, other subsystems can rely on these paths existing:

- `.mygit/objects`
- `.mygit/refs/heads`
- `.mygit/index`
- `.mygit/HEAD`

That means many later commands do not need to debate repository layout.
They simply open the known path and continue.

So init is not just creating folders.
It is defining the on-disk API of the repository.

### The Initial Branch Is Real Even Before The First Commit

One elegant idea in this repo is that `main` exists immediately, but as an **unborn branch**.

The file `.mygit/refs/heads/main` is created empty.
Then `HEAD` points to it.

That means:

- the current branch is already `main`
- branch-aware commands already have somewhere to read or write
- but history still begins from "no commit yet"

This is a clean design because it avoids a separate "no current branch" state.

Instead of saying:

"there is no branch until the first commit"

the code says:

"there is a branch, but it currently points nowhere"

That works very well with the rest of this project’s ref model.

### Path Construction Is Deliberately Mixed: Absolute For Building, Relative For Metadata

This is one of the most interesting implementation details.

Init creates filesystem paths using absolute locations after `realpath()`:

- `/abs/path/.mygit/objects`
- `/abs/path/.mygit/refs`
- `/abs/path/.mygit/refs/heads/main`

But when it writes `HEAD`, it does **not** store that absolute path.
It stores the relative repository ref path:

```text
.mygit/refs/heads/main
```

That split is a strong design choice.

Absolute paths are good for reliable file creation during the current process.
Relative ref paths are good because the repository metadata stays portable inside the repo itself.

So init is already reflecting a bigger project rule:

- filesystem operations may use absolute paths
- repository metadata should stay repo-shaped and local

### Small Helpers Keep The Command Narrative Clear

`create_subdirectory()` and `create_file_in_directory()` are tiny wrappers, but they are doing real readability work.

Without them, `init()` would be full of repeated:

- `generate_path(...)`
- error handling
- cleanup
- `create_directory(...)`
- `create_empty_file(...)`

Instead, the command reads more like a repository-construction checklist.

That matters in an educational codebase.
It keeps the main function focused on the repository model rather than raw mechanics.

## Algorithm Spotlight: bootstrapping the repository path

The most non-obvious algorithmic move in init is the two-phase path strategy.

In plain English:

1. create `.mygit` using a relative path
2. resolve that directory to an absolute path with `realpath()`
3. build all later children from that absolute base

Pseudocode:

```text
create_directory(".mygit")
repo_path = realpath(".mygit")

create_subdirectory(repo_path, "objects")
create_subdirectory(repo_path, "refs")
refs_path = generate_path(repo_path, "refs")
create_subdirectory(refs_path, "heads")
```

Why this is worth understanding:

- it avoids repeatedly reasoning about the current working directory afterward
- it makes child path creation concrete and predictable
- it still allows the repo metadata itself to remain relative where appropriate

That last point matters because the code later writes a relative ref path into `HEAD`.

## Algorithm Spotlight: creating an unborn current branch

The other important algorithmic idea is how the repo gets a current branch before any history exists.

In plain English:

1. create an empty `main` branch file
2. create or open `HEAD`
3. write the path `.mygit/refs/heads/main` into `HEAD`

Pseudocode:

```text
create_empty_file(".mygit/refs/heads/main")
create_empty_file(".mygit/index")
write_text(".mygit/HEAD", ".mygit/refs/heads/main")
```

The invariant this creates is:

"There is always a current branch ref path, even if that branch has no commit hash yet."

That one invariant simplifies a lot of later code.

## A Worked Example

Imagine you run:

```text
mygit init
```

inside an empty project directory.

Here is the resulting structure:

```text
.mygit/
├── HEAD
├── index
├── objects/
└── refs/
    └── heads/
        └── main
```

And the important file contents are:

```text
.mygit/HEAD               -> ".mygit/refs/heads/main"
.mygit/index              -> ""
.mygit/refs/heads/main    -> ""
```

What this means operationally is:

- object storage exists, but contains nothing
- the staging area exists, but tracks nothing
- the current branch exists, but points to no commit

So right after init:

- `add` can stage files into the index
- `commit` can create the first commit and write its hash into `main`
- `branch` can list `main`
- `log` can detect that there are no commits yet

That is a very small amount of state, but it is exactly the right small amount.

## Why The Design Feels Coherent

Init feels coherent because it sets up exactly the same model the later commands expect.

It does not create one-off bootstrap structures that disappear later.
It creates the real permanent paths:

- the real object store
- the real index
- the real branch namespace
- the real `HEAD` mechanism

That is a good design for a learning-oriented VCS.

The first command is not special in kind.
It is special only because it creates the baseline state.

After that, the rest of the system is already operating on the final repository model.

## What The Subsystem Does Not Do / Current Limits

It is useful to be explicit about the limits here.

### It does not support initializing another target path

The command always initializes `.mygit` in the current working directory.

### It does not repair or reuse an existing repo

If `.mygit` already exists, init fails immediately.
There is no "already initialized, verify structure and continue" path.

### It does not roll back partial initialization

If one of the later filesystem steps fails after `.mygit` or some subdirectories were already created, the command returns failure but does not remove partially created paths.

So the recovery model is simple, not transactional.

### It does not create any objects or history

There are no blobs, trees, or commits after init.
Only the storage locations for them exist.

### It does not populate the index with baseline files

The index is created empty.
The working tree is not scanned during init.

### It does not make `HEAD` detached or ambiguous

The initial branch is always `main`.
`HEAD` always points to that branch ref path after successful init.

### It does not print a separate "creating HEAD" message

In the current implementation, `main` and `index` are created through `create_empty_file()`, which prints creation messages.
`HEAD` is written through `file_io_write_text()`, so it appears in the final structure but is not announced with the same `creating ...` log line.

That is a small detail, but it is real behavior in this repo.

## Good Next Files To Read After This

If you want to see how the initialized repository state gets used afterward, read:

1. `src/commands/add.c`
2. `src/helpers/commit_object.c`
3. `src/commands/branch.c`
4. `src/checkout/prepare.c`
5. `src/commands/log.c`

That order shows how the index, refs, and `HEAD` created by init become the foundation for staging, committing, branch management, safety checks, and history reading.
