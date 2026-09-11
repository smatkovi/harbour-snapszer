# Snapszer for Sailfish OS

`harbour-snapszer` is a native Sailfish OS implementation of the classic
Hungarian two-player **Snapszer / Snapszli / 66** card game.

## Current gameplay

- 20-card Hungarian deck: Ace, Ten, King, Upper and Lower in four suits.
- Card values: Ace 11, Ten 10, King 4, Upper 3, Lower 2.
- Five cards each; one visible trump card plus a nine-card face-down talon.
- Open-talon phase with free response and trick-and-draw play.
- Strict follow / overtake / trump obligations after the talon is exhausted or closed.
- 20 and 40 declarations (King + Upper of one suit).
- Exchange of the trump Lower while at least three face-down talon cards remain.
- Talon closing, including the closer's failure penalty.
- Calling 66 and 1/2/3 game-point round scoring.
- Match play to 7 game points with alternating dealer.
- Four AI difficulty levels.
- English and Hungarian UI.
- Interrupted-match autosave and restore.

The current release deliberately focuses on the classic two-player game. The separate opening
"Snapszer" contract, kontra/rekontra, and three-/four-player bidding variants are
reserved for a later release.

## Visual assets

The Piatnik and Betyár card sets and `Card.qml` are copied directly from the
`harbour-zsirozas` RC7 source so the two games use the same card artwork and
rendering behaviour. Snapszer uses only the existing rank identifiers 10, 11 (Lower), 12 (Upper),
13 (King) and 14 (Ace).

## Sandboxing and persistence

The application is Sailjail-sandboxed. Preferences and autosave state are stored
with an explicit `QStandardPaths::AppConfigLocation` path, under the app-specific
`org.edp17/harbour-snapszer` sandbox area.

## RC4 device-feedback fixes

- Player and opponent names can now be cleared completely while editing, so replacing `Player` with `Nick` no longer requires keeping the first letter temporarily.
- Settings continues to use the single C++ engine instance exposed through the QML root context.
- The player captured-card pile remains beside the lower hand.
- About keeps the richer Sailfish card-game layout used by the related projects.
- The Snapszer icon keeps the same 66/Piatnik concept but makes the card fan larger and centered, with a larger 66 medallion at the bottom center.

## Building

The app targets Sailfish OS 5 and uses CMake, C++17, Qt 5, Sailfish Silica and
`libsailfishapp`.

Build in the Sailfish SDK in the usual way, or package with
`harbour-snapszer.spec`.

## License

MIT. See `LICENSE`.

## LAN multiplayer

Pull down → **Play over LAN** to play against a second phone running Snapszer
in the same Wi-Fi network (a hotspot opened by one of the phones works too).

- One phone taps **Host**, the other picks it from the list of found games or
  enters the host's address shown on its screen.
- The host's phone runs the authoritative game; the guest mirrors it, so each
  player sees their own hand at the bottom.
- The match against the AI is kept and resumes when the LAN game ends.
- Ports: TCP 45465 (game), UDP 45466 (discovery). Sailfish OS allows both
  through its firewall by default. The app needs the Sailjail `Internet`
  permission for this.
- Both phones receive the full game state, so the mode is meant for friendly
  games, not for playing against strangers.
