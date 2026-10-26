# Win32 network-fragment reliability update

This change repairs fragment lifetime and transfer sizing in the existing engine. It does **not** replace the 2003 gameplay, weapon systems, HUD, graphics, or terrain deformation.

## Runtime fixes

- Match scalar `new fragbuf_t` with scalar `delete` in the active fragment unlink and queue-clear paths. Previously, `delete[]` was used with scalar allocations.
- Release each queued `fragbufwaiting_t` when a channel is cleared. The previous code released child buffers but leaked the waiting-list nodes.
- Reject null and empty fragment lists before unlinking.
- Validate out-of-band packet payload length and use a bounded formatter for out-of-band text.
- Reserve filename bytes before calculating the first outgoing file-fragment payload. The previous subtraction could produce a negative send size or a non-progressing transfer for small configured chunk sizes or long filenames.
- Validate incoming filenames before copying to fixed-size path fields. Reject directory traversal and absolute paths.
- Check complete incoming ordinary messages against the available network buffer size before copying fragments.
- Check file-reassembly sizes and first-fragment filename offsets before allocating or copying; limit untrusted incoming file transfers to 256 MiB to reduce out-of-memory risks.

## Verification

`python -m unittest modernize.tests.test_netchan_fragment_lifetime modernize.tests.test_bitbuf_stability modernize.tests.test_network_packet_limits modernize.tests.test_megabuild_integration -v`

17 checks pass, including C++98 compilation and an AddressSanitizer/UndefinedBehaviorSanitizer fragment-lifecycle harness. `python modernize/validate.py` passes project-structure checks, still reporting 605 historical missing source references across auxiliary projects.

**Not verified:** complete Visual Studio Win32 compilation, multiplayer interoperation, stability under actual network loss or large custom downloads, or parity with a newer engine. Do not treat this update as an error-free or crash-free certification.

## Preserved behavior

The network wire protocol, beta HUD atlas, built-in content mount, first-person beta models, original weapon code, and terrain-deformation code remain unchanged. AMD64 configuration and deferred Phase 7 work are unaffected.
