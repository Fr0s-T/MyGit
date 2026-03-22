# Branch In MyGit

This document explains how `mygit branch` works in this project, what the branch subsystem is really responsible for, and why the implementation is much smaller than checkout, commit, or merge.

Like the other subsystem guides, this is not meant to be a formal API reference.
It is meant to help you build a clean mental model of the feature.

## Short Version

`mygit branch` is a very small ref-management command: it lists files in `.mygit/refs/heads`, creates a new branch by copying the current branch ref file, and deletes a branch by removing that ref file if it is not the one `HEAD` currently points to.

## The Mental Model

The most important thing to understand about branch in this project is this:

A branch is just a **plain text ref file**.

Not a special object.
Not a tree.
Not a copy of history.
Not a working-tree state.

Each branch lives under:

```text
.mygit/refs/heads/<branch-name>
```

And its content is just the commit hash at that branch tip, or an empty string if the branch has no commits yet.

The current branch is not tracked by keeping a special in-memory name somewhere.
It is tracked by `.mygit/HEAD`, whose content is a path like:

```text
.mygit/refs/heads/main
```

That means the branch subsystem is really managing a tiny ref namespace:

- list branch-ref files
- create another branch-ref file
- remove a branch-ref file

That is why the code is so much smaller than `checkout` or `merge`.
Those commands change repository state.
`branch` mostly changes repository metadata.

## The Big Architectural Split

Even though the feature is small, it still has a clear architectural split.

- `src/commands/branch.c`
- `src/helpers/file_io.c`
- `src/commands/init.c`

### `src/commands/branch.c`

This is the real subsystem.
It does almost everything:

- parses the three supported CLI shapes
- lists branches
- creates branches
- deletes branches
- exposes `branch_load_names()` for other code

This file answers:

"How do branch refs get presented and manipulated?"

### `src/helpers/file_io.c`

This file provides the tiny file primitives that make the design work.

For branch behavior, the important helpers are:

- `file_io_read_first_line()`
- `file_io_write_text()`
- `file_io_copy_file()`

Those helpers matter because branch refs in this repo are literally tiny text files.
So creating or moving branch state naturally becomes a file read, file write, or file copy.

### `src/commands/init.c`

This file creates the branch namespace that the rest of the system relies on:

- `.mygit/refs/heads/`
- `.mygit/refs/heads/main`
- `.mygit/HEAD`

That matters conceptually because `branch.c` does not invent the branch model.
It assumes the repository already uses the ref layout created by `init`.

### Shared Use Outside The Command

One nice detail is that `branch_load_names()` is not only for `mygit branch`.
Other code reuses it indirectly, especially checkout-related validation in `src/checkout/prepare.c`.

So the branch subsystem is small, but it is not isolated.
It acts as the project’s common "what branch names exist?" layer.

## End-To-End Flow

Here is the branch flow in plain English.

### 1. Parse the command shape

`input_check()` accepts exactly three forms:

```text
mygit branch
mygit branch <name>
mygit branch -d <name>
```

The parser does not return a boolean.
It returns an operation code:

- `1` means list
- `2` means create
- `3` means delete

That is a small choice, but it keeps `branch()` itself very readable:

```c
if (input_status == 1) {
    return branch_list();
}
if (input_status == 2) {
    return create_branch(name);
}
if (input_status == 3) {
    return delete_branch(name);
}
```

There is also a hard limit here:
branch names longer than 16 characters are rejected before anything else happens.

### 2. For listing, resolve the current branch first

`branch_list()` reads `.mygit/HEAD`, extracts the last path segment, and treats that as the current branch name.

So if `HEAD` contains:

```text
.mygit/refs/heads/main
```

the displayed current branch becomes `main`.

Then the command loads every entry in `.mygit/refs/heads`, prints the current branch first with a `*`, and prints the rest afterward.

The important point is that listing does **not** compare commit hashes.
It identifies the current branch from `HEAD`'s ref path.

