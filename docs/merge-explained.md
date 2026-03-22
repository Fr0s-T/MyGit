# Merge In MyGit

This document is not trying to be a formal API reference.
It is a guided explanation of what the merge code is doing, why it is split the way it is, and how to read it without getting lost in C details.

If you only want one sentence first:

`mygit merge` in this project is a snapshot-based three-way merge built on top of the same "tracked entries + apply to working tree + rewrite index" machinery that checkout and reset already use.

That single sentence explains most of the architecture.

## The Mental Model

Before reading the code, the most important idea is this:

MyGit does not merge line-by-line diffs.
It merges **file snapshots**.

For each tracked path, merge asks:

1. What did the file look like in the merge base?
2. What does it look like on the current branch?
3. What does it look like on the target branch?

Then it decides which whole-file version should survive.

That means the merge engine is not trying to combine edits inside a file.
It is deciding which blob hash should represent each path in the merged result.

So the real merge input is not "two branches".
The real input is:

- a base snapshot
- a current snapshot
- a target snapshot

And the real output is:

- a merged snapshot
- or a list of conflicting paths

Everything else in the command is support work around that core decision.

## The Big Architectural Split

The merge feature is split across three main files:

- `src/commands/merge.c`
- `src/merge/prepare.c`
- `src/merge/engine.c`

That split is good and intentional.

### `src/commands/merge.c`

This is the **orchestrator**.
It does not contain the merge algorithm itself.
Its job is:

- validate the request
- inspect repository state
- choose fast-forward vs real merge
- load the three snapshots
- call the merge engine
- apply the result to disk
- create the merge commit

In other words, this file answers:

"What are the stages of a merge command?"

### `src/merge/prepare.c`

This file is the **history and setup layer**.
It answers questions like:

- what branch are we merging?
- what commit are we on?
- is fast-forward possible?
- what is the merge base?
- how do we turn a commit into a list of tracked files?

This file exists so the command file does not drown in commit-history mechanics.

### `src/merge/engine.c`

This file is the **actual merge logic**.
It answers:

- given base/current/target versions of a path, which one wins?
- when is something a conflict?
- how are conflict paths collected?

This is the heart of the merge behavior.

### Reused checkout/apply helpers

Merge does not reinvent file application.
Instead, once the merged snapshot is decided, it reuses the checkout helpers from `src/checkout/apply.c`.

That is a strong design choice:

- merge decides **what** the repo should look like
- checkout/apply decides **how** to make the working tree and index match that target

That separation is why merge, checkout, and reset feel structurally similar.

## End-To-End Flow

Here is the merge flow in plain English.

### 1. Parse the merge mode

`merge_input_check()` in `src/merge/prepare.c` accepts three forms:

- `mygit merge branch`
- `mygit merge -i branch`
- `mygit merge -c branch`

Those modes mean:

- `MERGE_MODE_NONE`: detect conflicts and stop
- `MERGE_MODE_INCOMING`: auto-resolve conflicts by picking the target branch version
- `MERGE_MODE_CURRENT`: auto-resolve conflicts by keeping the current branch version

So the mode is not about history.
It is about the policy for unresolved paths.

### 2. Refuse to merge into a dirty repo

Very early, `merge_cmd()` calls `checkout_repo_is_up_to_date_with_branch()`.

This is a safety gate.
The merge code wants a clean starting point because the next stages may:

- delete tracked files
- recreate files from blob objects
- rewrite `.mygit/index`

If the working tree already has uncommitted tracked changes, the merge engine would no longer know whether a file difference came from branch history or from a local edit.

So the merge command insists on a stable baseline before doing anything risky.

### 3. Read both sides of the merge

The current branch state is loaded by `merge_read_current_state()`:

```c
if (file_io_read_first_line(".mygit/HEAD", current_ref_path) != 0) {
    return (-1);
}
if (file_io_read_first_line(*current_ref_path, current_commit_hash) != 0) {
    ...
}
```

This code is simple but conceptually important.

`HEAD` does not directly store a commit hash.
It stores a path to the current branch ref.

