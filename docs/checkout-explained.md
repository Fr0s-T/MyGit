# Checkout In MyGit

This document explains how `mygit checkout <branch>` works in this project, why the code is split the way it is, and which parts are the real logic rather than command plumbing.

Like the other subsystem guides, this is not meant to be a formal API reference.
It is meant to explain what is happening and why the implementation is structured this way.

If you only want the short version first:

`mygit checkout` verifies that the working tree still matches the current branch, reads the target branch’s commit tree, reconstructs the tracked file snapshot for that commit, safely transforms the working tree and index to match it, and only then updates `HEAD` to point at the new branch ref.

That is the entire command in one sentence.

## The Mental Model

The most important thing to understand about checkout is this:

Checkout is a **tracked-state transition**.

It is not primarily a branch operation.
The branch name is just how the target state is chosen.

What checkout is really doing is:

1. read the currently tracked state
2. read the target tracked state
3. make the working tree and index match the target
4. update `HEAD` so the repository now considers that branch current

So the branch switch only happens safely after the file transition succeeds.

That ordering is the heart of the design.

## The Big Architectural Split

Checkout is spread across four conceptual pieces:

- `src/commands/checkout.c`
- `src/checkout/prepare.c`
- `src/checkout/tree.c`
- `src/checkout/apply.c`

### `src/commands/checkout.c`

This is the orchestrator.

Its job is:

- validate the command
- load current and target state
- call the apply helpers in the right order
- perform rollback if needed
- update `HEAD` only at the end

This file answers:

"What are the stages of checkout?"

### `src/checkout/prepare.c`

This file is the state-inspection layer.

It answers:

- is the requested branch valid?
- is the repo clean enough to allow checkout?
- what commit does the target branch point to?
- what is that commit’s root tree?

This file exists so the command file does not drown in precondition checks and ref lookups.

### `src/checkout/tree.c`

This file reconstructs a tracked snapshot from stored tree objects.

It answers:

- how do we walk a tree object recursively?
- how do we flatten it into tracked file entries?

This is what turns a commit’s tree hash into something checkout can actually apply.

### `src/checkout/apply.c`

This is the filesystem transition layer.

It answers:

- which files can stay as they are?
- which tracked files must be removed?
- which tracked files must be materialized from blobs?
- would the transition overwrite untracked files?
- how do we rollback if something fails halfway through?

This file is not specific to branch switching only.
It is the shared "apply a tracked snapshot" engine also reused by reset and merge.

## End-To-End Flow

Here is the checkout flow in plain English.

### 1. Validate the branch request

The command accepts:

```text
mygit checkout branch_name
```

Then it checks that the branch actually exists.

That is a straightforward guard, but it matters because checkout is built around resolving a branch ref to a commit.
Without a real branch ref, there is no target state to apply.

### 2. Refuse to operate on a dirty tracked state

Before checkout does anything dangerous, it calls:

```c
checkout_repo_is_up_to_date_with_branch()
```

This is one of the most important safety gates in the whole subsystem.

It checks whether the current working tree and index still agree with the current branch tip closely enough to allow a safe transition.

That includes:

- detecting modified tracked files
- detecting missing tracked files
- checking that the index still matches the current commit tree

So checkout is not merely saying:

"Are there any files around?"

It is saying:

"Is the repository still in the exact tracked state I think it is in?"

If the answer is no, checkout stops.

### 3. Resolve the target branch to a commit

Once the repo is considered safe, checkout resolves the branch name into:

- the target branch ref path
- the target commit hash
- the target root tree hash
- the on-disk path to the target root tree object

This is conceptually the "what do I want to become?" stage.

### 4. Reconstruct current and target tracked snapshots

Checkout then loads:

- current tracked entries from `.mygit/index`
- target tracked entries from the target commit tree

This is a very important design choice.

The current state comes from the index because that is the project’s tracked working snapshot.
The target state comes from the commit tree because that is what the target branch points to historically.

