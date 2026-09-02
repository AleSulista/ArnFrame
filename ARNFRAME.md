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

This branch targets Intel `x86_64` Macs. ArnFrame includes a native macOS Intel build of ONNX
Runtime 1.27.0 and its required Protobuf Lite library.

The bundled runtime enables local AI features such as face detection, object segmentation,
automatic subtitles and noise removal when the corresponding AI models are installed.

The packaging script creates `ArnFrame-0.5.1-Intel-x86_64.dmg` and bundles ONNX Runtime, Protobuf
Lite, Qt and the multimedia dependencies required to run the application without Homebrew on the
destination Mac.

AI processing is performed locally. Performance depends on the processor and can be slower on
older Intel Macs, especially when analysing long or high-resolution videos.

## Versão para macOS Intel

O ArnFrame inclui o ONNX Runtime 1.27.0 compilado nativamente para processadores Intel `x86_64`,
permitindo utilizar os recursos locais de inteligência artificial quando os respectivos modelos
estiverem instalados.

O DMG inclui as bibliotecas necessárias para execução, não exigindo que o usuário instale o
Homebrew. Em computadores Intel mais antigos, análises de IA em vídeos longos ou de alta resolução
podem levar mais tempo.

## Compatibilidade

- Processador: Mac Intel `x86_64`
- Sistema mínimo: macOS 12 Monterey
- Compatível em princípio com Monterey, Ventura, Sonoma e Sequoia
- Testado no macOS Sequoia 15.7.3
- Memória recomendada: 8 GB ou mais
- Os recursos de IA exigem os respectivos modelos instalados
- A IA pode processar lentamente em Macs Intel antigos

Esta compilação não é nativa para Macs Apple Silicon. Nesses computadores, utilize uma versão
ARM64 apropriada ou execute por meio do Rosetta 2, sem garantia de funcionamento.
