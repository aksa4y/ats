Experimental Windows exhaust-smoke controller for ATS 1.61.3.18.

Implemented:
- SPF-managed particle allocation hook with exact executable SHA-256 checks.
- Smoke emission controlled by truck telemetry.
- Preservation of unrelated particles and allocator call sites.
- Russian panel with hook state and particle counters.
- Installer support for the companion smoke archive, with a pinned SHA-256.

In-game runtime validation is still pending. Use a backed-up profile for the first test.
Enable the companion manually in the ATS Mod Manager after installation.
The DLL alone does not attach smoke emitters to truck models.

Download a fresh repository ZIP for the updated installation scripts.
Future ATS versions require a separately analyzed compatibility profile.
