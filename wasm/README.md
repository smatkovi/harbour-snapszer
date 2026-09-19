# Snapszer in the browser

A Qt for WebAssembly build of the Android edition, so the game can be played on
devices that have no Snapszer package. It was made for the iPhone, which cannot
be reached any other way without a Mac and an Apple account, but it runs in any
current browser.

Published at <https://smatkovi.github.io/harbour-snapszer/>. On a phone, open
that address and use "Add to Home Screen"; the game then starts full screen with
its own icon, and the saved match survives, which it does not reliably do in a
plain browser tab.

Add it to the home screen **before** playing. iOS gives a home-screen web app
its own storage, separate from Safari's, so a match started in the tab stays in
the tab. The loading screen says so too.

A service worker keeps the game on the device after the first visit, so later
starts are immediate and work with no network. Its cache name carries a stamp of
the built binary, so a new deploy replaces the old cache rather than being
shadowed by it.

## What it can and cannot do

Everything the Android edition does except network play: the two-player game
against the computer, the three- and four-player tables with all seats on one
device, English and Hungarian, the interrupted-match autosave.

There is no LAN game and there cannot be one. A browser has no raw sockets, so
`QTcpServer`, `QTcpSocket` and `QUdpSocket` do not work; Qt's own notes list all
server classes as unsupported on this platform and Emscripten has no UDP at all.
The LAN entry is therefore hidden here, through the `lanAvailable` context
property set in `android/main.cpp`. The code still compiles, because both game
engines create a `LanSession` unconditionally, and both constructors are inert.

The table is laid out for an upright phone, exactly as on Android, which locks
itself to portrait. A browser cannot be locked, so `index.html` asks the player
to turn the phone back.

## Building

On the build machine, from the top of the tree:

    sh wasm/setup.sh      # Qt and Emscripten into /tmp/snapszer-wasm
    sh wasm/build.sh      # build and assemble wasm/dist

From the phone, `wasm/remote-build.sh` does both over ssh and fetches
`wasm/dist` back. `wasm/deploy.sh` publishes that directory to the `gh-pages`
branch.

Two things about the toolchain are worth knowing before changing versions:

* Qt publishes the **single-threaded** WebAssembly kit only as a Windows-host
  archive. It works on Linux, because the WebAssembly libraries are host
  independent and the wrapper scripts are plain shell. The multithreaded kit,
  which does ship a Linux archive, is useless here: threads need COOP and COEP
  response headers, and GitHub Pages cannot set them.
* The Emscripten version must be exactly the one Qt was built with, 4.0.7 for
  Qt 6.11.1. Qt records it in `QtPublicWasmToolchainHelpers.cmake` and refuses
  anything else. A distribution package is almost always too new.

The host Qt supplies `moc`, `rcc`, `qmlimportscanner` and `lrelease` and must be
the same version as the kit.

## Licence

The game is MIT. Qt for WebAssembly, unlike Qt for the desktop and mobile
platforms, is offered only under the GPL version 3, so the published page as a
whole is distributed under the GPL version 3. `LICENSE-GPL-3.0.txt` travels with
the page and `index.html` carries the notice and the link to the source.
