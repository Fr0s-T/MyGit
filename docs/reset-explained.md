# Reset In MyGit

This document explains how `mygit reset -r <commit_hash>` works in this project, why it feels similar to checkout, and what makes it different conceptually.

Like the other subsystem guides, this is not a formal API reference.
It is an educational explanation of what is happening and why.

If you only want the short version first:

`mygit reset -r <commit>` reconstructs the tracked file snapshot for the target commit, safely transforms the working tree and index to match it, and then moves the current branch ref itself to that commit hash.

That one sentence captures the feature.

## The Mental Model

The most important thing to understand about reset is this:

Reset is **not** "checkout by commit name."

Checkout changes which branch `HEAD` points to.
Reset keeps the current branch selected and moves that branch’s tip.

So reset is really:

1. keep the current branch identity
2. choose a different commit as the target state
3. make the working tree and index match that commit
4. repoint the current branch ref to that commit

That means reset changes both:

- the present tracked snapshot
- and the branch’s recorded history position

So it is a stronger historical action than checkout.

## Why Reset Looks So Similar To Checkout

If you read the code, reset looks very close to checkout.
That is not an accident.

Both commands need to solve the same main problem:

"How do I safely transform the repository from the current tracked snapshot to a target tracked snapshot?"

That is why reset reuses the same shared helpers:

- `src/checkout/prepare.c`
- `src/checkout/tree.c`
- `src/checkout/apply.c`

The difference is not in the application layer.
The difference is in **how the target is chosen and what ref gets updated at the end**.

## The Big Architectural Split

Reset spans these main pieces:

- `src/commands/reset.c`
- `src/checkout/prepare.c`
- `src/checkout/tree.c`
- `src/checkout/apply.c`

### `src/commands/reset.c`

This is the orchestrator for reset-specific behavior.

Its job is:

- validate `mygit reset -r <commit_hash>`
- read the current branch ref
- resolve the target commit to its root tree
- reconstruct target tracked entries
- reuse the shared apply pipeline
- update the current branch ref to the target commit hash

This file answers:

"What are the stages of resetting the current branch to another commit?"

### Shared checkout helpers

Reset does not need its own tree-apply subsystem because the state-transition problem is already solved by checkout helpers.

That reuse is good architecture:

- reset differs in target selection
- reset differs in final ref update
- but the filesystem/index transition is the same kind of work

So the project avoids duplicating complicated safety logic.

## End-To-End Flow

Here is the reset flow in plain English.

### 1. Validate the command shape

Reset accepts:

```text
mygit reset -r commit_hash
```

This immediately tells you something about the command design:

- reset is commit-targeted, not branch-targeted
- the current branch remains implicit

### 2. Read the current branch ref

Reset first reads `.mygit/HEAD`, then reads the branch ref that `HEAD` points to.

That gives it:

- the path of the current branch ref file
- the current commit hash at that branch tip

This is necessary because reset will eventually overwrite that branch ref.

Unlike checkout, reset does not plan to rewrite `HEAD`.
It plans to rewrite the current branch file.

### 3. Refuse to operate on a dirty tracked state

Just like checkout, reset calls:

```c
checkout_repo_is_up_to_date_with_branch()
```

This is important for the same reason:

reset is about to remove tracked files, materialize other tracked files, and rewrite the index.

If the repository’s current tracked state is already drifting away from the current branch tip, reset would be operating on an unreliable baseline.

So reset insists on a clean and consistent starting point first.

### 4. Resolve the target commit to a tree

Reset does not resolve a branch.
It resolves a commit hash directly.

That means it asks:

- what is the root tree hash for this commit?
- where is that tree object stored?

Once that is known, reset can reconstruct the target snapshot exactly the same way checkout does.

### 5. Load current and target tracked entries

Reset then constructs:

- current tracked entries from the current index
- target tracked entries from the target commit tree

At this point the command is no longer thinking in terms of commit history.
It is thinking in terms of:

- current tracked snapshot
- target tracked snapshot

That is the same flattening trick that makes checkout and merge manageable too.

### 6. Compute the surviving set and apply the transition

From here, reset follows the same apply pattern as checkout:

1. compute surviving entries
2. verify referenced objects exist
3. refuse to overwrite untracked files
4. remove tracked files that should disappear
5. materialize tracked files that should exist
6. rewrite the index

Once this succeeds, the working tree and index match the target commit.

### 7. Move the current branch ref

This is where reset differs most sharply from checkout.

Checkout ends by updating `HEAD` to a different branch ref path.

Reset ends by writing the target commit hash into the **current branch ref file**:

```c
file_io_write_text(current_ref_path, target_commit_hash)
```

So reset does not change which branch is current.
It changes what commit that branch points to.

That is the key conceptual difference.

## The Shared Apply Model

Because reset reuses checkout’s apply helpers, it benefits from the same good properties:

- non-surviving tracked files are purged
- target tracked files are materialized from blob objects
- the index is rewritten from the resulting tracked entries
- rollback is attempted if a later stage fails

