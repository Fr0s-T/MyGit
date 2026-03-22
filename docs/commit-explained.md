# Commit In MyGit

This document explains how `mygit commit -m "message"` works in this project, why the code is split the way it is, and which parts are the real logic rather than just command plumbing.

Like the add and merge guides, this is not meant to be a formal API reference.
It is meant to explain what the system is doing and why it is designed that way.

If you only want the short version first:

`mygit commit` reads the staged snapshot from `.mygit/index`, turns that flat staged state into an in-memory tree, computes tree hashes bottom-up, writes tree objects, builds a commit object that points at the root tree, stores it in `.mygit/objects`, and finally moves the current branch ref to that new commit.

That is the whole feature in one sentence.

## The Mental Model

The most important thing to understand about commit is this:

It commits the **index**, not the working tree directly.

That means commit is not asking:

"What files exist on disk right now?"

It is asking:

"What staged snapshot does `.mygit/index` currently describe?"

That is why `add` and `commit` are separate commands in the first place.

- `add` updates the staged snapshot
- `commit` freezes that staged snapshot into history

If you keep only that idea in your head, the rest of the feature becomes much easier to understand.

## The Big Architectural Split

The commit feature is split across three main pieces:

- `src/commands/commit.c`
- `src/helpers/commit_tree.c`
- `src/helpers/commit_object.c`

That split is doing real work.

### `src/commands/commit.c`

This is the orchestrator.

Its job is:

- validate CLI input
- locate `.mygit/index`
- build the staged tree
- hand that tree plus the message to the commit-object layer
- print success or failure feedback

This file answers:

"What are the stages of the commit command?"

### `src/helpers/commit_tree.c`

This file is about turning a staged file list into a real tree object structure.

It answers:

- how do index lines become directories and files?
- how do directory hashes get computed?
- how are tree objects written?

This is the structure-building half of commit.

### `src/helpers/commit_object.c`

This file is about history and persistence.

It answers:

- what branch are we on?
- what is the current commit?
- should we reject this commit because nothing changed?
- what does the commit payload look like?
- how do we store it and move the branch ref?

This is the history-writing half of commit.

That split is good because "build the tree" and "write the commit object" are two different problems.

## End-To-End Flow

Here is the commit flow in plain English.

### 1. Validate input

The command only accepts:

```text
mygit commit -m "message"
```

So the first job is just making sure:

- the flag is `-m`
- the message exists
- the message is not empty

That part is intentionally strict and simple.

### 2. Locate the staged snapshot

`commit.c` builds the path to `.mygit/index` and passes it into the tree builder.

That is important conceptually.

Commit does not walk the filesystem the way add does.
It does not rescan files.
It trusts the index as the current staged truth.

That means if a working-tree file changed after `add`, commit still uses the staged version, not the live file on disk.

That is exactly the right behavior for a staged VCS model.

### 3. Build the tree from the index

This happens here:

```c
if (build_tree_from_index_path(index_path, 1, &root) != 0) {
    ...
}
```

This line hides a lot of the interesting logic.

What it means is:

- parse each `path<TAB>hash` index entry
- build an in-memory directory tree
- compute hashes for every directory
- write tree objects to `.mygit/objects`
- return the fully built root node

By the time this succeeds, commit already knows the root tree hash for the staged snapshot.

### 4. Turn the root tree into a commit object

After tree construction, the command calls:

```c
accept_status = accept_commit(root, commit_msg, commit_hash);
```

This is where commit stops being about tree structure and starts being about history.

The object layer then:

- reads `HEAD`
- finds the current branch ref
- reads the current branch tip
- checks whether the new tree is identical to the current commit tree
- builds the commit payload
- hashes it
- writes the commit object
- updates the branch ref

So this one helper is really the "write history" phase.

### 5. Print the result

On success, the command prints:

- the root tree hash
- the commit hash

That output is useful because it shows both:

- the snapshot identity
- the history object identity

Those are related, but not the same thing.

## The Tree Side Of Commit

The most important thing on the tree side is that the index is flat, but the commit tree is hierarchical.

An index line looks like:

```text
src/helpers/file_io.c    <hash>
```

But a commit tree needs to represent:

- a root
- a `src` directory
- a `helpers` directory inside `src`
- a file inside that directory

So commit has to transform a flat staged list into a nested in-memory tree.

That is the job of `commit_tree.c`.

