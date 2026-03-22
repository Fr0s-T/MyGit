# Add In MyGit

This document explains how `mygit add .` works in this project, why the code is split the way it is, and which parts are the real algorithms rather than just plumbing.

Like the merge guide, this is not meant to read like a formal API document.
It is meant to help you build a strong mental model of the feature.

If you only want the short version first:

`mygit add .` walks the working tree, ignores repo-internal or ignored paths, hashes regular files, throws away entries that are already identical in the index, writes any missing blob objects, and then rewrites the index so tracked paths point at the latest content hashes.

That one sentence is the whole feature in compressed form.

## The Mental Model

The most important thing to understand about `add` is this:

It does **not** create commits.
It does **not** build history.
It does **not** compare against the last commit directly.

Instead, it refreshes the **staging area**.

The staging area in this project is `.mygit/index`, and each line looks like:

```text
path/to/file<TAB>sha1hash
```

So `add` is really answering this question:

"What should the index say the current tracked file snapshot is?"

That makes `add` a bridge between:

- the current working tree on disk
- and the staged snapshot in `.mygit/index`

Once you understand that, the architecture makes sense.

## The Big Architectural Split

The add feature is split across three main files:

- `src/commands/add.c`
- `src/add/snapshot.c`
- `src/add/traversal.c`
- `src/add/blob_and_index.c`

### `src/commands/add.c`

This is the command orchestrator.
It does very little heavy lifting itself.

Its job is:

- validate that the user ran `mygit add .`
- get the current working directory
- ask the snapshot layer for changed files
- stop early if nothing changed
- hand the changed list to the blob/index writer

So this file answers:

"What are the stages of the add command?"

### `src/add/snapshot.c`

This file defines what "changed files" means in the project.

It does two big things:

- collects all candidate files from the working tree
- filters out files that are already identical in the index

This is where add becomes smart instead of blindly restaging everything.

### `src/add/traversal.c`

This file is the filesystem-walking layer.

It is responsible for:

- recursive directory traversal
- skipping repo-internal directories
- consulting `.mygitignore`
- hashing regular files
- turning absolute paths into repo-relative paths

This file exists so the snapshot layer can think in terms of "what files did we find?" rather than raw `opendir/readdir/stat` details.

### `src/add/blob_and_index.c`

This file is the persistence layer for staging.

It is responsible for:

- ensuring a blob object exists for each changed file
- updating `.mygit/index`

So this file answers:

"Now that we know what changed, how do we store it?"

## End-To-End Flow

Here is the add flow in plain English.

### 1. Validate the command shape

`add.c` only supports:

```text
mygit add .
```

Not:

- `mygit add file.txt`
- `mygit add src/`
- `mygit add -A`

So the current design is intentionally simple:
the whole feature is built around staging the entire working tree snapshot.

### 2. Determine the repository root

The command uses `getcwd()` and passes that absolute path into the rest of the add pipeline.

That path becomes the anchor for:

- traversal
- path normalization
- locating `.mygit/index`
- locating `.mygit/objects`

So even though many paths later become repo-relative, the add command still begins from an absolute filesystem root.

### 3. Load ignore rules and walk the tree

`add_collect_changed_files()` does this:

```c
if (gitignore_load(cwd, &ignore) == -1) {
    return (-1);
}
if (traverse_directory(cwd, &files, &len_files, (char *)cwd, &ignore) == -1) {
    ...
}
if (filter_unchanged_files(files, &len_files, cwd) == -1) {
    ...
}
```

This sequence is the conceptual center of the add pipeline:

1. load ignore behavior
2. gather candidate files
3. shrink the list to only meaningful changes

That is why `snapshot.c` is the real "what counts as changed?" layer.

### 4. Turn discovered files into staged objects

Once the command has a list of changed files, it calls:

```c
create_blob_and_indexing(files, len_files, cwd);
```

That helper does two things for each changed file:

- make sure the blob exists in `.mygit/objects`
- rewrite or append the matching line in `.mygit/index`

If nothing changed, add exits early and prints `[add] nothing changed`.