So reset is not a cheap metadata-only ref move.
It is a full tracked-state transition plus ref rewrite.

That is why the command is more powerful than a simple "branch pointer edit."

## Algorithm Spotlight: reset as a state transition plus ref move

At the algorithm level, reset works like this:

```text
read current branch ref path
read current branch tip
verify repo is clean enough
read target commit's root tree
reconstruct target tracked entries
load current tracked entries
compute surviving entries
apply target tracked snapshot to working tree and index
write target commit hash into current branch ref
```

This is the reason reset feels similar to checkout but is not the same command.

Checkout says:

"make me look like another branch, then point HEAD there."

Reset says:

"make my current branch look like another commit, then move the branch there."

That difference is small in code shape but large in meaning.

## Why The Cleanliness Gate Matters Even More For Reset

Checkout can be thought of as a branch-context change.
Reset is more invasive because it moves the current branch tip itself.

That means if reset ran on top of a dirty tracked state, the command could:

- rewrite the working tree
- rewrite the index
- move the branch ref

all relative to a baseline that was already inconsistent.

So the cleanliness gate is not optional caution.
It is part of preserving historical sanity.

Reset is allowed only when the repository’s current tracked state is trustworthy.

## Reconstructing A Commit Snapshot

Reset depends on `checkout_generate_tree()` to flatten the target commit tree into tracked file entries.

This means it reconstructs state from commit storage rather than from any live branch abstraction.

That is conceptually important.

Reset is a commit-addressed operation.
The branch is only relevant because it is the thing being moved afterward.

So the target state is derived entirely from the target commit’s tree, not from some target branch identity.

## Untracked-File Protection

Reset uses the same untracked overwrite guard as checkout.

That is exactly right.

Even though reset is a history-moving command, it still must not destroy unrelated untracked files in the working tree.

So the command stops if the target commit’s tracked snapshot would overwrite:

- an untracked file
- an untracked ancestor path segment
- an untracked directory payload

That means reset is history-aware but still working-tree-safe.

## Algorithm Spotlight: rollback after partial reset

Reset can fail after some destructive work has already started.

For example:

- purging old tracked files succeeded
- materializing target files failed
- or the branch ref update failed after file/index transition succeeded

That is why the code uses `checkout_restore_state(...)`.

At the algorithm level, rollback is:

```text
purge partially applied target tracked state
materialize the original current tracked state
rewrite the index back to current
restore current branch ref if needed
```

This does not make reset magically transactional in the database sense, but it does mean the command has a coherent recovery model instead of simply giving up halfway through.

That is a big quality difference.

## Why Reset Feels Coherent

Reset works because the project did not treat it as a totally separate magical command.

Instead it was built as:

- a new target-selection policy
- plus the shared tracked-state application engine
- plus a different final ref write

That is strong architecture.

It means the complex, risky part of the system:

- file deletion
- file materialization
- untracked protection
- rollback

is implemented once and reused.

So reset ends up being conceptually crisp:

resolve commit -> reconstruct target snapshot -> apply snapshot -> move branch tip

## What Reset Does Not Do

It is useful to be very clear about the limits here.

### It does not just move metadata

Reset also transforms the working tree and index.

### It does not detach `HEAD`

The current branch remains current.

### It does not ignore local tracked drift

If the repo is not clean enough, reset refuses to proceed.

### It does not require a target branch

The target is a commit hash directly.

That is the main way it differs from checkout.

## A Worked Example

Imagine:

- `main` currently points to commit `C3`
- `C3` tracks files `a.txt` and `src/app.c`
- older commit `C1` tracks only `a.txt`

Now run:

```text
mygit reset -r C1
```

Reset behaves like this:

1. confirm `main` is the current branch and the repo is clean
2. read the root tree for `C1`
3. reconstruct the tracked snapshot for `C1`
4. compute surviving files
5. remove `src/app.c` because it is not present in `C1`
6. ensure `a.txt` matches `C1`
7. rewrite the index to match `C1`
8. overwrite `.mygit/refs/heads/main` with `C1`

Afterward:

- working tree matches `C1`
- index matches `C1`
- `main` now points to `C1`
- `HEAD` still points to `main`

That is reset in one concrete example.

## The Most Important Things To Remember While Reading The Code

If you keep only these ideas in your head, reset becomes much easier to follow:

1. Reset is commit-targeted, not branch-targeted.
2. It keeps the current branch selected.
3. It reuses the same tracked-state apply machinery as checkout.
4. The target state comes from the target commit tree.
5. The working tree and index are updated before the branch ref is moved.
6. Rollback exists because reset may become destructive before the final ref write.
7. The real conceptual difference from checkout is the final ref update target.

## Good Next Files To Read After This

If you want to follow the feature in code order, read:

1. `src/commands/reset.c`
2. `src/checkout/prepare.c`
3. `src/checkout/tree.c`
4. `src/checkout/apply.c`

That order matches the conceptual pipeline of reset.
