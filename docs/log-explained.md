# Log In MyGit

This document explains how `mygit log` works in this project, what it is really reading, and why the implementation is much simpler than a full Git-style history explorer.

Like the other subsystem guides, this is not meant to be a formal API reference.
It is meant to explain the logic and the design choices in a way that is easy to follow.

If you only want the short version first:

`mygit log` starts from the current branch tip, reads one commit object at a time, prints a small summary for each commit, then follows the first parent until it reaches the end of the chain.

That is the whole subsystem in one sentence.

## The Mental Model

The most important thing to understand about log is this:

Log is a **history reader**, not a tree reader.

It does not rebuild the working tree.
It does not read the index.
It does not compare snapshots.

Instead, it walks through commit objects already stored in `.mygit/objects` and asks:

- what commit am I on?
- what metadata is stored in that commit?
- what is its parent?
- what is the parent’s parent?

So log is not concerned with "what files are present."
It is concerned with "what commit chain is reachable from the current branch tip."

That is the right mental model.

## The Big Architectural Split

The log feature is split into two main pieces:

- `src/commands/log.c`
- `src/helpers/commit_object.c`

### `src/commands/log.c`

This is the orchestrator.

Its job is:

- validate the command
- resolve `HEAD` to the current branch tip
- loop through commit hashes
- ask the object parser for metadata
- print a readable summary
- choose which parent to follow next

This file answers:

"How do we walk history and present it to the user?"

### `src/helpers/commit_object.c`

This is the commit-object parsing layer.

For log, the important helper is:

- `commit_object_read_info()`

That helper reads a commit object file and turns it into structured fields:

- tree hash
- branch name
- timestamp
- parent hashes

That means `log.c` does not need to know the raw file format.
It can work with structured commit metadata instead.

That is a good split.

## End-To-End Flow

Here is the log flow in plain English.

### 1. Validate the command

The command accepts:

```text
mygit log
```

Nothing more.

This immediately tells you something about the design:

- log always starts from the current branch
- there is no alternate starting commit parameter yet

So the feature is intentionally small and focused.

### 2. Resolve `HEAD` to the current commit

The command reads:

1. `.mygit/HEAD`
2. the branch ref path stored inside `HEAD`

This gives log the current branch tip commit hash.

That is important because log does not scan all objects in the repository.
It begins from the current branch tip and follows reachable history from there.

So log is a traversal from a starting node, not a global repository index.

### 3. Stop early if there are no commits yet

If the branch ref is empty, log prints:

```text
[log] no commits yet
```

That is the simplest possible base case:

- the branch exists
- but it points to no commit

This means there is no history chain to walk.

### 4. Read one commit object at a time

The core loop is:

```c
while (current_commit_hash[0] != '\0') {
    commit_object_info info;
    ...
    if (commit_object_read_info(current_commit_hash, &info) != 0) {
        goto cleanup;
    }
    print_commit_log(current_commit_hash, &info);
    ...
}
```

Conceptually, this means:

- load the current commit’s metadata
- print it
- choose the next commit to follow
- repeat until there is no next commit

So log is a sequential commit-chain walk.

## What The Commit Reader Actually Parses

The commit-object helper expects a commit payload shaped like:

```text
tree <root-tree-hash>
branch <branch-name>
time <unix-timestamp>
parent <parent-hash-or-NULL>

<commit-message>
```

or, for merge-style commits, multiple parent lines:

```text
parent <hash1>
parent <hash2>
```

The parser reads:

- first line as tree hash
- second line as branch name
- third line as timestamp
- all following non-empty `parent ...` lines as parent hashes

The blank line ends the metadata section.

That means `commit_object_read_info()` is essentially the decoding half of the commit writer.

## Algorithm Spotlight: commit-object parsing

At the algorithm level, `commit_object_read_info()` works like this:

```text
open .mygit/objects/<commit-hash>
read tree line
read branch line
read time line

for each following line:
    trim line endings
    if line is empty:
        stop metadata parsing
    parse it as a parent line
    if parent is not NULL:
        append it to the parent list
```

This is a small but important parser because it turns a plain-text object format into structured data the rest of the code can use safely.

The command layer does not need to manually split strings or understand payload layout.
It just asks for a `commit_object_info`.

That is a good abstraction boundary.

## How Log Chooses The Next Commit

After printing one commit, log decides where to go next.

