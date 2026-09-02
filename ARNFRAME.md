# ArnFrame by Studio Arn

ArnFrame is a macOS Intel adaptation of the open-source Drift video editor.

## Credits

- Intel adaptation, visual identity, and project direction: **Alessandro Henriques Teixeira — Studio Arn**
- Original project: **Drift**, developed by **CutWire Studios**
- License: **GNU General Public License v3.0 or later**

ArnFrame is an independent derivative and is not an official CutWire Studios release. The
original copyright notices and GPLv3 license remain in place. Source code for this derivative is
distributed with the same license.

## Intel build

This branch targets `x86_64` macOS. ONNX Runtime headers are supplied separately during the build;
the Linux header archive is used only because the C and C++ API headers are platform-independent.
No Linux runtime library is bundled in the macOS application.

The packaging script creates `ArnFrame-0.5.1-Intel-x86_64.dmg` and bundles the Qt and multimedia
dependencies needed to run without Homebrew on the destination Mac.