### 3. For creation, clone the current ref file

This is the heart of the subsystem.

To create a branch, the code does not walk history.
It does not rebuild a tree.
It does not inspect commits.

Instead it:

1. loads existing branch names and refuses duplicates
2. reads `.mygit/HEAD` to find the current branch ref path
3. builds `.mygit/refs/heads/<new-name>`
4. copies the current ref file into the new ref file

That means branch creation is really "duplicate the current pointer."

If the current branch ref is empty because no commits exist yet, the new branch is also created as an empty ref.
So the new branch starts at the same historical position, even when that position is "nowhere yet."

### 4. For deletion, protect the current branch

Deletion is also simple and direct:

1. load branch names and confirm the target exists
2. read `.mygit/HEAD`
3. extract the current branch name from the ref path
4. refuse deletion if the target is the current branch
5. remove the corresponding file from `.mygit/refs/heads/`

That safety rule is the main policy in the delete path.

The command is not asking:

"Is this branch merged?"

It is asking:

"Is this the branch `HEAD` currently names?"

If yes, deletion is rejected.

## The Most Important Internal Logic Sections

### Branches Are Files, Not Special Objects

This is the single most important design choice.

Because branches are plain text files, three other behaviors become natural:

- `commit` moves a branch by overwriting the current ref file
- `checkout` switches branches by rewriting `HEAD`
- `branch` creates a branch by copying a ref file

These are different commands, but they all share the same ref model.

That is why the subsystem feels so small.
The repository representation is doing most of the conceptual work.

### `HEAD` Stores A Ref Path, Not A Branch Name

The code never stores the current branch as just `main`.
It stores:

```text
.mygit/refs/heads/main
```

That is why `extract_branch_name()` exists in `branch.c`.
The display name is derived from the path by taking the final path segment.

This is a good fit for the rest of the project too, because the same `HEAD` content can be handed directly to file helpers when another command needs to read or overwrite the current branch ref file.

So `HEAD` is not just identity metadata.
It is also an immediately usable filesystem pointer.

### `branch_load_names()` Is The Shared Core

The public helper in this subsystem is not branch creation or deletion.
It is branch-name discovery.

`branch_load_names()` opens `.mygit/refs/heads`, skips `.` and `..`, heap-allocates a string for each remaining entry, and returns the whole list.

That list is then reused for:

- listing
- duplicate detection during creation
- existence checks during deletion
- branch existence checks from checkout-related code

So even though the branch subsystem is tiny, it still has a real reusable center.

### The Namespace Is Intentionally Flat

The implementation treats branch names as top-level entries under `.mygit/refs/heads`.

That has a few consequences:

- there is no explicit support for nested branch names like `feature/x`
- there is no sorting pass
- the code assumes branch discovery is "read one directory and collect its entries"

This makes the subsystem very easy to reason about.
It is a flat directory of branch refs, not a recursive ref database.

## Algorithm Spotlight: current-branch detection

The simplest interesting algorithm in this subsystem is how it decides which branch is current.

In plain English:

- read the text stored in `.mygit/HEAD`
- find the last `/`
- use everything after that slash as the branch name

So the command is not maintaining a second source of truth.
It is deriving the display name from the same ref path the rest of the repository already uses.

Pseudocode:

```text
head_ref = read_first_line(".mygit/HEAD")
last_slash = last '/' in head_ref

if no slash or slash is final character:
    current_branch_name = head_ref
else:
    current_branch_name = text after last slash
```

That fallback behavior is also telling.
If `HEAD` ever contained a nonstandard value, the code would still try to display something sensible instead of crashing.

## Algorithm Spotlight: branch creation as ref cloning

This is the most important algorithmic idea in the subsystem.

In plain English:

- find out what ref file `HEAD` points to
- duplicate that file under the new branch name

That means "create branch" really means:

"Create another name for the exact same current commit."

Pseudocode:

