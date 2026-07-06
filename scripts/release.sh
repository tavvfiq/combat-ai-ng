#!/usr/bin/env bash
#
# release.sh - bump the version in xmake.lua + src/main.cpp, commit, and tag.
#
# Usage:
#   scripts/release.sh patch          # 1.7.2 -> 1.7.3   (default)
#   scripts/release.sh minor          # 1.7.2 -> 1.8.0
#   scripts/release.sh major          # 1.7.2 -> 2.0.0
#   scripts/release.sh 1.9.0          # set an explicit version
#
# Requires a clean working tree. Does NOT push - it prints the push command.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

XMAKE="xmake.lua"
MAIN="src/main.cpp"

current="$(grep -oE 'set_version\("[0-9]+\.[0-9]+\.[0-9]+"\)' "$XMAKE" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)"
if [ -z "$current" ]; then
    echo "error: could not find set_version(\"X.Y.Z\") in $XMAKE" >&2
    exit 1
fi
IFS='.' read -r MA MI PA <<< "$current"

arg="${1:-patch}"
case "$arg" in
    major) new="$((MA + 1)).0.0" ;;
    minor) new="${MA}.$((MI + 1)).0" ;;
    patch) new="${MA}.${MI}.$((PA + 1))" ;;
    [0-9]*.[0-9]*.[0-9]*) new="$arg" ;;
    *)
        echo "usage: $0 [major|minor|patch|X.Y.Z]" >&2
        exit 1
        ;;
esac

tag="v$new"

if git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
    echo "error: tag $tag already exists" >&2
    exit 1
fi
if [ -n "$(git status --porcelain)" ]; then
    echo "error: working tree not clean - commit or stash changes first" >&2
    exit 1
fi

# Update the version in both source-of-truth locations.
sed -i -E "s/set_version\(\"[0-9]+\.[0-9]+\.[0-9]+\"\)/set_version(\"$new\")/" "$XMAKE"
sed -i -E "s/(loading\.\.\.\", \")[0-9]+\.[0-9]+\.[0-9]+(\")/\1$new\2/" "$MAIN"

# Sanity check the edits actually landed.
if ! grep -q "set_version(\"$new\")" "$XMAKE"; then
    echo "error: failed to update version in $XMAKE" >&2
    git checkout -- "$XMAKE" "$MAIN"
    exit 1
fi

git add "$XMAKE" "$MAIN"
git commit -m "chore: release $tag"
git tag -a "$tag" -m "Release $tag"

echo "Bumped $current -> $new, committed, and tagged $tag."
echo "Push with: git push && git push origin $tag"
