# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| latest  | Yes       |

## Reporting a Vulnerability

This plugin is native code loaded into the OBS process, so any bug in it runs with the full privileges of OBS.

For non-sensitive issues, open a regular GitHub issue. If you believe you have found a security vulnerability that should be disclosed privately, please [contact](https://www.nathanv.com/contact) the maintainer directly.

Please include:

- A description of the issue
- Steps to reproduce
- Potential impact
- A suggested fix (if you have one)

You should receive a response within 72 hours.

## Known Limitations

- **Runs in-process with OBS.** There is no sandbox between a plugin and OBS; the plugin can do anything OBS can. Install only from the GitHub releases page (each release lists SHA-256 checksums) or build from source.
- **macOS builds are ad-hoc signed and not notarized.** Release builds come from GitHub Actions without a Developer ID, so Gatekeeper warns on the `.pkg` and you have to open it deliberately. The signature says nothing about who built it; the SHA-256 checksums on the release do. Windows and Linux builds carry no signature at all.
- **Drives OBS's own UI for two actions.** The virtual camera settings dialog and the YouTube broadcast flow have no plugin API, so the plugin finds the built-in Controls dock's buttons by object name and clicks them. This does nothing a user could not do by clicking the built-in dock.
- **Reads the profile config.** The plugin reads the output mode, replay buffer, and stream delay settings from the active profile to decide which buttons to show. It never writes to the config.
- **No network access.** The plugin opens no sockets and makes no requests. Streaming itself goes through OBS's own outputs.
- **No phoning home.** This project does not collect analytics or metrics and does not call home in any way.