That means the command is designed to be idempotent.
Running `add .` repeatedly on an unchanged tree does not keep doing fake work.

## What Counts As "Changed"

This is one of the most important ideas in the add implementation.

The command does **not** ask:

"Is this file different from the last commit?"

It asks:

"Does this file's current content hash differ from what the index already says?"

That difference matters.

Because the index is the staging area, `add` is comparing:

- working tree now
- staged tree now

not:

- working tree now
- historical commit tree

That makes the feature behave like a real staging refresh rather than a commit comparison tool.

## Traversal: What The Command Actually Scans

The traversal layer recursively walks from the repo root.

But it does not track everything.

Some directories are skipped immediately:

- `.mygit`
- `.git`
- `out`
- `.vscode`

That hard-coded skip list exists because some directories are not part of the project content model at all:

- repo internals
- git internals
- build artifacts
- editor metadata

This is a structural skip, not a user-level ignore rule.

Then `.mygitignore` adds a second filtering layer on top.

## Algorithm Spotlight: recursive traversal and normalization

At the algorithm level, traversal is doing this:

```text
open directory
for each entry:
    skip hard-coded internal names
    resolve absolute path
    stat the path
    normalize it back to a repo-relative path
    ask ignore logic whether to skip it

    if entry is a directory:
        recurse
    else if entry is a regular file:
        hash it
        append it to the candidate list
```

The important detail is that the code deliberately moves between two path worlds:

- absolute/resolved paths for filesystem operations
- repo-relative paths for index storage

That is why traversal first uses `realpath()` and `stat()`, then later calls `normalize_path()`.

This is a good design because the filesystem wants absolute paths, but the repository model wants portable relative paths in the index.

So traversal is not just "walk folders."
It is a translation layer between the real filesystem and the repository’s internal path model.

## `.mygitignore` Behavior

The ignore behavior is more thoughtful than a simple filename blacklist.

The code supports:

- blank lines
- comment lines
- directory-only rules ending in `/`
- basename pattern matching
- path-aware pattern matching when the rule contains `/`

But the most interesting rule is this:

an ignored path is **not** skipped if doing so would hide something already tracked in the index.

That matters for directories especially.
If a directory matches an ignore rule but it contains tracked descendants, traversal keeps walking it.

That protects the repository from accidentally "forgetting" tracked files just because a broad ignore rule now matches their parent directory.

## Algorithm Spotlight: ignore rules with tracked descendants

The ignore algorithm is not just:

```text
if rule matches:
    skip
```

It is closer to:

```text
if rule matches file:
    skip it unless it is already tracked

if rule matches directory:
    skip it unless the index already contains tracked descendants under it
```

That is a very important safety idea.

Without it, a user could add an ignore rule and accidentally make the traversal stop seeing files that are still part of the staged/tracked world.

So the ignore layer is doing two jobs at once:

- honoring user intent
- preserving repository continuity for already tracked content

This is why `gitignore_should_skip()` consults the index instead of only reading `.mygitignore`.

## Filtering Out Unchanged Files

Once traversal has built a list of candidate files and hashes, `snapshot.c` shrinks it.

This is where the add command becomes efficient.

The code reads the current index hash for each candidate path:

```c
lookup_status = file_io_read_index_hash(index_path,
    files[read_index]->path, tracked_hash);
if (lookup_status == 1
        && strcmp(tracked_hash, files[read_index]->hash) == 0) {
    file_data_destroy(files[read_index]);
    continue;
}
files[write_index] = files[read_index];
write_index++;
```

The effect is:

- if index already has the same hash, drop the candidate
- otherwise keep it

This means `add` does not report success just because it saw files.
It only reports success for files whose staged content actually needs refreshing.

## Algorithm Spotlight: in-place compaction of changed files

This is a nice little algorithmic detail in `filter_unchanged_files()`.

The function does not build a second result array.
It compacts the existing array in place.

Conceptually:

```text
read_index = scans every discovered file
write_index = marks where the next kept file should go

for each discovered file:
    if unchanged:
        destroy it
    else:
        move it to files[write_index]
        advance write_index

truncate logical length to write_index
```

This is a standard two-pointer compaction pattern.

