# System Architecture In MyGit

This document is intentionally denser than the subsystem guides.

It is not trying to teach one command in isolation.
It is trying to explain the architecture of the whole repository as an engineered system:

- what the central abstractions are
- why the code is split the way it is
- what trade-offs the design is making
- where the architecture is elegant
- where it is deliberately narrow
- and where future complexity would start to push back on the current shape

If the subsystem guides are the guided tours, this document is the architectural map.

## Short Version

MyGit is built around a small number of on-disk truths, and nearly every command is just a different way of translating between them: a flat staged snapshot in `.mygit/index`, immutable content-addressed objects in `.mygit/objects`, branch refs under `.mygit/refs/heads`, and a working tree that checkout-style code can transform to match any tracked snapshot.

That sentence is the system.

## The Core Architectural Bet

The deepest design choice in this repository is this:

**keep the storage model very small, then reuse it everywhere instead of inventing command-specific state representations.**

That one choice explains most of the architecture.

Instead of giving each command its own custom representation of repository state, MyGit keeps coming back to a few core forms:

1. a flat path-to-hash index
2. a hierarchical in-memory tree
3. immutable object files keyed by SHA-1
4. branch refs as plain text files
5. tracked-file snapshots flattened into `checkout_entry` arrays

Each subsystem mostly exists to translate from one of those forms into another.

That is why the code feels more coherent than a feature-by-feature implementation would.
The project is not really a pile of commands.
It is a pipeline architecture with several different entry points.

## The Repository Model

The repository’s entire data model is small enough to describe directly.

### 1. The working tree

This is the live filesystem the user sees and edits.

It is not authoritative for history.
It is only authoritative for commands that explicitly scan current files, mainly `add`.

### 2. The index

`.mygit/index` stores a flat staged snapshot:

```text
path/to/file<TAB>sha1hash
```

This is the boundary between "current files on disk" and "what we intend to commit."

Architecturally, the index is the central coordination point of the system.
It is not just a staging convenience.
It is the common representation that:

- `add` writes
- `commit` reads
- `checkout` rewrites
- `reset` rewrites
- `merge` rewrites
- repository cleanliness checks validate against current branch history

### 3. The object store

`.mygit/objects/<hash>` stores:

- blob contents
- tree payloads
- commit payloads

There is no directory fan-out by hash prefix.
There is no object header format.
There is no compression layer.
The store is intentionally simple: one file per hash in one directory.

That is a very strong educational trade-off.
It gives up scale-oriented object-store engineering in exchange for visibility and simplicity.

### 4. The refs

`.mygit/refs/heads/<branch>` stores the commit hash at the branch tip.

`.mygit/HEAD` stores the path to the current branch ref:

```text
.mygit/refs/heads/main
```

That means MyGit does not need a separate branch database.
Branch identity is plain filesystem state.

### 5. Commit and tree objects

Tree objects store lines like:

```text
blob file.txt <hash>
tree src <hash>
```

Commit objects store:

```text
tree <root-tree-hash>
branch <branch-name>
time <unix-timestamp>
parent <parent-hash-or-NULL>

<message>
```

For merge commits, there can be multiple `parent` lines.

This is not Git-compatible, but that is the point.
The format is optimized for inspectability, not interoperability.

## The Three Main State Transitions

Once you strip away command names, the system is mostly built around three transitions.

### Transition 1: working tree -> index

This is `add`.

The code scans live files, hashes them, filters out unchanged entries, writes missing blobs, and updates the flat staged snapshot.

### Transition 2: index -> objects + ref move

This is `commit`.

The code turns the flat staged snapshot into a tree, writes tree objects, writes a commit object, and moves the current branch ref.

### Transition 3: commit/tree/ref state -> working tree + index

This is `checkout`, `reset`, and most of `merge`.

The code reconstructs a tracked file snapshot from stored objects, then uses a shared application pipeline to make the working tree and index match that target.

That third transition is especially important.
It is the place where the architecture becomes more elegant than it first appears, because three seemingly different commands are actually reusing the same operational engine.

## The Real Architectural Layers

The file layout reflects a real layering strategy.

### 1. Command orchestration

Files:

- `src/main.c`
- `src/commands/*.c`

This layer does not hold the whole algorithm for each feature.
Its job is to:

- validate top-level command shape
- collect the right inputs
- call helper layers in a meaningful order
- print user-facing outcomes
- coordinate rollback on multi-step failures

This is why the command files read more like scripts than libraries.
They are orchestration code.

### Trade-off

This keeps top-level flows understandable.
The cost is that some common orchestration patterns are repeated across commands instead of being abstracted into a generic transaction framework.