Once both are converted into `checkout_entry` arrays, checkout can work with two flat tracked snapshots instead of mixing ref logic, tree logic, and filesystem logic all at once.

### 5. Compute the surviving set

The next step is to find the tracked files that already match between current and target.

These entries become the **surviving** set.

They matter because checkout does not want to delete and recreate files unnecessarily.

If a tracked file already has the exact right path and blob hash, it can stay in place.

That reduces needless churn and also makes rollback logic cleaner.

### 6. Safety-check the transition

Before changing the working tree, checkout verifies:

- all referenced blob objects for current entries still exist
- all referenced blob objects for target entries still exist
- the target state would not overwrite untracked files

This means the transition is validated before destructive operations begin.

### 7. Apply the target snapshot

Once the command is convinced the transition is safe, it performs the actual state change:

1. remove current tracked files that should not survive
2. materialize target tracked files from `.mygit/objects`
3. rewrite `.mygit/index` to match the target entries

At this point the working tree and index now look like the target branch.

### 8. Update `HEAD`

Only after the tracked snapshot is fully applied does checkout call:

```c
checkout_update_head_ref(target_ref_path)
```

That is the actual branch switch.

So checkout does not "switch branch and hope the files work out later."
It makes the files right first, then updates repository identity.

That is the correct safety order.

## Why The Cleanliness Check Is More Than A Dirty-Flag Check

The `checkout_repo_is_up_to_date_with_branch()` helper is more thoughtful than a simple:

"Has anything changed?"

It verifies three things:

1. tracked files have not been modified in the working tree
2. tracked files have not disappeared from disk
3. the index still describes the same tree as the current branch tip

That third check is especially important.

Even if the working tree looks quiet, the index might have drifted away from the branch tip.
If checkout ignored that, it could apply a new branch on top of a staged-but-uncommitted tracked snapshot.

So the cleanliness check is really a consistency check across:

- working tree
- index
- current commit

## Algorithm Spotlight: repository cleanliness gate

At the algorithm level, the guard works like this:

```text
get cwd
detect modified tracked files
ensure all tracked files still exist on disk
read HEAD and current branch tip

if current branch has no commits:
    fail if index already has tracked entries
else:
    read current commit tree hash
    rebuild tree from index without writing objects
    compare rebuilt index tree hash to current commit tree hash
```

This is stronger than a normal "working tree dirty?" check because it treats the index as part of the state that must remain trustworthy before checkout is allowed.

That makes sense in this project, because checkout uses the current tracked snapshot as one half of the state transition.

## Reconstructing The Target Tree

Checkout cannot apply a branch name directly.
It needs the branch’s tracked file snapshot.

That is what `checkout_generate_tree()` does.

It reads tree-object lines like:

```text
blob file.txt <hash>
tree src <hash>
```

For directories, it recurses into the corresponding tree object.
For files, it builds tracked entries for path + blob hash.

So this subsystem converts:

- nested tree objects on disk

into:

- a flat list of tracked file entries ready for application

That flattening is exactly what makes the apply layer possible.

## Algorithm Spotlight: tree-object reconstruction

At the algorithm level, tree reconstruction works like this:

```text
open tree object
for each line:
    parse type, name, hash
    create a node under the current tree

    if line describes a subtree:
        recurse into that subtree object

    if line describes a file:
        compute its repo-relative path from the node hierarchy
        append a tracked entry for that path/hash
```

So even though the stored representation is hierarchical, checkout eventually flattens it into a path-indexed snapshot.

That is a very good design because the filesystem application stage is fundamentally path-based.

## The Apply Model

Once current entries and target entries exist, checkout is mostly an application problem.

The command does not need to think about branch history anymore.
It only needs to transition from one tracked snapshot to another.

The apply model is:

- preserve what is already correct
- remove what should disappear
- copy in what should exist
- rewrite the index to match

