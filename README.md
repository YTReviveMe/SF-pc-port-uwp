# Syphon Filter UWP

Xbox Dev Mode UWP port of *Syphon Filter*. This repository contains source
code only; it does not include game data.

## Xbox setup

1. Deploy a signed `.appx` package through Xbox Device Portal.
2. Launch the game. On first launch, select the folder containing a legally
   dumped Syphon Filter USA v1.1 (`SCUS-94240`) CUE and BIN image.
3. The game imports the image into its `LocalState\GAME` folder before starting.

For an internal transfer, launch the app once, then use Xbox Device Portal's
**File Explorer**. Open **LocalAppData**, choose the `SyphonFilterUWP`
package, and browse to `LocalState/Game`. Upload your BIN and CUE game files
into `LocalState/Game`, and the game will automatically prepare them for you.

## Xbox controller

| Control | Action |
| --- | --- |
| Left Stick / D-pad | Move, turn, and navigate menus |
| Right Stick | Look while aiming |
| A | Kneel / confirm |
| B | Roll, zoom out, or cancel |
| X | Fire |
| Y | Interact or zoom in |
| LB | Aim |
| RB | Target lock |
| LT / RT | Step left / right |
| View | Change weapon; hold with LT / RT to cycle weapons |
| Menu | Pause |
| L3 + R3 | Open Display & Performance |

Controller presets and custom bindings are available from **Pause > Options >
Controller**. Display settings are available from **Pause > Options > Display
& Performance** and are saved for future launches.

## Build from source

The UWP build requirements, configuration command, package signing, and game
media setup are documented in [Xbox Dev Mode UWP](docs/XBOX_DEVMODE.md).

## License

Project-owned source code is available under the [MIT License](LICENSE).
Third-party components retain their respective licenses. No rights to *Syphon
Filter*, its game data, characters, artwork, or trademarks are granted by this
repository.