That is an acceptable trade in a learning codebase.
The repetition exposes the flow clearly.

### 2. Storage and format helpers

Files:

- `src/core/hash.c`
- `src/core/services.c`
- `src/helpers/file_io.c`
- `src/core/gitignore.c`

This layer defines the low-level mechanics:

- path generation
- directory creation
- empty file creation
- small text reads and writes
- file copying
- SHA-1 hashing
- ignore-rule loading and matching

These helpers are intentionally thin.
They do not try to build a rich infrastructure framework.

### Trade-off

Thin helpers keep the code honest.
You can still see what the system is actually doing.

The cost is that there is less centralized policy enforcement.
For example, different commands still manage their own cleanup and rollback logic rather than delegating all filesystem mutation to a transactional layer.

### 3. Structural data models

Files:

- `src/data_structures/file_data.c`
- `src/data_structures/checkout_entry.c`
- `src/data_structures/node.c`

This layer is one of the strongest parts of the architecture.
The repo does not use one giant "repository state" struct.
It uses several small representations at different abstraction levels.

### `file_data`

Represents:

"I found this file in the working tree, and here is its hash."

Used by `add`.

### `node`

Represents:

"This file or directory exists in a tree hierarchy."

Used by:

- index-to-tree construction during commit
- tree-object reconstruction during checkout-style flows

### `checkout_entry`

Represents:

"This tracked path should be backed by this blob object."

Used by:

- checkout
- reset
- merge
- cleanliness checks

### Trade-off

This is a good example of *intentional multiple representations*.
The code does not force every stage into one universal model.

That reduces conceptual overload.
Each stage gets the structure that naturally fits its job.

The cost is more conversion code between forms.
But in this repository that is a good trade, because those conversions are the architecture.

### 4. Snapshot acquisition and staging

Files:

- `src/add/snapshot.c`
- `src/add/traversal.c`
- `src/add/blob_and_index.c`

This layer defines how live filesystem state becomes staged repository state.

The important point is that `add` is *not* treated as a history operation.
It is treated as staged-snapshot refresh.

That is why it compares file hashes to the index rather than to the current commit.

### Trade-off

This keeps `add` conceptually pure:

- it updates the staging area
- it does not inspect commit history

The cost is that some users coming from a more history-centric mental model might expect different comparisons.
But architecturally, this is the correct split for a staged VCS.

### 5. Tree and commit-object logic

Files:

- `src/helpers/commit_tree.c`
- `src/helpers/commit_object.c`

This layer takes staged flat state and turns it into durable history objects.

There are two key operations here:

- build or read tree identity
- build or read commit identity

The split between `commit_tree.c` and `commit_object.c` is genuinely good architecture.
It separates:

- structural snapshot formation
- historical metadata and ref movement

Those are related, but not the same problem.

### Trade-off

The split improves comprehension and reuse.
For example:

- tree logic is reused by cleanliness checks
- commit parsing is reused by `log` and merge-base search

The cost is extra conversion and indirection between layers.
Again, this is a good trade in a repo where explainability matters.

### 6. Tracked-state reconstruction and application

Files:

- `src/checkout/tree.c`
- `src/checkout/index.c`
- `src/checkout/prepare.c`
- `src/checkout/apply.c`

This layer is arguably the architectural center of the second half of the project.

It answers two system-wide questions:

1. how do we reconstruct a tracked snapshot from stored state?
2. how do we safely apply one tracked snapshot over another?

The code answers the first question by flattening commit trees into `checkout_entry` arrays.

It answers the second by:

- computing surviving entries
- validating object availability
- protecting untracked files
- deleting tracked files that should disappear
- materializing tracked files that should exist
- rewriting the index
- optionally updating a ref afterward

That apply pipeline is used directly or indirectly by:

- checkout
- reset
- merge
- fast-forward merge
- rollback during failed transitions

### Trade-off

This is probably the single best reuse decision in the repository.

Instead of implementing checkout, reset, and merge as separate file-mutation systems, the repo treats them as different *target-selection policies* feeding the same apply engine.

That keeps the risky filesystem logic centralized.

The cost is that the apply layer becomes very important and somewhat broad in scope.
It owns:

- safety
- materialization
- deletion
- index rewrite
- parts of rollback

That is a lot of responsibility, but here it is still coherent because all of those responsibilities belong to the same question:

"How do we make tracked state on disk match a chosen target snapshot?"

### 7. History analysis and merge policy

Files:

- `src/merge/prepare.c`
- `src/merge/engine.c`

This layer shows another strong architectural choice:

