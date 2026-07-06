<#
.SYNOPSIS
    Bump the version in xmake.lua + src/main.cpp, commit, and tag.

.DESCRIPTION
    Requires a clean working tree. Does NOT push - prints the push command.

.EXAMPLE
    .\scripts\release.ps1              # 1.7.2 -> 1.7.3 (default: patch)
    .\scripts\release.ps1 minor        # 1.7.2 -> 1.8.0
    .\scripts\release.ps1 major        # 1.7.2 -> 2.0.0
    .\scripts\release.ps1 1.9.0        # explicit version
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Bump = 'patch'
)

$ErrorActionPreference = 'Stop'

# Work from the repo root.
$root = (git rev-parse --show-toplevel)
if ($LASTEXITCODE -ne 0) { throw 'not inside a git repository' }
Set-Location $root

$xmake = 'xmake.lua'
$main  = 'src/main.cpp'

# Read current version from set_version("X.Y.Z").
$xmakeText = Get-Content $xmake -Raw
$m = [regex]::Match($xmakeText, 'set_version\("(\d+)\.(\d+)\.(\d+)"\)')
if (-not $m.Success) { throw "could not find set_version(""X.Y.Z"") in $xmake" }
$current = "$($m.Groups[1].Value).$($m.Groups[2].Value).$($m.Groups[3].Value)"
$ma = [int]$m.Groups[1].Value
$mi = [int]$m.Groups[2].Value
$pa = [int]$m.Groups[3].Value

# Compute the new version.
switch -Regex ($Bump) {
    '^major$'            { $new = "$($ma + 1).0.0"; break }
    '^minor$'            { $new = "$ma.$($mi + 1).0"; break }
    '^patch$'            { $new = "$ma.$mi.$($pa + 1)"; break }
    '^\d+\.\d+\.\d+$'    { $new = $Bump; break }
    default              { throw "usage: release.ps1 [major|minor|patch|X.Y.Z]" }
}

$tag = "v$new"

# Guard: tag must not exist, working tree must be clean.
git rev-parse -q --verify "refs/tags/$tag" *> $null
if ($LASTEXITCODE -eq 0) { throw "tag $tag already exists" }
if (git status --porcelain) { throw 'working tree not clean - commit or stash changes first' }

# Update the version in both source-of-truth locations.
$xmakeNew = [regex]::Replace($xmakeText, 'set_version\("\d+\.\d+\.\d+"\)', "set_version(""$new"")")
Set-Content -Path $xmake -Value $xmakeNew -NoNewline

$mainText = Get-Content $main -Raw
$mainNew = [regex]::Replace($mainText, '(loading\.\.\.", ")\d+\.\d+\.\d+(")', "`${1}$new`${2}")
Set-Content -Path $main -Value $mainNew -NoNewline

# Sanity check the edits landed.
if ((Get-Content $xmake -Raw) -notmatch [regex]::Escape("set_version(""$new"")")) {
    git checkout -- $xmake $main
    throw "failed to update version in $xmake"
}

git add $xmake $main
git commit -m "chore: release $tag"
git tag -a $tag -m "Release $tag"

Write-Host "Bumped $current -> $new, committed, and tagged $tag."
Write-Host "Push with: git push && git push origin $tag"
