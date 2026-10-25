# Windows ATS 1.61.3.18 particle evidence

Source: the user's `bin/win_x64/amtrucks.exe`, supplied through Yandex Disk.
The executable is not distributed in this repository.

| Property | Value |
| --- | --- |
| ProductVersion | 1.61.3.18 |
| FileVersion build | 05126873bfae600294ce6dc4469a0edb8a06726a |
| SHA-256 | c60e8ee02cc97b3cc6ef8c6298bb5c3c23c4ffd330b8185b646fb1b608a3c7b4 |
| File size | 53,218,192 bytes |
| PE machine | AMD64 |
| Preferred image base | 0x140000000 |
| SizeOfImage | 0x3975000 |
| Allocator thunk RVA | 0x1f1520 |
| Particle allocation call RVA | 0x1584425 |
| Caller return RVA | 0x1584428 |
| Base particle_gen vtable RVA | 0x2435af0 |
| Particle container vtable RVA | 0x2435ad8 |

## Allocation ABI

The thunk at RVA `0x1f1520` adds `0x28` to RCX, then jumps to RVA `0x1f2220`.
The pool allocator reads its state through RCX and allocates `0x60`-byte nodes.
It returns a node pointer or NULL in RAX. The detour uses the Windows x64 signature `void* (void*)`.

The thunk's 16-byte signature is unique within the executable `.text` section:

```text
48 83 C1 28 E9 F7 0C 00 00 CC CC CC CC CC CC CC
```

At RVA `0x15842f8`, the particle-spawn code sets RSI to `generator + 0x650`.
At RVA `0x1584412`, it reads the container vtable.
At RVA `0x1584425`, it calls the vtable slot at `+8`.
The container vtable slot points to the allocator thunk.

At RVA `0x1584457`, the caller checks the returned pointer:

```text
48 85 C0       test rax, rax
74 4A          je 0x15844a6
```

The NULL branch skips particle initialization, list linking, and the live-count increment.
The emission timer advances before allocation at RVA `0x1583c30`.
Thus suppression does not prevent the emission loop from advancing.

## Object fields

These offsets come from constructor and spawn instructions, not Linux layouts.

| Offset from generator | Meaning | Evidence RVA |
| --- | --- | --- |
| 0x000 | Base vtable | 0x1581da1 / 0x1581da8 |
| 0x054 | Particle size X | 0x1583c53 |
| 0x058 | Particle size Y | 0x1583c5b |
| 0x634 | Base lifetime | 0x1584015 |
| 0x638 | Maximum live count | 0x1583c38 / 0x1583c46 |
| 0x650 | Particle container | 0x15842f8 |

The controller recognizes only the original three smoke-layer tuples:

| Layer | Count | Size X/Y | Lifetime |
| --- | --- | --- | --- |
| Haze | 40 | 0.090 | 3.0 |
| Mid | 20 | 0.040 | 0.7 |
| Core | 4 | 0.020 | 0.2 |

These tuples match `unit/hookup/smoke_new.sii` in the companion archive.
The controller also checks both vtables and the exact caller return address.
The derived `particle_emitter` class and dynamic-blend presets are not supported.
A different object or caller always proceeds through the original allocator.

## Verification limits

Static analysis confirms the ABI, field reads, and NULL-result branch for this exact executable.
Synthetic tests cover classification, rejection, and emission sampling.
The Windows DLL must still be tested with this game and compatible truck exhaust meshes.
No in-game runtime validation has been performed by this repository's automated checks.

The installer and DLL both reject an unknown executable hash.
The DLL also checks loaded target bytes, the NULL-result branch, and the vtable slot before registration.
SPF owns hook installation and removal. The detour does not write generator fields.
No file reads, logging, locks, or dynamic allocation occur inside the detour.
