# Windows smoke backend

The experimental profile for Windows ATS 1.61.3.18 is implemented.
See windows-1.61.3.18.md for the executable hash and static evidence.
The release manifest declares visual_smoke=true and runtime_validated=false.
The first value means that particle control code exists. It does not claim an in-game test.

Each future profile requires binary evidence and an in-game test.
Never populate game_sha256 from a product-version string alone.
The updater delivers published packages. It does not discover new object layouts.
