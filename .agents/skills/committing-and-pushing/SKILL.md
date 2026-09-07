---
name: committing-and-pushing
description: Use for Git commits, commit messages, branch synchronization, pushes, and PR preparation in this STM32F4 smartwatch repository. Applies to requests such as 提交代码, 提交并同步, and 推送分支.
---

# Committing and Pushing

## Overview

This repository contains STM32F4 smartwatch firmware. Use Conventional Commits and the simplified Git Flow below. Apply these rules to this repository only.

## Request Scope

- "提交代码" / "commit": review and validate the requested changes, then create local commits. Do not infer a push request.
- "提交并同步" / "提交并推送": review, validate, commit, fetch and integrate relevant remote changes into the working branch, then push that branch.
- "同步分支": inspect divergence and integrate remote changes as requested; do not automatically commit unrelated uncommitted edits.
- "创建 PR": prepare or create a PR against the appropriate target using available host tooling. A push or PR-creation request does not authorize merging the PR.
- Skill invocation alone is not authorization to commit or publish. Once the user requests the action, complete it without asking for redundant confirmation; still respect tool-enforced permissions.

## Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

- **Header** (required, one line, ≤ 70 chars):
  - `type` — required, one of the types below.
  - `scope` — optional; use `f4` for firmware changes in this repo. Use an appropriate tooling scope or omit it for non-firmware changes.
  - `subject` — required, imperative present tense ("fix", not "fixed"), no trailing period.
- **Body** (optional): explain *why* the change is needed and the logic behind it — not just *what* changed.
- **Footer** (optional): `BREAKING CHANGE:` notes, or `Closes #123` / `Fixes PROJ-456`.

Example:
```
fix(f4): preserve calibration data during configuration updates

Keep existing calibration values when updating unrelated settings so that
configuration changes do not invalidate factory calibration.
```

## Commit Types

| type | use when |
|------|----------|
| `feat` | new feature |
| `fix` | bug fix |
| `docs` | documentation, README, comments only |
| `style` | formatting only, no logic change (NOT CSS) |
| `refactor` | code restructure, no feature/bugfix |
| `perf` | performance or UX improvement |
| `test` | add/update unit or integration tests |
| `chore` | build, deps, tooling (e.g. `.gitignore`, config) |
| `ci` | CI/CD config or scripts |
| `build` | build system or external dependencies |
| `revert` | revert a previous commit |

## Branch Naming

| branch | purpose | rules |
|--------|---------|-------|
| `main` | production, always buildable | PR-merge only, ≥1 reviewer |
| `dev` | development main line | PR-merge only; never merge `dev` → `main` directly |
| `feature/xxx` | feature work | branch off `dev`, free commit, merge back to `dev` via PR only |
| `release` | pre-release, version freeze | merge to `dev` and `main` when stable |
| `fix` | bug fix | branch off `dev`, merge back to `dev` via PR |
| `hotfix` | urgent fix | branch off `main`/`dev`, merge back to the required branches via PR |

**The integration branch is `dev`, NOT `develop`.**

## Merge Strategy

- `feature/xxx` → `dev`: **Squash and merge** (single commit, clean `dev` history).
- `release` → `dev`: **merge commit** (keep full history).
- `release` → `main`: **merge commit**.
- Never merge `dev` → `main` directly.

**Rule: a `feature/xxx` branch reaches `dev` only through a PR. Never push or merge a feature branch directly to `dev`.**

## Push Workflow

1. Inspect `git status`, the current branch/upstream, remote configuration, staged and unstaged diffs, and untracked files. Read the actual changes before drafting a message. Preserve unrelated edits and existing staged work; stage explicit paths or hunks rather than blindly using `git add .`.
2. Work on an appropriate feature/fix/hotfix branch. Do not create ordinary work commits directly on `main` or `dev`. For new work, branch from the appropriate integration base. If changes already exist on a protected branch, create a working branch at the current HEAD to preserve them, then reconcile its base with the intended integration branch. Do not discard edits or transplant existing commits blindly. Inspect available branches; if `dev` is absent, report that mismatch and obtain the intended integration target before publishing an incompatible workflow.
3. Review the selected diff for unintended generated artifacts and credentials. Do not include private signing keys, build outputs, unrelated IDE changes, or unrelated edits merely because they are present. Honor an explicit user request to include particular files.
4. Validate according to the change. For sensor, display, touch, or driver changes, also run Tests/run_all_host_tests.ps1. For firmware changes, use the existing CMake Debug build (`cmake --build --preset Debug`; configure with `cmake --preset Debug` if needed) and relevant existing tests. Documentation-only or skill-only edits do not need a firmware build. Report failed or unavailable checks accurately and address relevant failures before claiming the change is ready.
5. Review `git diff --cached` and `git diff --cached --check`. Create focused commits with the format above; use a message file for multiline bodies. Do not amend existing commits unless requested. A local-only commit request ends after reporting the commit and remaining worktree changes.
6. For a commit-and-sync request, fetch the configured remote (normally `origin`). Inspect the working branch's upstream and integration branch. Merge relevant remote changes into the working branch; fast-forward where possible and avoid rewriting published history. Resolve routine conflicts while preserving both sides' intent. If resolution requires an unknown product decision, ask a focused question. Re-run affected checks after integration changes.
7. Push only the working branch. Set its upstream on the first push. If the push is rejected because the remote advanced, fetch, inspect and integrate again; never force-push to bypass the rejection. Stop and report unresolved authentication, permission, or branch-policy errors instead of retrying unchanged commands.
8. Report the branch, commit hash and subject, validation, push result, PR target/link if applicable, and any remaining uncommitted changes. For local-only commits, state that no push occurred.

Example for a feature branch, after review and local commits:

```bash
git fetch origin
git merge origin/dev
git push --set-upstream origin feature/sensor
```

The merge runs while on the feature branch. Inspect and integrate an existing remote feature branch too, when it has independent changes. A push uploads the working branch; it does not merge it into `dev`.

## Pull Requests

- Feature PRs target `dev` and use squash merge. Release PRs target `dev` and `main` using merge commits.
- Follow the configured remote host; do not assume GitHub or use `gh` against an unsupported host.
- If PR creation is requested but no authenticated host integration is available, push the authorized branch and provide a ready-to-use title, description, and the verified repository link. Do not claim a PR was created or invent its URL.
- PR descriptions explain the problem, resulting behavior, and relevant validation. Do not auto-merge as part of routine synchronization.

## Common Mistakes

- Typing `develop` instead of `dev` in commands — the branch is `dev`.
- Subject ending with a period — subjects must not end with `.`.
- Subject in past tense ("fixed", "added") — use imperative ("fix", "add").
- Subject repeating the type verb ("fix(f4): fix ...") — pick a different verb ("fix(f4): resolve ...").
- "Merge feature to dev" but running `git merge dev` — that command merges `dev` INTO the current branch, not the other way around.
- Merging `dev` → `main` directly — must go through a `release` PR.
- Pushing `feature/xxx` straight to `dev` (`git push origin dev`, or `git merge feature/xxx` while on `dev`) — feature branches reach `dev` via PR only.