Why it is a good fit here:

- no second heap array is needed
- unchanged entries are destroyed immediately
- the kept entries stay packed at the front

So the result is a smaller logical list without doing a second round of allocation.

## Blob Creation Is Deduplicated

After changed files are identified, the next question is:

"Do we need to write a new blob object for this content?"

The answer is: only if the object does not already exist.

That logic lives in `create_blob()`:

```c
exists_status = blob_exists(file->hash, git_obj_path);
if (exists_status == 0) {
    return (0);
}
...
file_io_copy_file(file->path, blob_path);
```

This means object storage is content-addressed in a practical sense:

- if two files have the same content hash, they map to the same blob object name
- if a blob already exists, add reuses it instead of writing a duplicate copy

So add is not merely staging paths.
It is also maintaining the repository’s object store efficiently.

## Updating The Index

The index update code is more careful than just opening `.mygit/index` and overwriting one line in place.

Instead it:

1. opens the current index for reading
2. opens a temp file for writing
3. copies every index entry across
4. replaces the matching path when found
5. appends the path if it was not already present
6. renames the temp file over the real index

That strategy matters because it keeps index updates from being half-written if something fails partway through.

## Algorithm Spotlight: temp-file index rewrite

At an algorithm level, index updating works like this:

```text
open old index
open temp index
found = false

for each old entry:
    if entry path matches file path:
        write updated path/hash to temp
        found = true
    else:
        copy old entry to temp

if not found:
    append new path/hash to temp

rename temp to real index
```

This is a very good pattern for repository metadata.

Why it is better than editing in place:

- simpler logic
- no risk of partially overwriting a line and corrupting the index format
- failure leaves the old index intact until the final rename

So even though the project is educational, this part is already using a solid real-world technique.

## Why Add Feels Coherent

The add architecture works because each file owns one question.

### `add.c`

What are the command stages?

### `snapshot.c`

Which files really count as changed for staging?

### `traversal.c`

How do we safely and recursively discover candidate files?

### `blob_and_index.c`

How do we persist the staged result?

That separation keeps the codebase from collapsing into one huge `add()` function full of:

- CLI validation
- directory walking
- ignore parsing
- hashing
- deduplication
- object storage
- index rewriting

all mixed together.

Instead, the feature reads as a pipeline:

discover -> filter -> persist

That is why it is relatively easy to explain.

## What Add Does Not Do

It is also useful to understand what this feature does **not** currently try to solve.

### It does not do path-specific staging

Only `add .` is supported.

### It does not track deletions here

This add flow finds regular files that exist now.
It does not separately scan the index for tracked paths that were deleted from disk and remove them from staging.

So today the model is:

- stage new or changed files
- not full deletion reconciliation

### It does not create commits

Add only prepares the index and object store.
History comes later.

## A Worked Example

Imagine this working tree state:

- `README.md` unchanged from the index
- `src/main.c` changed
- `notes.txt` new

Then add behaves like this:

1. traversal sees all three files
2. each file is hashed
3. filter step removes `README.md` because its hash already matches the index
4. `src/main.c` and `notes.txt` remain
5. missing blobs for those hashes are created if needed
6. index is rewritten so those two paths now point at their latest hashes

The final message reports `2 file(s)` added, not `3`.

That is exactly the point of the unchanged-file filter.

## The Most Important Things To Remember While Reading The Code

If you keep only these ideas in your head, the add code becomes much easier to follow:

1. `add` updates the index, not history.
2. The comparison is against the current index, not against the last commit directly.
3. Traversal works in absolute paths, but the index stores repo-relative paths.
4. `.mygitignore` is constrained by already tracked content.
5. Unchanged candidates are removed by in-place compaction.
6. Blob storage is deduplicated by content hash.
7. The index rewrite uses a temp file for safety.

## Good Next Files To Read After This

If you want to follow the feature in code order, read:

1. `src/commands/add.c`
2. `src/add/snapshot.c`
3. `src/add/traversal.c`
4. `src/core/gitignore.c`
5. `src/add/blob_and_index.c`

That order matches the conceptual flow of the feature from command to persistence.
