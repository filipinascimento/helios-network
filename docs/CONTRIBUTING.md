# Contributing & Release Guide

Welcome! This document summarises the day-to-day development workflow, testing requirements, and the release process across npm, vcpkg, and Conan. It also sketches how you could automate the steps with GitHub Actions.

---

## Local Development

1. **Clone & install**
   ```bash
   git clone https://github.com/helios-graphs/helios-network.git
   cd helios-network
   npm install
   ```
2. **Generate WASM artefacts** (needed for linting/IDE features):
   ```bash
   npm run build:wasm
   ```
3. **Build bundles**
   ```bash
   npm run build
   ```
4. **Run tests**
   - Node tests: `npm test`
   - Browser tests (optional): `npm run test:browser`
5. **Native builds**
   ```bash
   # Build both shared and static libraries via CMake
   cmake -S . -B build/native-cmake -DCMAKE_BUILD_TYPE=Release \
     -DHELIOS_BUILD_SHARED=ON -DHELIOS_BUILD_STATIC=ON
   cmake --build build/native-cmake
   ```
   Toggle `HELIOS_BUILD_SHARED` or `HELIOS_BUILD_STATIC` to restrict the output, and run `cmake --install build/native-cmake --prefix <dest>` when you need headers + libs staged. If you prefer the shorthand targets, `make native`, `make native-static`, or `make native-shared` wrap the same flow. (The repository’s Meson project is wired for the Emscripten build that `npm run build:wasm` uses; adapting it for a host compiler requires enabling platform extensions such as `_DARWIN_C_SOURCE`.)

Follow the existing coding style (C17 for native code, modern ES modules for JS). Keep functions documented via Doxygen/JSDoc where they are part of the public API.

---

## Versioning & Release Workflow

All version metadata is synchronised by `sync-version.cjs`. The `Makefile` offers a turnkey release flow.

1. **Prepare a clean tree**
   ```bash
   make check-clean
   ```
2. **Run tests/builds** (ensure `npm test`, `npm run build`, and optional native builds succeed).
3. **Bump version & create tag**
   ```bash
   make release VERSION=0.3.0
   ```
   This will:
   - Update `package.json`, `package-lock.json`, `meson.build`, `CMakeLists.txt`, `src/native/include/helios/CXNetwork.h`, `packaging/vcpkg/.../vcpkg.json`, and `packaging/conan/conanfile.py`.
   - Commit with message `Release v0.3.0`.
   - Create an annotated tag `v0.3.0`.
4. **Push**
   ```bash
   make push-release VERSION=0.3.0
   ```
5. **Create a GitHub release** (optional but recommended) through the UI or API to attach release notes and build artefacts.

### Publishing to npm

1. Ensure you are logged in: `npm login`.
2. Publish from a clean build:
   ```bash
   npm run build
   npm publish
   ```
   The published package includes `dist/` only (see `package.json` `files` entry).

### Updating vcpkg

1. Use the overlay port for testing:
   ```bash
   ./vcpkg/vcpkg install helios-network --overlay-ports=packaging/vcpkg
   ```
2. For upstream submission, follow `docs/packaging.md` (copy the port to `ports/helios-network` in your vcpkg fork, swap in `vcpkg_from_github`, run `x-add-version`, and open a PR).

### Updating Conan

1. Test locally:
   ```bash
   conan create ./packaging/conan --build=missing
   ```
2. For ConanCenter, adapt the recipe to fetch the release tarball, add a `test_package`, and follow the contribution steps described in `docs/packaging.md`.

---

## Automation with GitHub Actions

The repo does not yet include workflows, but the release steps above can be automated:

- **CI checks**: run `npm install`, `npm test`, `npm run build`, `cmake --preset` builds on pull requests.
- **Release workflow**: on manual dispatch or GitHub release:
  1. Check out the repo and set up Node/CMake.
  2. Run `npm run sync-version -- --version ${{ github.event.release.tag_name }}` to align metadata.
  3. Use `npm publish` with an `NODE_AUTH_TOKEN` secret.
  4. Optionally invoke vcpkg/Conan scripts to update overlays or produce artefacts.
- **Version bump**: a workflow could expose an `workflow_dispatch` input `version`. The job would run `make release VERSION=...`, push the commit + tag using a PAT (personal access token), and create the GitHub release automatically.

> **Secrets needed**: `NPM_TOKEN`, optionally `GITHUB_TOKEN` or PAT for pushing tags, and credentials for any mirrored registries. For Conan/vcpkg submissions you still need human review on their upstream repositories, so automation should focus on building artefacts and opening PRs.

The repository includes `Build and Deploy Docs` (`.github/workflows/docs.yml`), which rebuilds the JSDoc and Doxygen output on every push to `main` and publishes the combined site to GitHub Pages automatically.

---

## Code Style & Pull Requests

- Use descriptive branch names (`feature/…`, `fix/…`).
- Keep commits scoped; rebase onto the latest `main` before opening a PR.
- Include tests where feasible (add JS unit tests or native coverage). Highlight any manual testing in the PR description.
- Document new APIs in JSDoc/Doxygen and update `README.md` if behaviour changes.

Thank you for contributing to Helios Network! For questions or proposal drafts, open a GitHub Discussion or issue. We appreciate bug reports, docs improvements, performance analysis, and new feature ideas alike.
