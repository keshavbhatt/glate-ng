# Licensing

Glate is open source. Two licences apply, by path.

## The program: GPL-3.0-or-later

Everything in this repository except the licensing module (next section) and the
third-party files listed further down is licensed under the GNU General Public License,
version 3 or, at your option, any later version. The text is in `LICENSE` and
`LICENSES/GPL-3.0-or-later.txt`. You may use, study, change and share it under the GPL's
terms: derived works stay under the GPL and come with their source.

### Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by combining it with the Ktechpit
Licensing Module (the files listed below), the licensors of this program grant you
additional permission to convey the resulting work, provided the Module is unmodified and
the resulting work is an official build published by Ktechpit. A work under the GPL that
is built or conveyed by anyone else must not contain the Module.

## The licensing module: Ktechpit Licensing Module License

The account, licence, evaluation-period, Pro-feature and usage-allowance code is not part
of the GPL-licensed work. It is source-available under the Ktechpit Licensing Module
License (`LICENSES/LicenseRef-Ktechpit-Licensing-Module.txt`): you may read it and build it
unmodified for your own use, and you may not modify it, bypass it, redistribute it or use
its endpoints in other software. These paths:

- `modules/AccountAndLicense/`
- `src/GlateLicenseConfig.h`
- `src/licenseservice.cpp`, `src/licenseservice.h`
- `src/accountdialog.cpp`, `src/accountdialog.h`, `src/accountdialog.ui`
- `src/planscomparisiondialog.cpp`, `src/planscomparisiondialog.h`, `src/planscomparisiondialog.ui`

`REUSE.toml` records the same mapping in machine-readable form; the module's own source
files carry an SPDX header.

## Third-party files in this repository

| Path | Licence | Origin |
|---|---|---|
| `src/QHotkey/` | BSD-3-Clause | QHotkey by Felix Barz |
| `src/waitingspinnerwidget.*` | MIT | QtWaitingSpinner by Alexander Turkin, William Hallatt, Jacob Dawid |
| `src/icons/*-line.png`, `src/icons/*-fill.png` | Apache-2.0 | Remix Icon |

Components fetched or downloaded rather than stored here (Qt, Mozilla's translation engine,
the Firefox translation models) are listed in `THIRD_PARTY.md`.

## What this means for forks and contributors

- Contributions to the GPL part are welcome under the GPL.
- A fork that ships its own builds must leave the module out and replace what it
  provides, and must not talk to Ktechpit's licence servers.
- Official builds (the snap, Flatpak, Windows and other packages published by Ktechpit)
  combine both parts under the additional permission above.

## Earlier versions

Glate 4.3.0 and earlier were released under the MIT licence. Those releases keep it; this
licensing applies from the first release after 4.3.0.