This is why the surviving set matters.

Checkout is not implemented as:

"delete everything and recreate everything."

It is implemented as:

"safely transform current tracked state into target tracked state."

## Untracked-File Protection

One of the most important safety features is `checkout_target_conflicts_with_untracked()`.

This protects against overwriting files or directories the repository does not currently own.

It handles several tricky cases:

- an untracked file exists exactly where checkout wants to write
- a parent path segment is an untracked file where checkout needs a directory
- a directory exists but contains untracked content that the target branch would overwrite

So the guard is not only about exact filename collisions.
It is also about path-shape conflicts and directory contamination.

Without this logic, checkout could destroy unrelated local work.

## Algorithm Spotlight: apply and rollback sequencing

Checkout’s apply logic is easiest to understand as a carefully ordered transition:

```text
1. compute surviving entries
2. verify current and target objects exist
3. refuse to overwrite untracked files
4. purge non-surviving current tracked files
5. materialize target tracked files
6. rewrite the index
7. update HEAD
```

That order is deliberate.

Why update `HEAD` last?
Because if it moved first and file application failed later, the repository would claim to be on a branch whose tracked state was not actually present in the working tree.

Rollback follows the opposite direction:

```text
purge the partially applied target state
materialize the original current state
rewrite the index back to current
restore HEAD if needed
```

So checkout is designed around preserving consistency, not just reaching the target branch in the happy path.

## Why Checkout Feels Coherent

Checkout works well because each part owns a different conceptual layer.

### `checkout.c`

What are the stages of switching branches?

### `checkout_prepare.c`

Is the repo safe to switch, and what state are we switching to?

### `checkout_tree.c`

How do we reconstruct tracked file state from commit trees?

### `checkout_apply.c`

How do we safely transform the working tree and index?

That split makes the feature readable as a pipeline:

validate -> inspect -> reconstruct -> apply -> repoint `HEAD`

That is why the implementation feels much cleaner than one giant branch-switch function would.

## What Checkout Does Not Do

It is also useful to be clear about what this command is not doing.

### It does not support detached HEAD

Checkout targets branches, not arbitrary commits.

### It does not merge local tracked changes

If the repo is not clean enough, checkout refuses to continue.

### It does not trust working-tree appearance alone

It insists that working tree, index, and current commit all line up.

That is stricter, but it keeps the state transition safer.

## A Worked Example

Imagine:

- current branch `main` tracks `a.txt` and `src/app.c`
- target branch `feature` tracks `a.txt` unchanged, deletes `src/app.c`, and adds `src/new.c`

Checkout behaves like this:

1. verify current tracked state is clean
2. read `feature`’s target commit and root tree
3. reconstruct `feature`’s tracked entries
4. mark `a.txt` as surviving because it matches on both sides
5. remove `src/app.c` because it should not survive
6. materialize `src/new.c` from its blob object
7. rewrite the index to match the feature snapshot
8. update `HEAD` to `.mygit/refs/heads/feature`

So checkout is really a tracked snapshot replacement plus a final branch identity change.

## The Most Important Things To Remember While Reading The Code

If you keep only these ideas in your head, checkout becomes much easier to follow:

1. Checkout is a tracked-state transition first and a branch switch second.
2. The repo must already match the current branch cleanly before checkout is allowed.
3. The target branch is converted into a flat tracked snapshot before application.
4. The surviving set avoids unnecessary delete/recreate churn.
5. Untracked-file protection is a major safety feature, not a minor detail.
6. `HEAD` is only updated after the file/index transition succeeds.
7. Rollback exists because checkout may begin destructive work before the last stage.

## Good Next Files To Read After This

If you want to follow the feature in code order, read:

1. `src/commands/checkout.c`
2. `src/checkout/prepare.c`
3. `src/checkout/tree.c`
4. `src/checkout/apply.c`

That order matches the conceptual pipeline of the command.
