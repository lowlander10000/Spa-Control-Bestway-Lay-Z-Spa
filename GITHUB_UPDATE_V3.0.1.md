# Spa Control v3.0.1 — GitHub update

This ZIP contains only the files changed for v3.0.1.

## 1. Upload the update

Open the repository and choose:

**Add file → Upload files**

Upload the contents of this folder while preserving the directory structure.

Replace the existing files when GitHub asks.

Commit message:

```text
Release Spa Control v3.0.1
```

After the commit, the **Build** workflow should start automatically.

## 2. Check the build

Open:

**Actions → Build**

Wait until the new run has a green check mark.

Do not create the release tag if the build is red.

## 3. Create the v3.0.1 release

When the build succeeds:

1. Open **Releases**.
2. Choose **Draft a new release**.
3. Create the tag `v3.0.1` from the `main` branch.
4. Release title: `Spa Control v3.0.1`
5. Use the contents of `RELEASE_NOTES_V3.0.1.md` as the description.
6. Publish the release.

Because the release workflow listens for tags beginning with `v`, pushing the
new `v3.0.1` tag should automatically build and attach:

- `Spa_Control_v3.0.1_firmware.bin`
- `Spa_Control_v3.0.1_littlefs.bin`
- `SHA256SUMS.txt`

## Files included

- `src/webApp.cpp`
- `src/wifiManager.cpp`
- `data/index.html`
- `data/js/app.js`
- `data/sw.js`
- `CHANGELOG.md`
- `RELEASE_NOTES_V3.0.1.md`