MyGit does not merge text.
It merges **path-to-blob choices**.

The merge engine does not care about C code, line edits, or diff hunks.
It only cares about whether, for a given path:

- current matches target
- current matches base
- target matches base
- neither side matches base

That reduction is only possible because the earlier architecture already flattened commits into `checkout_entry` arrays.

### Trade-off

This makes merge dramatically easier to reason about.
It also makes the engine reusable and testable at the path-selection level.

The cost is obvious:

- there is no content-level merge
- conflicts are coarse
- the system can only pick whole-file versions or stop

That is a feature limitation, but architecturally it is also a deliberate simplification boundary.

## The Two Big Pipelines

Most of the repository can be understood as two pipelines that meet in the middle.

### Pipeline A: toward history

```text
working tree
-> traversal
-> file_data[]
-> index
-> node tree
-> tree objects
-> commit object
-> branch ref update
```

This is the `add` plus `commit` side.

### Pipeline B: back toward a tracked snapshot

```text
branch ref or commit hash
-> commit object
-> root tree hash
-> tree objects
-> checkout_entry[]
-> apply engine
-> working tree + index
```

This is the `checkout` / `reset` / `merge` side.

The whole system becomes much easier to understand once you see that:

- `node` is the bridge from flat staged state to hierarchical tree storage
- `checkout_entry` is the bridge from hierarchical tree storage back to flat tracked application state

That is one of the cleanest ideas in the codebase.

## Why The Index Is More Central Than It First Looks

Many VCS explanations talk about the index as a staging convenience.
In this repo, it is more important than that.

The index is:

- the output of `add`
- the input of `commit`
- the local tracked-state source used by cleanliness checks
- the local tracked-state source for checkout/reset/merge transitions
- the file rewritten after every successful tracked-state transition

So the index is not just "where staged files live."
It is the repo’s flat canonical description of the current tracked snapshot.

That is why cleanliness checks compare the tree rebuilt from the index against the current commit tree hash.

The code is treating the index as a serious part of repository truth.

### Trade-off

This makes the index powerful and consistent.
But it also means many commands become stricter.

If the index drifts from the current branch tip, checkout-style commands refuse to continue.
That is a stronger model than "working tree looks fine, so go ahead."

Architecturally, that strictness is a consequence of treating the index as first-class state.

## Why Trees Exist At All In A Mostly Flat System

Another subtle design point is that MyGit is mostly flat during operational work:

- the index is flat
- checkout application is flat
- merge works on flat path lists

And yet commits still use hierarchical tree objects.

That is not accidental.
It reflects a good separation of concerns:

- flat is better for scanning, comparing, rewriting, and applying paths
- hierarchical is better for representing committed directory structure

So the system uses both, but at different phases.

### Trade-off

This is a classic conversion trade-off.

Benefits:

- simple apply logic
- simple merge logic
- still preserves real tree structure in history

Costs:

- repeated flattening and re-hierarchizing
- tree reconstruction code is unavoidable

In this repository, the benefits clearly win.
Flattened tracked state is what makes the second half of the architecture tractable.

## Why Branches Are So Simple

Branches are plain text ref files.
That sounds almost too simple, but it is a powerful architectural move.

Because of that:

- `branch` can create a branch by copying a ref file
- `commit` can advance the current branch by overwriting a ref file
- `reset` can move a branch by overwriting the current ref file
- `checkout` can switch branches by rewriting only `HEAD`

This means ref management is orthogonal to object management.
That is good.

The branch subsystem does not know how to interpret commit trees.
It does not need to.
It only manages pointers.

### Trade-off

The simplicity is excellent for readability.
The cost is that features like detached `HEAD`, ref namespaces, symbolic refs beyond `HEAD`, or richer ref validation are simply outside the current model.

Again, that is a deliberate boundary, not an accident.

## The Most Important Invariants

The architecture stays coherent because a few invariants are preserved repeatedly.

### Invariant 1: object identity comes from content

Blob, tree, and commit names are SHA-1 hashes of their payloads.

That means object identity is derived, not assigned.

### Invariant 2: `HEAD` names a branch ref path

The current branch is resolved by reading `.mygit/HEAD`, then reading the branch file it points to.

### Invariant 3: the index is repo-relative

Index paths are normalized relative to the repository root.
That makes staged state independent of the machine’s absolute filesystem layout.

### Invariant 4: checkout-style commands only run from a trustworthy tracked baseline

Before risky transitions, the repository must be clean enough that:

- tracked files are unmodified
- tracked files still exist
- the index still describes the same tree as the current branch tip

