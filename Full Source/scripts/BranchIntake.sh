#!/usr/bin/env bash
set -euo pipefail

REMOTE="${REMOTE:-origin}"
MAIN_BRANCH="${MAIN_BRANCH:-main}"
PREFIX="${PREFIX:-cursor/}"
PUSH="${PUSH:-0}"

git status --porcelain | grep -q . && { echo "Working tree not clean"; exit 1; } || true

git fetch "$REMOTE" --prune
git checkout "$MAIN_BRANCH"
git pull --no-rebase "$REMOTE" "$MAIN_BRANCH"

mapfile -t refs < <(git for-each-ref "refs/remotes/$REMOTE/$PREFIX*" --format="%(refname:short)" | sed '/\/HEAD$/d' | sed '/^$/d')

for ref in "${refs[@]}"; do
  if git merge-base --is-ancestor "$ref" "$MAIN_BRANCH"; then
    continue
  fi

  before="$(git rev-parse HEAD)"
  git merge --no-ff "$ref" -m "merge: intake $ref into $MAIN_BRANCH"
  after="$(git rev-parse HEAD)"

  deleted="$(git diff --name-status "$before..$after" | awk '$1=="D"{print $2}' || true)"
  if [[ -n "${deleted}" ]]; then
    # Restore deleted files from pre-merge commit.
    while IFS= read -r f; do
      [[ -z "$f" ]] && continue
      git checkout "$before" -- "$f"
    done <<< "${deleted}"
    git add ${deleted}
    git commit -m "restore: keep files (no deletions policy)"
  fi
done

if [[ "$PUSH" == "1" ]]; then
  git push -u "$REMOTE" "$MAIN_BRANCH"
fi