```text
names = load_names(".mygit/refs/heads")
if new_name already exists:
    fail

current_ref_path = read_first_line(".mygit/HEAD")
new_ref_path = ".mygit/refs/heads/" + new_name

copy_file(current_ref_path, new_ref_path)
```

This is much cleaner than asking the branch command to understand commit objects directly.
The current branch ref already contains the answer.

## Algorithm Spotlight: listing with current-first output

The listing code does not sort or mark entries during directory traversal.
It uses a simpler two-stage presentation rule:

1. print the current branch first
2. iterate through all loaded names and skip that one

So the "starred current branch" output is really a display policy layered on top of a raw directory listing.

Pseudocode:

```text
current = extract_branch_name(read_first_line(".mygit/HEAD"))
names = load_branch_names()

print "*" + current
for each name in names:
    if name != current:
        print "  " + name
```

This means the current branch is always visually promoted, even though the underlying branch directory is not ordered.

## A Worked Example

Imagine the repository was just initialized.

So the ref state is:

```text
.mygit/HEAD               -> ".mygit/refs/heads/main"
.mygit/refs/heads/main    -> ""
```

Now run:

```text
mygit branch feature
```

What happens?

1. the command verifies that `feature` is short enough and not a duplicate
2. it reads `.mygit/HEAD` and gets `.mygit/refs/heads/main`
3. it creates `.mygit/refs/heads/feature`
4. it copies the contents of `.mygit/refs/heads/main` into that new file

Afterward the ref state is:

```text
.mygit/HEAD                  -> ".mygit/refs/heads/main"
.mygit/refs/heads/main       -> ""
.mygit/refs/heads/feature    -> ""
```

Notice what did **not** happen:

- `HEAD` did not move
- the working tree did not change
- the index did not change
- no commit objects were read

Now imagine `main` later points to commit `A1` and you run the same command again in that state.
Then `feature` would be created with `A1` inside its ref file.

So branch creation is really pointer duplication, not repository transition.

## Why The Design Feels Coherent

The branch subsystem feels coherent because it stays within its lane.

- `init` defines the ref layout
- `branch` manages names inside that layout
- `checkout` changes which ref `HEAD` points to
- `commit`, `merge`, and `reset` move ref contents

That is a strong separation of responsibilities.

The branch command never tries to switch branches.
Checkout never tries to list the whole branch namespace itself.
Commit never invents a different branch storage model.

Everything agrees on the same underlying rule:

"A branch is a file, and `HEAD` points at one of those files."

Once that rule is in place, a lot of behaviors become simple on purpose.

## What The Subsystem Does Not Do / Current Limits

It is useful to be very clear about the limits here.

### It does not switch branches

Creating a branch leaves `HEAD` unchanged.
Switching is checkout’s job.

### It does not check merge status before deletion

Deletion only blocks removing the current branch.
There is no "is this branch fully merged?" protection.

### It does not support long branch names

Names longer than 16 characters are rejected at input parsing time.

### It does not implement rename or force-delete

The only supported operations are:

- list
- create
- delete with `-d`

### It does not explicitly support nested branch namespaces

The code works with top-level entries in `.mygit/refs/heads`.
There is no recursive directory creation or recursive branch discovery.

### It does not print a normal success message for ordinary branch creation

A regular successful create returns success silently.

The one exception is a hard-coded special case:

```text
mygit branch does_it_work
```

which prints:

```text
[branch] created 'does_it_work'
yes it does.
```

That is a tiny repo-specific detail, but it is real behavior in the implementation.

### It does not sort branch listings

The current branch is printed first, but the remaining names come from directory iteration order.
So their order is not explicitly defined by the subsystem.

## Good Next Files To Read After This

If you want to follow how the rest of the repository uses this branch model, read:

1. `src/commands/checkout.c`
2. `src/checkout/prepare.c`
3. `src/helpers/commit_object.c`
4. `src/commands/reset.c`
5. `src/commands/merge.c`

That order shows how the same ref model gets reused for branch switching, commit advancement, history movement, and merge updates.
