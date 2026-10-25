# Windows smoke backend status

No Windows particle profile is implemented or verified.
The diagnostic package declares visual_smoke=false and an empty game_sha256 list.
Do not populate the list from a version string alone.

A future visual backend requires evidence from the exact Windows game executable.
The evidence must identify the particle function ABI, object layout, target bytes, and smoke identity checks.
Each supported executable needs its SHA-256 hash and an in-game smoke test.
The backend must leave unrelated particle generators unchanged.

The updater only delivers published packages. It cannot discover correct hooks or repair object layouts.
