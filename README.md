# MyGit

> A small Git-like version control project written in C, built to make the core ideas feel visible instead of magical.

![C](https://img.shields.io/badge/language-C-blue)
![Status](https://img.shields.io/badge/status-educational%20but%20usable-success)
![Scope](https://img.shields.io/badge/scope-Git--like%2C%20not%20Git-orange)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey)

> **MyGit is not Git.**  
> It is a smaller system built to make Git-like internals easier to understand.  
> **The box is open.**

---

## 🔍 Overview

MyGit is a **learning-oriented local VCS**.

It is not trying to compete with Git.  
It is trying to make the moving parts easier to see:

- staging through an index
- blob storage
- tree construction
- commit objects
- branch refs
- checkout-style state transitions
- merge as snapshot logic

> If Git sometimes feels like a black box, this project is the opposite spirit.  
> **The box is open.**

---

## 🧠 Why This Exists

Modern version control tools are powerful, but that power can hide the underlying model.

This project exists to answer questions like:

- What does a branch really need to be?
- What does the index actually do?
- How does a commit become a tree plus metadata?
- What has to happen before checkout or reset is safe?
- How can merge work without starting from line-by-line diff magic?

The idea is simple: build a smaller system, keep the representation readable, and learn by tracing the real code.

---

## 👤 Who This Is For

### ✅ A good fit if you are:

- learning C and want a project with real structure
- learning version control internals
- curious about how Git-like tools are modeled under the hood
- building your own toy VCS and want reference ideas
- trying to connect high-level VCS concepts to concrete code

### ❌ Probably not for you if you want:

- full Git compatibility
- production-grade performance
- a polished end-user replacement for Git

---

## ⚙️ Project Spirit

The project tries to stay light:

- small enough to hold in your head
- serious enough to teach real architectural ideas
- simple enough that the storage model is inspectable by hand

That means the code usually prefers:

- clarity over feature count
- readable object formats over compact ones
- explicit state transitions over hidden abstractions

---

## 📌 Project Status

MyGit is currently in an **educational but usable** state.

The core repository model is implemented and the project already supports a meaningful local workflow for studying how a Git-like tool behaves.

### ✅ Implemented

- repository initialization  
- staging through an index  
- blob creation and object storage  
- tree construction  
- commit creation  
- commit history viewing  
- branch creation and branch refs  
- checkout-style branch switching  
- reset behavior  
- simplified merge flow  

### 🎯 Current Focus

This project is being built to make the architecture understandable first, then usable, rather than chasing Git feature parity.

### ⚠️ Known Limitations

MyGit is intentionally narrower than Git. Some commands are simplified and may not support all edge cases or safety guarantees that real Git provides.

Examples of limitations include:

- no full Git compatibility  
- no advanced conflict resolution  
- merge is simplified and snapshot-oriented  
- behavior may differ from Git in edge cases  
- internal formats are designed for readability, not performance  
- safety checks are present where needed, but the tool is still educational in scope  

### 🚫 Not Planned

At least for this version, the goal is **not** to turn MyGit into a full Git clone.  
The goal is to build something small, readable, and honest enough to teach the real ideas well.

---

## 📚 Documentation Style

This repository includes two kinds of documentation support.

### 🧩 Header-Level API Documentation

The header files act as the project's API documentation.

They are the fastest way to understand:

- exposed functions  
- data structures  
- expected inputs and outputs  
- module boundaries  

### 📖 Detailed Guided Explanations

The `docs/` section contains long-form explanations written **with AI assistance** to reduce ambiguity and make the code easier to understand, especially for readers still growing in C.

These guides aim to:

- explain intent behind the code  
- clarify implementation flow  
- reduce confusion from low-level C details  
- connect architecture to actual behavior  

They are a learning aid, not a replacement for reading the code.

### 🧷 Source of Truth

The guides help understanding, but:

> **The ultimate source of truth is always the code itself.**

If anything diverges, trust the implementation.

---

## 🛠 Current Commands

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

---

## 🚀 Install / Build

There is no installer yet. You build it from source.

### ⚠️ Platform Note

> This project is **Linux-only**.  
> It was developed and tested on Linux, and **has not been tested on Windows**.

### Requirements

- `gcc`  
- `make`  
- OpenSSL `libcrypto`  

### Build

```bash
make