### Invariant 5: ref moves happen after object or file application work

Examples:

- commit writes the commit object before moving the branch ref
- checkout rewrites files and the index before updating `HEAD`
- reset rewrites files and the index before moving the branch ref
- merge fast-forward applies target tracked state before moving the branch ref

These are very good invariants.
They are what keep the repo from pointing at states that have not actually been materialized or stored yet.

## Safety Model: Careful, But Not Fully Transactional

The codebase has a clear safety philosophy:

**be safer than the naive implementation, but do not build a full transactional filesystem layer.**

You can see this in several places.

### Temp-file index rewrites

Index rewriting uses a temp file plus `rename()`.
That is a strong local safety improvement over in-place rewriting.

### Object-write cleanup

If object writes fail after creating a destination file, some helpers remove the partial file.

### Ordered ref updates

Ref movement tends to happen after the expensive or risky work succeeds.

### Rollback for checkout-style transitions

Checkout, reset, and merge all attempt to restore prior tracked state after partial failure.

### But not full transactions

There is no global journaling mechanism.
There is no durable multi-step commit protocol.
There is no guaranteed rollback for every partially completed filesystem mutation.

This is an important architectural trade-off.

The code is making the system *coherent enough for serious learning* without paying the complexity cost of a full transactional substrate.

That is a smart choice for the project’s goals.

## The Cleanliness Gate Is A Design Choice, Not Just A Guard

One of the most important architectural choices is the strict cleanliness gate in `checkout_prepare.c`.

Many systems could choose a weaker rule like:

"if the working tree has obvious local modifications, stop."

MyGit goes further.
It checks:

- tracked file content on disk
- tracked file existence
- index-to-current-commit tree equivalence

This matters because checkout, reset, and merge all rely on the current tracked snapshot being trustworthy before they can apply another one on top of it.

So this guard is not just caution.
It is a consequence of the architecture.

### Trade-off

Benefit:

- simpler state transitions
- fewer ambiguous cases
- safer reuse of the same apply engine

Cost:

- stricter UX
- fewer partially-dirty workflows
- less Git-like flexibility

For this repository, that is the right trade.
The code is optimizing for understandable correctness, not workflow permissiveness.

## The Major Simplifications That Make The Whole System Work

Several design simplifications are doing enormous work.

### 1. Human-readable object formats

This makes:

- commit parsing easy
- tree parsing easy
- object inspection easy
- merge-base traversal easier to debug

Cost:

- less efficient storage
- no compatibility with Git object formats
- more custom parser code

### 2. One-directory object store

This keeps object lookup simple and removes fan-out logic.

Cost:

- weaker scaling behavior
- all object files share one namespace directory

### 3. Flat tracked snapshots for application

This makes apply and merge logic much simpler.

Cost:

- repeated flattening from hierarchical state

### 4. Whole-file merge semantics

This keeps merge algorithmic rather than text-diff-oriented.

Cost:

- coarse conflicts
- no line-level merging

### 5. No detached `HEAD`

This removes a large category of edge cases.

Cost:

- less flexibility
- some Git mental models do not apply directly

### 6. No deletion staging in `add`

The current `add .` stages discovered files and refreshes index entries, but the design is still strongly biased toward present-file discovery rather than a full "compare working tree to commit and stage removals" model.

Cost:

- tracked deletion behavior is narrower than a full VCS

### 7. Strict CLI shapes

Many commands support only one or two exact invocation patterns.

Cost:

- less ergonomic
- more educational than production-like

Together, these simplifications define the project’s character.
This is not an attempt to become a mini Git clone in every detail.
It is a deliberately bounded architecture for understanding the core ideas.

## Where The Architecture Is Especially Strong

Some parts of the system are stronger than they might first appear.

### Shared apply engine

This is the best reuse seam in the codebase.

### Ref model

Branch refs plus `HEAD` as a ref path are simple and powerful.

### Representation layering

`file_data`, `node`, and `checkout_entry` are genuinely well-chosen stage-specific models.

### Tree/history split

Separating tree construction from commit-object logic was the right move.

### Path normalization

Keeping index paths repo-relative makes the model cleaner and more portable.

### Merge as snapshot selection

Given the project scope, this is exactly the right reduction.

## Where The Architecture Is Paying For Simplicity

The repo also has some costs that are worth naming clearly.

### Linear scans everywhere

Many operations are list-based:

- branch existence checks
- index lookups
- tracked entry searches
- merge path lookups
- surviving-entry computation

This keeps code simple, but it means several operations are O(n) or O(n^2).

That is acceptable for a small educational repository.
It would become a scaling pressure point in a larger system.