## Pass One: Build The Shape Of The Tree

`build_tree_pass_one()` reads index lines and passes each one into `add_index_entry_to_tree()`.

That helper walks the path one segment at a time:

- `src`
- `helpers`
- `file_io.c`

As it walks:

- it finds existing child nodes if they already exist
- otherwise it creates them
- intermediate nodes become directories
- the final node becomes a file and receives the blob hash

So pass one is not about computing hashes yet.
It is about building the shape of the directory tree from staged paths.

## Algorithm Spotlight: path insertion into the tree

At the algorithm level, path insertion works like this:

```text
split "src/helpers/file_io.c" by "/"
start at root

for each path segment:
    if matching child already exists:
        move into it
    else:
        create a new child node and move into it

    if there are more segments ahead:
        this node must be a directory
    else:
        this node is the final file node
        attach the blob hash
```

This is really a tree-building algorithm from flat path keys.

Why this design is good:

- shared prefixes naturally become shared directory nodes
- repeated staged paths do not create duplicate directory subtrees
- the same logic works whether the file is shallow or deeply nested

So this stage is effectively converting:

- flat staged namespace

into:

- hierarchical repository tree

## Pass Two: Compute Tree Hashes Bottom-Up

Once the tree shape exists, commit still does not know the directory hashes.

A directory hash depends on the content of its children.
So the code has to work from the leaves upward.

That is what `finalize_tree_hashes()` does.

For each directory/root node:

1. recursively finalize all children
2. build a textual payload describing those children
3. hash that payload
4. optionally write a tree object file
5. store the resulting hash on the node

That means tree hashing is a bottom-up process.

Files already have blob hashes, so they are the leaves.
Directories are derived from their children.

## Algorithm Spotlight: bottom-up tree finalization

At the algorithm level:

```text
if node is a file:
    its hash already exists
    return

for each child:
    finalize child first

build string:
    "blob name hash" for file children
    "tree name hash" for directory children

hash that string
optionally write it as a tree object
store the hash on this node
```

This algorithm matters because it mirrors the repository model itself.

A directory tree hash is not independent.
It is a digest of:

- child names
- child types
- child hashes

So every directory object is really summarizing its subtree.

That is why changing one deep file ultimately changes:

- that file’s blob hash
- its parent directory hash
- its grandparent directory hash
- and finally the root tree hash

That cascading identity is the whole point of content-addressed trees.

## Why The Root Hash Matters

The root tree hash is the snapshot identity of the staged repository state.

If the root hash is the same as before, then the staged snapshot is the same as before.

That is exactly why the object layer can detect "nothing changed" without comparing every file manually.

The root tree hash is a compressed answer to:

"Is the entire staged tree meaningfully different?"

## The Commit Object Side

Once the root tree exists, the next problem is no longer structural.
Now the question becomes:

"How do we turn this snapshot into a history object?"

That is what `commit_object.c` does.

The key helper is `write_commit_to_head()`.

Its job is:

- find the current branch
- find the current commit
- decide what the parent list should be
- optionally reject the commit if the tree matches the current one
- build the commit payload
- hash and write the object
- move the current branch ref

This is where commit becomes part of history instead of just staged structure.

## Why Commit Reads HEAD First

The commit command does not write directly to some hard-coded branch.

It reads `.mygit/HEAD`, which stores the path to the current branch ref.

So commit is effectively asking:

- what branch am I currently on?
- what commit does that branch point to?

This makes commit branch-aware without needing a separate branch selection argument.

That is also why commit can update the correct branch tip after writing the new object.

## Detecting "Nothing To Do"

One of the nicest parts of the implementation is how commit avoids duplicate history.

It does not compare commit messages.
It does not compare timestamps.
It compares tree identity.

Specifically:

- read current commit
- extract its tree hash
- compare that previous tree hash to the new root tree hash

If they match, commit returns `COMMIT_NOTHING_TO_DO`.

That is exactly the right check.

Because in this repository model, the commit message alone is not enough to justify a new commit if the snapshot did not change.

## Algorithm Spotlight: nothing-to-do detection

At the algorithm level, this logic is:

```text
read current branch tip
if branch has a current commit:
    read that commit's tree hash
    if previous tree hash == new root tree hash:
        abort commit as "nothing changed"
```

Why this is strong:

