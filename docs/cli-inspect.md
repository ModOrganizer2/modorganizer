# Inspecting an instance from the command line

`inspect` reports the selected instance and profile using the same loaded models as
the user interface. It waits for the initial directory scan, so the plugin list
includes the enabled mods' files. It does not launch the game, change mod or plugin
activation, or sort the load order.

```powershell
# Human-readable summary
ModOrganizer.exe -i "My instance" -p "Default" inspect

# UTF-8 JSON on stdout; suitable for a pipe or subprocess capture
ModOrganizer.exe -i "My instance" -p "Default" inspect --json

# Write JSON atomically to a file, including paths containing non-ASCII characters
ModOrganizer.exe -i "My instance" -p "Default" inspect --json --output "state.json"
```

Close any running MO process first. `inspect` is not forwarded to an existing
process because the command forwarding protocol does not return output to its
caller. Do not use `--multiple` to work around this restriction.

The normal instance initialization still takes place: game support plugins must
be installed, the game directory must be valid, and MO may update its normal
configuration files and caches. This command is not a standalone parser or a
guarantee of completely unattended startup for an unconfigured installation.

`--output` is optional and accepts a path relative to the caller's current working
directory. The parent directory must exist. The destination is replaced only after
the complete report has been written. Exit status is `0` on success and `1` if the
report cannot be generated or written.

When using `--json`, `--logs` requires `--output`, so console logs cannot contaminate
the JSON on stdout. Reports include local paths and configured executable arguments;
review them before sharing.

## JSON format

The root object contains:

| Field | Contents |
| --- | --- |
| `schemaVersion` | Integer format version, currently `1` |
| `mo2Version` | MO version string |
| `instance` | `name`, configuration `path`, `baseDirectory`, and `portable` |
| `game` | `name` and installation `path` |
| `profile` | `name` and `path` |
| `mods` | Entries ordered by increasing profile priority |
| `plugins` | Entries ordered by increasing plugin priority |
| `executables` | Configured executable entries in MO's order |

Each mod has `name`, `path`, `priority`, `enabled`, `unmanaged`, `separator`, and
`overwrite`. These entries include the special entries known to the profile;
their count is not necessarily the number of installed mod directories.

Each plugin has `name`, `enabled`, `priority`, `loadOrder`, and `origin` (the source
mod name). `priority` is the position in the full plugin list, including disabled
plugins. `loadOrder` is the value reported by MO's plugin model and may be `-1`
for a plugin without a load index. It is not an ESL/ESP hexadecimal form ID prefix.

Each executable has `title`, `binary`, `arguments`, `workingDirectory`,
`forcedLibrariesEnabled`, and `forcedLibraries`. Each library has `process`,
`library`, and its individual `enabled` flag. A library is requested only when both
the executable's flag and the library's flag are enabled. These are configuration
values, not confirmation that injection has succeeded in a running process.

All strings use JSON escaping and UTF-8. Consumers should ignore unknown fields.

## Integration checks

After building MO and installing it alongside its dependencies and game support
plugins, run the integration checks against an existing, disposable test instance:

```powershell
python tests/test_cli_inspect.py --mo2 "C:\MO2-test\ModOrganizer.exe" `
    --instance "Test instance" --profile "Default" `
    --expected-plugin "Oblivion.esm"
```

The test instance must already be configured and must not require first-run or
missing-game dialogs. Using a profile with non-ASCII mod names exercises Unicode
output. The checks cover complete JSON on stdout, repeatable state, atomic file
output, unwritable output paths, and rejection of mixed logs and JSON on stdout.
