# Native node lifetime

Node, mask, Welding, parameter and binding handles remain retaining references. Closing a Puppet
does not authorize clearing nodes still retained by another model, history,
an array snapshot, a map entry, or a copied filter delegate.

## Collection boundaries

The native runtime tracks nodes weakly when they enter the hierarchy or a
Puppet scan. Collection runs after all Puppet members have been destroyed and
at `inDrainPendingTextureDisposals` (also called by the normal update/unload
path). If a caller retains a node beyond Puppet destruction, it remains intact.
After releasing that last external handle, run the normal update/drain boundary
to collect any remaining cycle. This is deferred cycle collection, not a claim
that every shared handle release immediately destroys an object.

These boundaries require quiescent model access, as do hierarchy mutation and
render-resource disposal: do not read, mutate, or acquire weak node handles
concurrently with collection. The registry mutex protects bookkeeping; it does
not make the model API safe for concurrent graph access.

## Ownership accounting

`retained_graph` counts actual ownership cells, not apparent node counts:

- Nodes own child arrays, mask arrays, Welding links, filter arrays, deferred
  changes, and native node-keyed Welding/attachment maps. Grid/Path method
  contexts, Projectable child caches and native notification listeners are edges.
- Parameters own their bindings, and bindings retain their parent parameter and
  Node/Parameter target. Native physics systems/drivers retain their real
  receivers. These cycles are included without weakening their handles.
- Array backing, map storage/entries, and shared native filter callable contexts
  are separate objects. Shallow copies and retained value pointers are therefore
  visible as external owners.
- Every candidate is pinned for the whole collection. Objects with ownership
  outside the discovered graph are roots; their reachable objects remain live.
- Only unreachable nodes drop descriptors. Shared backing and live targets are
  never modified by the collector. Weak parent/model pointers are not edges.

Unknown user callbacks and foreign retained-array backing are conservative:
their unaccounted references remain external roots. The collector does not
inspect arbitrary closure memory or guess the type of foreign storage. Hosts
must explicitly detach cycles owned by such extensions. It is not a general C++
garbage collector.

## Failure behavior and resource retirement

Discovery, marking, and retirement-capacity reservation finish before any
descriptor is dropped. Allocation failure defers collection without modifying
nodes; the next update retries. The collector never propagates an exception
through Puppet destruction.

Native drawable/texture retirement queues are reserved for the batch before
release. Shared vertex/UV/deformation atlas unregistering removes dead pointer
bindings without allocating. Compaction happens when the atlas is next consumed
at the normal resource drain/upload boundary; existing surviving offsets remain
valid until then. Source texture handles also retain their creating backend weakly and are retired
only when that backend is current, so colliding IDs in another renderer are safe.
Custom backends and custom node/texture destructors must still
honor their own non-throwing destruction contracts.

Removing a Welding link/filter and removing a hierarchy child now detach the
resulting array descriptor from unused backing cells. A zero-length view must
not keep the removed target alive indefinitely. Existing retained snapshots
remain valid owners.

## Regression coverage

`node_cycle_lifetime_test.cpp` covers normal/ancestor/detached/self masks,
independent and shared models, retained roots and reattachment, root replacement,
deleted subtrees (including while the model stays open), Welding removal/unload,
copied arrays/delegates/maps/map entries, native frame-initialized Welding caches,
INP round trips and late texture failures, exception unwinding, repeated unload,
allocation failure followed by retry (with geometry and both texture APIs),
source-texture retirement across backend switches with colliding texture IDs,
Grid/Path/physics cycles, parameter/binding/link cycles, and externally retained
bindings or driver contexts. The Ao INP is also loaded, updated, drawn and released
in a separate validation process; its 163 nodes and all textures/geometry/atlas
registrations are released.

The source of the pre-fix audit and its results are separate from these regression
tests. Regression tests assert expected safe behavior; none mark known failures
as expected successes. Reference expiry and queue/buffer counts test leaks even
on platforms without LeakSanitizer. ASan/UBSan cover the exercised accesses;
they do not prove safety for untested platform backends or concurrent misuse.