- it uses the tree identity as the snapshot fingerprint
- it avoids rebuilding a per-file comparison just to detect sameness
- it aligns the "should we commit?" decision with the repository’s object model

So the code is not merely checking whether the index file text changed.
It is checking whether the staged repository tree changed.

That is a better semantic test.

## Building The Commit Payload

Once the command knows the commit should proceed, it builds a textual payload containing:

- tree hash
- branch name
- timestamp
- parent lines
- blank line
- commit message

The format looks like:

```text
tree <root-tree-hash>
branch <branch-name>
time <unix-timestamp>
parent <parent-hash-or-NULL>

<commit-message>
```

That payload is then hashed to produce the commit object name.

This is important:

the commit hash is not random.
It is derived from the payload.

So if the payload changes, the hash changes too.

That means commit identity is determined by:

- snapshot identity
- branch metadata
- parent history
- message
- time

## Algorithm Spotlight: commit object creation

At the algorithm level, commit object creation is:

```text
read HEAD to find current branch ref
read current branch tip
decide parent list
build textual payload from tree + branch + time + parents + message
hash the payload
write payload into .mygit/objects/<commit-hash>
update current branch ref to point at commit-hash
```

This algorithm is simple, but it captures the core VCS idea:

a commit is not just a message.
It is a history object that points to:

- a snapshot
- its parent history

That is why the parent line matters so much.
It is what turns isolated snapshots into a chain.

## Why The Branch Ref Update Comes Last

The branch ref is only updated after the object write succeeds.

That ordering matters.

If the branch ref moved first and object writing failed, the repository would point to a commit object that does not exist.

By writing the object first and updating the ref second, the code preserves the invariant:

"If a branch points at a commit hash, that object should already be present."

That is a very important safety property, even in a learning project.

## How Plain Commit And Merge Commit Relate

It is also worth noticing that plain commit and merge commit share the same object-writing core.

Plain commit uses:

- `accept_commit()`

Merge uses:

- `accept_commit_with_parents()`

The difference is mostly in parent handling and whether same-tree rejection is enforced.

That is a sign of good factoring.

The project does not have one commit system for normal commits and a different one for merges.
It has one commit-writing core with a slightly different parent policy.

## Why Commit Feels Coherent

The commit feature works well because each file owns a separate conceptual layer.

### `commit.c`

What are the user-visible stages of the command?

### `commit_tree.c`

How does a flat staged snapshot become a hierarchical tree object?

### `commit_object.c`

How does that snapshot become a history object and a moved branch ref?

That means commit is not one giant function that mixes:

- CLI parsing
- path tree building
- hashing
- object file writes
- branch history rules

all together.

Instead, it reads like:

staged snapshot -> tree objects -> commit object -> branch update

That is a very clean conceptual pipeline.

## What Commit Does Not Do

It is also useful to see the limits clearly.

### It does not read the working tree directly

Commit trusts the index.

### It does not commit if the staged tree matches the current commit tree

So a new message alone does not create a new commit.

### It does not need to know per-file diffs

It works at the level of:

- staged tree identity
- parent history

That keeps the command relatively simple.

## A Worked Example

Imagine the index currently contains:

```text
README.md    h1
src/main.c   h2
src/lib.c    h3
```

Commit works like this:

1. build a tree with root, `src/`, and the three file nodes
2. compute the hash of `src/` from `main.c` and `lib.c`
3. compute the root hash from `README.md` plus the `src` tree
4. compare that root hash to the current commit’s tree hash
5. if different, build the commit payload
6. hash the payload to get the commit hash
7. write the commit object
8. move the current branch ref to the new commit

So commit is really freezing the staged tree into history.

## The Most Important Things To Remember While Reading The Code

If you keep only these ideas in your head, the commit code becomes much easier to follow:

1. Commit reads the index, not the working tree.
2. The tree builder converts flat staged paths into hierarchy.
3. Directory hashes are computed bottom-up from child identities.
4. The root tree hash is the identity of the whole staged snapshot.
5. "Nothing changed" is detected by tree-hash equality.
6. A commit object is a structured payload that includes tree + parent history.
7. The branch ref only moves after the new object is safely written.

## Good Next Files To Read After This

If you want to follow the feature in code order, read:

1. `src/commands/commit.c`
2. `src/helpers/commit_tree.c`
3. `src/helpers/commit_object.c`

That order matches the actual conceptual pipeline of the command.