So "where am I?" is a two-step lookup:

1. read `HEAD` to find the current branch file
2. read that branch file to find the current commit hash

Then `checkout_read_target_commit()` does the same kind of lookup for the branch being merged in.

### 4. Detect easy no-op cases

Before doing any three-way logic, `merge_cmd()` checks a few cases that should end early:

- target branch has no commits
- current and target are already the same ref
- current and target already point to the same commit
- target is already an ancestor of current

That avoids doing a full merge when the answer is already known: nothing should change.

### 5. Detect fast-forward opportunities

This part is one of the most important branches in the entire file.

If current is an ancestor of target, then no merge commit is required.
Current can simply "catch up" to target.

That check happens here:

```c
fast_forward_possible = merge_commit_is_ancestor(current_commit_hash,
    target_commit_hash);
```

Conceptually:

- if current is behind target in the same history line, fast-forward is possible
- if current and target diverged, a real merge is needed

For plain `mygit merge branch`, fast-forward happens automatically.

For `-i` and `-c`, the command asks the user whether to fast-forward anyway.
That is a nice UX detail: if there is no real conflict to resolve because history is linear, the tool gives you the simpler option.

### 6. Find the merge base

If fast-forward is not used, we need a real three-way merge.

That means we need the **merge base**:
the best common ancestor of the two commits being merged.

This is handled by `merge_find_merge_base()`.

The interesting part is that the code does not assume single-parent history.
It walks parent chains recursively using `collect_commit_depths()`, which reads commit metadata through `commit_object_read_info()`.

The helper builds a set of:

- commit hash
- depth from the starting commit

Then `merge_find_merge_base()` compares the two ancestry sets and picks the shared ancestor with the smallest depth sum.

That means the chosen merge base is the common commit that is "closest overall" to both sides.

This is the key heuristic:

```c
depth_sum = left_entries[i].depth + right_entries[right_index].depth;
if (best_hash == NULL || depth_sum < best_sum
        || (depth_sum == best_sum
            && right_entries[right_index].depth < best_right_depth)) {
    best_hash = left_entries[i].hash;
    best_sum = depth_sum;
    best_right_depth = right_entries[right_index].depth;
}
```

Why this matters:

- merge logic is only meaningful relative to a base
- without a base, you cannot tell whether a side changed a file or merely kept the inherited version

### Algorithm Spotlight: merge-base search

If you want to think about this section as an algorithm rather than as code, it works like this:

```text
collect all ancestors of current with their depth
collect all ancestors of target with their depth

for each ancestor in current-history:
    if it also exists in target-history:
        score = current_depth + target_depth
        keep the common ancestor with the best score
```

The reason this works is that merge is looking for the best shared historical reference point, not merely any shared ancestor.

What makes the helper slightly more interesting is `collect_commit_depths()`:

- it walks parents recursively
- it records depth from the starting commit
- if it sees the same commit again through another parent path, it keeps the shorter depth

That last part matters because merge commits create multiple parent routes through history.
So this helper is really building a shortest-known-distance map over the commit graph.

In graph language, this is not a full general-purpose graph algorithm library.
It is a project-specific ancestor walk that is just smart enough to support merge-base selection in a commit DAG.

## How A Commit Becomes A Snapshot

The merge engine does not operate directly on commit objects.
It operates on arrays of `checkout_entry`.

That conversion happens in `merge_collect_entries_from_commit()`.

The sequence is:

1. read the commit's root tree hash
2. locate the corresponding tree object
3. recursively walk that tree object
4. flatten it into tracked entries

This is where `src/checkout/tree.c` becomes relevant.

The tree walker reads lines that look like:

```text
blob main.c 9f...
tree src a1...
```

When it sees a directory entry, it descends into the child tree object.
When it sees a blob entry, it builds a `checkout_entry` holding:

- the repo-relative path
- the blob hash
- the `.mygit/objects/<hash>` object path

That means by the time merge reaches `merge_build_entries()`, all three inputs are in the same uniform format:

- one array entry per tracked file path
- no special handling for trees anymore