This is the crucial line of behavior:

```c
if (info.parent_count > 0) {
    next_commit_hash = duplicate_string(info.parent_hashes[0]);
}
```

That means:

- if there is at least one parent, log follows the first one
- if there are no parents, the walk ends

This is a very important design fact.

The current log implementation is a **first-parent linear walk**.

It does not branch out through every parent.
It does not print a graph.
It does not recursively explore merge history.

It chooses one path through history: the first parent chain.

## Algorithm Spotlight: first-parent traversal

At the algorithm level, the history walk is:

```text
read current branch tip

while commit hash is not empty:
    parse current commit
    print current commit summary

    if commit has at least one parent:
        current = first parent
    else:
        current = empty string
```

This is not a general graph traversal.
It is a linear linked-chain walk over the first-parent edges of the commit graph.

That design has two consequences:

### Consequence 1: simplicity

The implementation is easy to follow.
There is:

- no visited-set
- no recursion
- no graph rendering
- no ambiguity about print order

### Consequence 2: limited history visibility

If a commit has multiple parents, log only follows `parent_hashes[0]`.

So merge commits are represented only through their current printed metadata and first-parent continuation.
The second parent is parsed and stored, but it is not traversed by log.

That is a real limitation, and it is important to understand when reading the output.

## Formatting The Output

The command prints:

- the current commit hash
- the commit time, formatted if possible
- the branch name stored inside the commit object

The timestamp is stored in the object as raw Unix time text, but `print_commit_log()` tries to convert it into a human-readable local time string.

If time formatting fails, it falls back to printing the raw stored value.

That is a nice detail because it keeps the display friendly without making the command dependent on time formatting succeeding perfectly.

## Why The Branch Name In The Log Is Interesting

Each commit object stores a `branch` field.

That means the printed branch name comes from the commit payload itself, not from the currently selected branch.

This is conceptually interesting because it means:

- log is showing commit metadata as recorded at commit creation time
- not simply the branch you are standing on right now

So if history contains commits originally created on another branch and later merged or reached in a different context, the printed branch field is still describing the metadata inside that commit object.

That makes the log more like:

"show me what this commit says about itself"

than:

"show me my current branch repeatedly."

## Why Log Feels Coherent

The log subsystem works because it is extremely focused.

### `log.c`

How do we walk and print commit history from the current branch tip?

### `commit_object.c`

How do we decode one commit object into structured metadata?

That is a clean split:

- one layer traverses history
- one layer parses objects

Because the feature is so focused, it stays easy to reason about.
There is very little accidental complexity.

## What Log Does Not Do

It is useful to be explicit about the limits.

### It does not walk all parents of merge commits

It only follows the first parent.

### It does not show the commit message yet

The parser stops metadata at the blank line, but the current log output does not display the message body.

### It does not render a graph

There is no branching visual structure.

### It does not compare commits or inspect trees

It is purely a history-reader for commit metadata.

### It does not start from arbitrary commits

It always begins from the current branch tip.

These limits are not flaws by themselves.
They just define the current scope of the feature.

## A Worked Example

Imagine the current branch tip points to commit `C3`, whose first-parent chain is:

- `C3 -> C2 -> C1 -> no parent`

And suppose `C3` is a merge commit with two parents.

Log behaves like this:

1. read `HEAD`
2. read the current branch ref to get `C3`
3. parse and print `C3`
4. follow only `C3`’s first parent, say `C2`
5. parse and print `C2`
6. follow `C2`’s first parent, `C1`
7. parse and print `C1`
8. stop because `C1` has no parent

So even though `C3` had two parents, log only walks one branch of that history.

That is exactly what the current implementation promises.

## The Most Important Things To Remember While Reading The Code

If you keep only these ideas in your head, log becomes much easier to follow:

1. Log starts from the current branch tip, not from all objects.
2. It reads commit objects, not trees or the index.
3. `commit_object_read_info()` is the real decoding layer.
4. The walk is linear because log follows only the first parent.
5. Merge commits may have multiple parents, but log does not traverse them all.
6. The printed branch name comes from commit metadata, not from current runtime branch selection.
7. The subsystem is small because it does one thing: walk and print commit metadata.

## Good Next Files To Read After This

If you want to follow the feature in code order, read:

1. `src/commands/log.c`
2. `src/helpers/commit_object.c`

That is the whole conceptual pipeline for the current log implementation.
