# GHOST — Release Process

Ghost is published on the VCV Library (slug `tnn1t1s-ghost`). This is how to cut
an update.

## Versioning

`plugin.json` `version` is `MAJOR.MINOR.REVISION`, no `v` prefix. The MAJOR
number tracks the Rack major version, so it stays `2.x.x` while targeting
Rack 2. Choose `MINOR.REVISION` per change: REVISION for fixes and small tweaks,
MINOR for new modules or notable features.

## Cutting a release

1. Land the changes on `main`.
2. Bump `version` in `plugin.json`.
3. Add a dated entry to `CHANGELOG.md`.
4. Commit, then tag and push:
   ```
   git tag vX.Y.Z
   git push origin main
   git push origin vX.Y.Z
   ```
5. Create the GitHub release: `gh release create vX.Y.Z`.

## Getting the update into the VCV Library

The Library build farm builds from a specific commit you name — not a tag, not a
branch. After pushing the release commit:

1. Get the commit hash: `git rev-parse HEAD`.
2. Comment on the plugin's Library thread — VCVRack/library#919 — with the new
   version and that exact commit hash.

A Library maintainer builds the commit per-OS and closes the thread when the
build is live. Users then pick up the update through Rack's plugin update flow
(Library → Update all, then restart). `configParam` default changes apply only
to newly added modules; saved patches keep their stored values.