This flattening is a very good architectural move.
It reduces merge to path-level reasoning instead of tree-level recursion.

## The Core Merge Algorithm

Now we reach `src/merge/engine.c`.

This is the real three-way merge logic.

### Step 1: Build the full path universe

The engine first collects every unique path that appears in any of the three snapshots:

- base
- current
- target

That is what the `all_paths` array is for.

This is crucial.
If the engine only iterated over current or target paths, it would miss deletions.

A path that existed in the base but disappeared on one side is still part of the merge problem.

### Step 2: Compare the three versions of each path

For each path, the engine finds:

- `base_entry`
- `current_entry`
- `target_entry`

Any of these may be `NULL`.

`NULL` is meaningful here.
It means "this path does not exist in this snapshot".

That is how the code represents deletion.

### Step 3: Apply the three-way decision rules

This block is the heart of the algorithm:

```c
if (entries_match(current_entry, target_entry)) {
    resolved_entry = current_entry;
}
else if (entries_match(current_entry, base_entry)) {
    resolved_entry = target_entry;
}
else if (entries_match(target_entry, base_entry)) {
    resolved_entry = current_entry;
}
else if (mode == MERGE_MODE_INCOMING) {
    resolved_entry = target_entry;
}
else if (mode == MERGE_MODE_CURRENT) {
    resolved_entry = current_entry;
}
else {
    is_conflict = 1;
}
```

This is easier to understand as a decision table.

### Algorithm Spotlight: three-way path resolution

At the algorithm level, the engine is doing this for each tracked path:

```text
look up path in base
look up path in current
look up path in target

if current == target:
    pick current
else if current == base:
    pick target
else if target == base:
    pick current
else if mode says prefer incoming:
    pick target
else if mode says prefer current:
    pick current
else:
    mark conflict
```

The beauty of this design is that it does not care whether a "version" is:

- a normal blob
- a different blob
- or no entry at all

That makes additions, modifications, and deletions all fit the same shape.

So the merge engine is really a **path-by-path three-state comparison machine**:

- same as base
- changed from base
- absent

It does not need a separate subsystem for delete handling because "absent" already carries that meaning.

### Rule A: both sides already agree

If current and target match, use that version.

This covers cases like:

- both kept the base version
- both changed to the same blob
- both deleted the file

There is nothing to resolve because history converged on the same result.

### Rule B: only target changed

If current still matches base, then current did nothing interesting.
So the merge should take target's version.

That also works for deletion:

- base had the file
- current still has the base version
- target deleted it
- result should be deletion

In that case `target_entry` is `NULL`, so `resolved_entry = target_entry` means "do not keep the path".

### Rule C: only current changed

This is the mirror image of Rule B.
If target still matches base, then target made no meaningful change and current wins.

Again, this naturally handles deletions.

### Rule D: both changed differently

This is the true conflict case.

If neither side matches base and the two sides do not match each other, the engine says:

- pick target if mode is `MERGE_MODE_INCOMING`
- pick current if mode is `MERGE_MODE_CURRENT`
- otherwise record a conflict

That is the entire conflict policy of MyGit.

It is intentionally simple:

- no content merge
- no conflict markers
- no partial file combination

Just whole-path resolution.

## Why Deletion Works Without Special Cases

This is one of the cleanest parts of the design.

Deletion is not implemented through a separate "deleted" flag.
Deletion is just "path absent from this snapshot".

That means all these cases fall out of the same logic:

- modified vs unchanged
- deleted vs unchanged
- deleted vs modified
- deleted vs deleted

Example:

- base: `notes.txt = A`
- current: `notes.txt = A`
- target: path missing

Then:

- current matches base
- target does not
- Rule B applies
- result is target, which is `NULL`

So the file disappears from the merged snapshot.

That is elegant because it keeps the engine about path identity and version equality, not about many custom state enums.

## What Happens When There Is A Conflict

In default mode, conflicts do not partially apply.

The engine records the conflicting paths in `conflict_paths`.
Then `merge_cmd()` checks:

```c
if (mode == MERGE_MODE_NONE && conflict_count > 0) {
    merge_print_conflicts(branch_name, conflict_paths, conflict_count);
    goto cleanup;
}
```

That means:

- the merge result is not written to disk
- the index is not rewritten
- no merge commit is created

This is a good safety choice.
The command only continues once it has a fully resolved snapshot.

## Applying The Merged Snapshot

Once the engine has produced `merged_entries`, the next problem is:

"How do we make the working tree and index actually match this snapshot?"

That job is handled by `merge_apply_entries()` in `src/commands/merge.c`, but it mostly delegates to checkout helpers.

This is the sequence:

1. compute surviving files
2. verify referenced blob objects exist
3. check for untracked-file conflicts
4. remove tracked files that should disappear
5. materialize target files from object storage
6. rewrite `.mygit/index`

### Surviving files

`checkout_build_surviving_entries()` finds the intersection between current entries and merged entries.

These are files already present in the working tree with the exact right content.
They do not need to be deleted and recreated.

This makes the apply phase less destructive than "delete everything, rewrite everything".

### Untracked-file protection

One of the most important safety checks is `checkout_target_conflicts_with_untracked()`.

This function protects user files that are not currently tracked by the branch but would be overwritten by the merge result.

It checks several tricky cases:

- a plain untracked file already exists at the target path
- a parent path exists as an untracked file where the merge needs a directory
- a directory exists but contains untracked content that would be overwritten

That is why the function is longer than you might expect.
It is not just checking exact file paths.
It is also checking path-shape conflicts like:

- merge wants `src/main.c`
- working tree currently has an untracked file named `src`

or:

- merge wants to write into an existing directory
- but that directory contains untracked files

Without this guard, merge could silently destroy unrelated local work.

### Algorithm Spotlight: untracked overwrite detection

This is one of the more subtle algorithms in the merge path because it is not just doing:

"does target path already exist?"

It is really checking whether applying the merged snapshot would cross into user-owned territory.

At a high level, for each target path it asks:

```text
do any ancestor path segments already exist as untracked files?
does the full target path already exist as an untracked file?
does the full target path already exist as a directory with untracked contents?
```

That means it is protecting against three classes of damage:

1. Direct overwrite
   Example: merge wants `notes.txt`, but there is already an untracked `notes.txt`.

2. Path-shape conflict
   Example: merge wants `src/main.c`, but there is an untracked file named `src`.

3. Directory contamination
   Example: merge wants to write tracked files under `assets/`, but `assets/` already contains untracked user files.

This is why `checkout_target_conflicts_with_untracked()` first walks ancestor segments, then inspects the final path, and then may recurse through directories.

The point is not merely to avoid failed writes.
The point is to avoid silently overwriting things the repository does not currently own.

### Purge, materialize, rewrite index

The apply model is:

- purge tracked things that should no longer exist
- materialize tracked things that should exist
- rewrite the index so tracked metadata matches the new snapshot

That same pattern is used elsewhere in the repository.
Merge benefits from that shared design because it is not a special snowflake.
It is another "transform current tracked state into target tracked state" operation.

### Algorithm Spotlight: apply and rollback sequencing

The apply phase is easiest to understand as a carefully ordered state transition:

```text
1. compute surviving entries
2. validate all required blob objects exist
3. refuse to overwrite untracked files
4. remove current tracked files that should disappear
5. copy merged tracked files into place
6. rewrite the index to match the merged snapshot
```

That order is deliberate.

Why not rewrite the index first?
Because then the metadata would claim the merge succeeded even if file materialization failed.

Why not materialize first and delete later?
Because stale tracked files might remain and cause the working tree to contain paths that are no longer part of the merged snapshot.

Rollback follows the same model in reverse:

```text
purge the partially applied target state
materialize the original current state
rewrite the index back to current
```

So the merge code is not just "do the merge."
It is "move from one tracked snapshot to another with the least unsafe window possible."

## Fast-Forward Is Still An Apply Operation

A nice detail in this implementation is that fast-forward does not only move the branch ref.

It still uses `merge_apply_entries()` first.

That means a fast-forward merge updates:

- the working tree
- the index
- and only then the current branch ref

So even though no merge commit is created, the on-disk repo is still made consistent with the target snapshot.

This is exactly the right mental model:

fast-forward is not "skip merge work".
It is "the merged snapshot is exactly the target snapshot, so no new commit object is needed".

## Writing The Merge Commit

If the merge was not a fast-forward, and conflicts are resolved, the final step is to write a commit with two parents.

This happens here:

```c
parent_hashes[0] = current_commit_hash;
parent_hashes[1] = target_commit_hash;
accept_commit_with_parents(root, merge_message, parent_hashes, 2,
    merge_commit_hash);
```

The merge result tree is first rebuilt from the rewritten index:

```c
build_tree_from_index_path(".mygit/index", 1, &root)
```

This is an important design choice.

The merge code does not build the commit tree directly from `merged_entries`.
Instead, it commits what is actually in the staged index after the apply phase.

That makes the commit match the repository state that was successfully applied, not just the intended in-memory result.

Then `accept_commit_with_parents()` writes a commit payload with two `parent` lines.

The payload format looks like:

```text
tree <root-tree-hash>
branch <branch-name>
time <unix-timestamp>
parent <current-commit>
parent <target-commit>

Merge branch 'feature'
```

That two-parent structure is what makes the commit a real merge commit in this repository model.

## Rollback Strategy

Merge is careful about failure after the working tree has started changing.

There are two especially risky points:

- after apply has changed files/index but before commit tree build succeeds
- after apply succeeds but writing the merge commit fails

In both cases the code calls `checkout_restore_state(...)`.

That rollback works by:

1. purging the partially applied target state
2. materializing the original current state
3. rewriting the index back to current

This is not perfect transactional storage, but it is a meaningful safety layer.
The code is trying hard not to leave the repository half-merged.

## Why The Design Feels Coherent

The merge architecture works well because it treats merge as a composition of smaller existing ideas:

### History inspection

Handled in `merge_prepare.c`.

### Snapshot reconstruction

Handled by commit-object reading plus checkout-tree expansion.

### Per-path merge reasoning

Handled in `merge_engine.c`.

### Working tree/index application

Handled by checkout helpers.

### Commit creation

Handled by commit-object helpers.

This is much better than writing one giant merge function that:

- parses history
- merges paths
- edits the filesystem
- rewrites the index
- writes the commit

all in one place.

The current split makes it possible to understand merge in layers.

## A Worked Example

Imagine this history:

- base commit tracks `a.txt = hello`
- current branch changes `a.txt` to `hello from main`
- target branch changes `a.txt` to `hello from feature`

The engine sees:

- `base_entry`: blob for `hello`
- `current_entry`: blob for `hello from main`
- `target_entry`: blob for `hello from feature`

Then:

- current != target
- current != base
- target != base

So:

- default mode: conflict
- `-i`: pick target
- `-c`: pick current

Now a deletion example:

- base tracks `old.txt`
- current leaves it unchanged
- target deletes it

Then:

- current == base
- target is `NULL`

So the result is `NULL`, which means `old.txt` is removed from the merged snapshot.

That is the same algorithm, not a separate delete subsystem.

## The Most Important Things To Remember While Reading The Code

If you keep only these ideas in your head, the merge code becomes much easier to follow:

1. Merge in this project is snapshot-based, not line-based.
2. A missing path is how deletion is represented.
3. `merge.c` is orchestration, not the merge algorithm itself.
4. `merge_prepare.c` is about history and snapshot loading.
5. `merge_engine.c` is the actual three-way decision table.
6. Applying a merged result is intentionally reused from checkout/reset machinery.
7. A merge commit is just a normal commit object with two parent lines.

## Good Next Files To Read After This

If you want to continue the merge tour in code order, read these next:

1. `src/commands/merge.c`
2. `src/merge/prepare.c`
3. `src/merge/engine.c`
4. `src/checkout/apply.c`
5. `src/checkout/tree.c`
6. `src/helpers/commit_object.c`

That order matches the way the feature is mentally layered.
