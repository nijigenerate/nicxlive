# Binary format primitives source port

The authoritative original is `nijilive/source/nijilive/fmt/binfmt.d`, in full.
The implementation is public at `<fmt/binfmt.hpp>` through the existing SDK
`core` include directory. The old `fmt/binfmt.hpp` forwards to that single body.
No additional CMake source registration is required for this inline module.

| Original | Published implementation |
| --- | --- |
| `MAGIC_BYTES`, `TEX_SECTION`, `EXT_SECTION` | The exact eight bytes, retaining the existing native array API. |
| `inVerifySection` | Length short circuit followed by prefix equality, including arbitrary and empty sections. |
| `inVerifyMagicBytes` | Calls the actual section implementation with the magic bytes. |
| `inInterpretDataFromBuffer(T)` | Copies the complete first `sizeof(T)` bytes before output assignment, decodes big endian, returns `sizeof(T)`, ignores trailing bytes. |

Source byte-array arguments are retained `common::SharedArray<uint8_t>` values.
The span implementation and existing vector ingress borrow bytes only for the
duration of the call. No input descriptor is mutated and no byte allocation is
introduced by the conversion. A short input raises the canonical Source
`ArraySliceError(0, sizeof(T), length)` at original line 30 before output changes.
The complete local snapshot preserves input/output overlap ordering.

The selected Phobos implementation accepts integral/character/bool types,
integral enums, float and double; real and non-scalars are excluded. Native
correspondences use 1/2/4/8-byte integral types (including character types),
integral enum underlying types, float and double. Unsigned assembly avoids
signed-shift undefined behavior. Float/double decode their raw representation;
bool conversion follows the selected scalar conversion. A const output cannot
be assigned. No new conversion wrapper is imposed on existing callers.

The actual `fmt/fmt.hpp` INP loader's `readU32` invokes this implementation and
advances its offset by the returned byte count. Its existing outer EOF handling
is preserved. This connection does not accept the rest of the INP loader or the
whole `fmt/package.d` original. Phobos attribution is retained in
`core/licenses/phobos-binfmt-source.txt`.

Source bodies, template selection, call sites, public include publication and
provider signatures were reviewed directly. Configure, compilation, build,
tests and runtime execution were not performed. Native ABI, target floating
representation and exception transport have no execution evidence. This is one
SDK original, zero nijigenerate application originals.