### Parsing is optimistic and format-coupled

Object readers assume the custom text formats are well-shaped.
Malformed objects are usually treated as failure rather than recovered from gracefully.

This keeps parsers small.
It also means the repository relies heavily on internal format discipline.

### Some rollback is best-effort

Rollback exists, which is good.
But it is not guaranteed to restore every possible partially mutated situation.

That is still much better than no rollback.
It is just important not to overstate what it provides.

### Some responsibilities are repeated across commands

The orchestration for checkout/reset/merge has repeated patterns:

- load entries
- build surviving set
- validate objects
- check untracked conflicts
- purge
- materialize
- rewrite index
- move ref or commit
- rollback on failure

This duplication is not ideal from a DRY perspective.
But it also keeps each command readable in isolation.

That is a reasonable trade in this project.

## The Architecture Through The Lens Of Command Families

The commands are not all equally different.
They fall into families.

### Family 1: repository bootstrap

- `init`

Creates the persistent storage contract.

### Family 2: staged snapshot formation

- `add`

Refreshes the index from live files.

### Family 3: history persistence

- `commit`
- `log`
- parts of `branch`

These commands are primarily about stored objects and refs rather than filesystem transformation.

### Family 4: tracked-state transitions

- `checkout`
- `reset`
- `merge`

These are the commands that actually transform the working tree and index to match another tracked snapshot.

That family structure matters because it reveals the real design:

the repo is not organized around user-facing verbs only.
It is organized around kinds of state transition.

## Why The Design Feels Coherent Overall

The architecture feels coherent because the project keeps making the same kind of decision:

take the simplest representation that preserves the core concept, and reuse it aggressively.

Examples:

- refs are plain files
- objects are plain files
- index is plain text
- trees are explicit text payloads
- merge works on flat path entries
- checkout/reset/merge share an apply model
- cleanliness checks compare tree identity instead of inventing ad hoc state checks

Nothing here is flashy.
But the pieces fit together.

That is why the codebase is good for learning.
The abstractions are visible.
They are not hidden behind a framework or a large internal platform.

## If You Wanted To Extend This System, Where Would The Pressure Show Up?

This is where the architectural trade-offs become especially concrete.

### 1. Deletion staging

Supporting full tracked deletions in `add` would force the system to answer:

- how should absent working-tree paths be represented during staging?
- should add compare against index, current commit, or both?

That would push on the current "add is only about discovered files" simplification.

### 2. Detached `HEAD`

This would push on the ref model, because `HEAD` currently assumes it stores a branch ref path, not an arbitrary commit hash.

### 3. Rename detection

Current identity is path-based plus content-hash-based.
Rename-aware behavior would require a different comparison layer.

### 4. Content-level merge

This would require a fundamentally different merge representation.
Whole-file snapshot merge would no longer be enough.

### 5. Larger repositories

Scaling would pressure:

- one-directory object store
- linear lookups
- repeated tree reconstruction
- repeated dynamic allocation patterns

### 6. Harder safety guarantees

Stronger transactional guarantees would pressure:

- multi-step filesystem mutation orchestration
- rollback design
- durable staging of ref updates

In other words, the current architecture is not accidentally small.
It is small because the project has chosen the smallest system that still exposes the essential version-control ideas.

## The Most Important Things To Keep In Your Head While Reading The Code

If you keep only these ideas in mind, the whole codebase becomes much easier to follow:

1. The index is the system’s flat tracked snapshot format.
2. Trees exist for committed structure; `checkout_entry` arrays exist for application.
3. `add` and `commit` move from live files toward durable history.
4. `checkout`, `reset`, and `merge` move from durable history back toward a tracked working snapshot.
5. Branches are plain text ref files, and `HEAD` points to one of them.
6. The checkout apply layer is shared infrastructure for most state-changing operations after commit.
7. The repository prefers explicitness and inspectability over Git compatibility or scale.

## Good Next Files To Read After This

If you want to follow the architecture in the order the concepts build on each other, read:

1. `src/commands/init.c`
2. `src/core/services.c`
3. `src/helpers/file_io.c`
4. `src/add/traversal.c`
5. `src/add/blob_and_index.c`
6. `src/helpers/commit_tree.c`
7. `src/helpers/commit_object.c`
8. `src/checkout/prepare.c`
9. `src/checkout/tree.c`
10. `src/checkout/apply.c`
11. `src/merge/prepare.c`
12. `src/merge/engine.c`

That sequence mirrors the architecture itself:

bootstrap -> stage -> persist -> reconstruct -> apply -> merge policy
