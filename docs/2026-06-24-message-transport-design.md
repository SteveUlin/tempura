# tempura `comm` — A C++26 Extensible Message + Transport Framework

*Lead-architect synthesis, v2 (final). A learning instrument for ML + robotics dataflow, optimized for insight density and measurable tradeoffs — not throughput. Every C++26 feature claim below was compile-probed on the project's actual toolchain (GCC 16.0.1 20260226, `nix develop .#trunk`, `-std=c++26 -freflection`) before being committed to.*

---

## Executive summary

- **Thesis:** the byte-mover never needs to understand the payload to route, version, or store it. Identity, schema, and reduction live *beside* the bytes (capability/metadata), never *inside* the transport. We enforce as `static_assert` what every surveyed system (DDS, Cap'n Proto, MPI, zenoh) enforces by external IDL or unsafe casts — because we run `-freflection`.
- **Build on what exists, don't reinvent.** The repo already ships a full P2300 sender/receiver stack (`src/task`), a deterministic-friendly executor seam (`InlineScheduler`), an io_uring async substrate (`src/io_uring`), digesters (`src/hash`), and the established typed-error idiom `std::expected<T, std::error_code>` (`src/io_uring/io_uring.h`). Delivery, transports, errors, and digests **compose these**, they do not parallel them.
- **The open question is already solved by `src/task`.** Async PUSH = a `repeat_effect`/`then` loop; BARRIER/RENDEZVOUS = `when_all` over N per-rank receive senders; cancellation/partition = `stop_token`. Delivery is a per-pattern policy *expressed as a sender pipeline over one `Scheduler`* — the c10d "two faces over one transport" insight, made literal.
- **Determinism is a scheduler, not a struct field.** A single-threaded `SimScheduler` with a logical clock + seeded RNG owns all time and concurrency; latency injection, delivery wakeups, and the measurement clock all flow through it. "Same seed → identical trace" is then a property of the executor, not a hope.
- **Toolchain reality (probed today):** P2786 `is_trivially_relocatable` is **absent** → the Blit gate is hand-rolled, fail-*closed*. P3394 annotations **work** (`[[=Tag{n}]]`, `annotations_of`, `extract<Tag>`) but have no feature-test macro and require a *structural* payload; `type_of(annotation)` is `const Tag` (needs `remove_const`). `offset_of(m).bytes` works at consteval → `layoutId` is buildable. `reflection.h::dataMembers` must thread `access_context::current()` + `define_static_array` to be iterable.
- **Scope discipline (the critiques' loudest signal):** ship a *measurement by Phase 2*. The minimum learning loop is SimScheduler + Live + Structural + pub/sub + the sim-vs-real harness with the copy-count honesty oracle. Blit, the offset-arena, ShmTransport, crash-reclamation, and the dynamic-replay reader are **fenced behind "only if the instrument shows the crossover is worth it."**
- **Key decisions honored** (1–10 from the brief): serialization is a transport concern; static typed channels at the core, type-erasure only at edges; reflection-derived schema, no IDL; the tier ladder; two digests with the append-to-evolve asymmetry; message-as-capability; tier-set-intersection negotiation; immutable-shared/destination-form ownership; flat envelopes with orthogonal meta rings; patterns as generic algorithms. Where research contradicted a decision's *mechanism* (not its intent), the mechanism is revised and the revision is marked.

---

## 0. The structural law

Twenty surveyed systems converge on one spine:

> **The byte-mover must never need to understand the payload to route, version, or store it.** Identity, schema, and reduction live *beside* the bytes (as metadata/capability), never *inside* the transport.

Every clean seam obeys it (DDS "transport-agnostic", eCAL "no coupling to serialization", UCX "common-denominator HAL", Arrow "serialization-free payload + Flatbuffers metadata"). Every leaky seam violates it (ROS 2's IDL welded to DDS DynamicData; FlatBuffers' vtable offset *being* field identity, coupling evolution to zero-copy).

tempura's differentiator: **we run `-freflection`.** Disciplines those systems enforce with an external IDL compiler (Cap'n Proto ordinals, protobuf `reserved`) or push to unsafe (`zenoh` `unsafe impl ResideInShm`, MPI manual `offsetof`), we enforce as `static_assert` over P2996 member reflection. The IDL *is* the C++ struct; the schema compiler *is* `consteval`. **Verified feasible today:** `offset_of(m).bytes` and `annotations_of(m)` both evaluate at consteval on the project compiler (§7.3 records the exact gotchas).

---

## 1. Layered architecture and the constexpr/runtime line

Four layers. The library is one `tempura` INTERFACE target; each brick is `foo.h` + `foo_test.cpp` at the **flat `src/` root** with one `tempura_test(foo LABEL comm)` line — the repo's convention (no `src/comm/` subdir; nested modules like `bayes/` are legacy lineages being de-versioned, not the target). Strict downward dependency.

```
┌─────────────────────────────────────────────────────────────────┐
│ HARNESS    sim-vs-real comparison; composes benchmark.h          │  measures
│            (median+MAD, doNotOptimize) + comm-specific axes      │
├─────────────────────────────────────────────────────────────────┤
│ PATTERN    generic algorithms + concept reqs on the message      │  composes
│            PubSub<requires nothing> · Collective<Reducible>      │
│            DELIVERY = a sender pipeline over one Scheduler        │
├─────────────────────────────────────────────────────────────────┤
│ TRANSPORT  Transport concept; tier-set ∩ negotiation; Scheduler  │  moves bytes
│            seam. SimTransport(SimScheduler) ‖ TcpT(io_uring)     │  + locators
├─────────────────────────────────────────────────────────────────┤
│ MESSAGE    MessageTraits<T> · tier ladder · two digests          │  describes
│            Envelope · meta rings · Discovery registry            │  + capability
└─────────────────────────────────────────────────────────────────┘
  reuses: src/task (senders/Scheduler/when_all/repeat_effect/stop_token)
          src/io_uring (real async)  ·  src/hash (digests)
          src/meta/reflection.h (P2996 + annotation helpers)  ·  src/benchmark.h
```

**Why this exact cut** (informs: TensorPipe transport-vs-channel; UCX UCT/UCP mechanism-vs-policy; MPI BTL-vs-coll). Two universally-confirmed orthogonality axes: (a) *describe the message* ⊥ *move the bytes*; (b) *move the bytes* ⊥ *what shape the group is*. Message⊥Transport realizes (a); Transport⊥Pattern realizes (b). The substrate is delivery-agnostic: Transport exposes `send(bytes, locator)`, a receive primitive *that returns a Sender*, a `Scheduler`, and constexpr capabilities. Push-vs-rendezvous lives one layer up.

### The constexpr/runtime carve-out, drawn explicitly (per the `benchmark.h` precedent)

The framework moves real bytes over real sockets — I/O has no compile-time value. We honor `constexpr-by-default` where it *can* hold and consciously waive it where it can't, stating the line per surface rather than tagging everything `constexpr` and silently violating it:

| Surface | constexpr? | Why |
|---|---|---|
| `MessageTraits`, `layoutId`/`schemaId`, `negotiatedTier`, tag-discipline `static_assert`s, the reflection walks, `Envelope` shape | **consteval / constexpr** | pure capability/identity computation; this is where the principle earns its keep |
| `ReflCodec::serializedSize`, struct→bytes encode of a `constexpr` value | constexpr-capable | testable at compile time like `root_finding` |
| Transport I/O, delivery scheduling, the harness | **runtime by necessity** | sockets/shm/clocks; mirrors `benchmark.h`'s documented single carve-out |

---

## 2. The Message capability model

> **Decision 6 (honored):** a message is a *capability*, not a shape. `MessageTraits<T>` answers `{tier set, optional schema, optional serializer}`, reflection-defaulted; specialize only for containers and resources.

### 2.1 `MessageTraits<T>`

Informs: eCAL `Serializer<T>` static-method struct (cleanest serializer seam surveyed); Arrow `ExtensionType` registry; NCCL's *closed* `(op,datatype)` matrix (the anti-pattern we avoid by keeping traits open).

```cpp
// message.h
namespace tempura {

enum class Tier : unsigned { None = 0, Live = 1, Structural = 2, Blit = 4 };  // bitset
constexpr Tier operator&(Tier, Tier);  constexpr Tier operator|(Tier, Tier);  constexpr Tier operator~(Tier);

template <typename T>
struct MessageTraits {
  // Default for an aggregate: always Live (in-proc share) + Structural (reflection-serializable).
  // Blit is OPT-IN (§2.2) — never inferred for an arbitrary aggregate, because the relocation
  // proof cannot see into std:: members (verified: the walk can't read std::string internals).
  static constexpr Tier tiers =
      Tier::Live | (isStructurallySerializable<T>() ? Tier::Structural : Tier::None)
                 | (MessageTraits::blit_relocatable && isStandardLayout<T>() ? Tier::Blit : Tier::None);
  static constexpr bool blit_relocatable = false;   // specialize true for arena/POD types

  static consteval SchemaDescriptor schema();        // names+tags, order-independent
  static consteval LayoutDescriptor layout();        // offsets+sizes+endianness+ABI fingerprint
  static constexpr std::uint64_t layoutId = digestLayout(layout());   // §2.3
  static constexpr std::uint64_t schemaId = digestSchema(schema());   // §2.3
  using serializer = ReflCodec<T>;                   // §7.1
};

template <typename T>
concept Message = requires {
  { MessageTraits<T>::tiers } -> std::convertible_to<Tier>;
  { MessageTraits<T>::layoutId } -> std::convertible_to<std::uint64_t>;
};
}  // namespace tempura
```

### 2.2 The Live / Structural / Blit tier ladder — Blit deferred, fail-closed

> **Decision 4 (honored, scoped):** Live (in-proc, `shared_ptr<const T>`, zero copy) / Blit (same-host + matching layout fingerprint, memcpy) / Structural (cross-version/network/disk, tagged self-describing). **The capability ladder auto-degrades; always ship the copy floor** (informs: UCX short/bcopy/zcopy; TensorPipe CUDA-IPC→CPU-staged fallback; ROS 2 `can_loan_messages`→allocator copy; Ray inline→Plasma→pull).

| Tier | When | Mechanism | Gate | Informs |
|---|---|---|---|---|
| **Live** | same process | `shared_ptr<const T>`; move a `unique_ptr` to a sole owner | always | rclcpp IntraProcessManager; Ray immutable Plasma |
| **Structural** | cross-version / network / disk | tag-prefixed self-describing encode via `serializer` | `Message<T>` (always) | protobuf tags; Arrow IPC; MCAP Schema record |
| **Blit** *(Phase 3, fenced)* | same host, `layoutId` match | `memcpy` into shm/loaned slot; offset-pointers for unbounded | `BlittableMessage<T>` (below) | iceoryx loaning; FlatBuffers offset layout; Fast-DDS data-sharing |

**Blit is cut from v1 and fenced behind a measured need** (the simplicity critique's strongest point, and correct): the offset-arena allocator + relocatable container family + cross-process crash-reclamation watchdog is an *entire shared-memory IPC subsystem* with zero precedent in `src/`, orthogonal to the framework's thesis. **The two-rung ladder loses none of the distinctive lessons:** negotiation, the two digests, append-to-evolve, pluggable delivery, and sim-vs-real all live in Live+Structural. Critically, **`layoutId` still gates a `could-we-have-blitted?` fact the harness reports** (§8) — the honesty-oracle payoff survives without shipping the machinery.

**When Blit does land (Phase 3), the gate is hand-rolled and fail-closed** — two hard constraints proven today drive this:

> **REVISIT (mechanism, verified): P2786 `is_trivially_relocatable` is UNAVAILABLE on the project toolchain.** Probed: `__cpp_trivial_relocatability`, `__cpp_lib_trivially_relocatable`, `__builtin_is_trivially_relocatable` all absent on GCC 16.0.1 20260226. The design's prior "P2786 is the gate" is *aspirational*. The real gate is the reflection walk, plus an opt-in trait:

```cpp
template <typename T>
concept BlittableMessage =
    std::is_standard_layout_v<T>          // stable cross-process offsets; excludes vtables/bitfield ambiguity
    && MessageTraits<T>::blit_relocatable // OPT-IN: true only for whitelisted arena/POD types
    && relocatableByWalk<T>();            // consteval allowlist walk, fail-CLOSED
// A feature-probe in feature_probe.h flips to std::is_trivially_relocatable_v the day it lands.
```

> **REVISIT (correctness, verified): the `noOwningRawPointers` walk fails OPEN — invert it.** Probed: a naive "default true, blocklist `is_pointer_v`" walk on `struct{ std::string s; }` cannot even *see* `std::string`'s buried buffer pointer (the access-context walk can't read libstdc++ internals), so it would mislabel it Blit-safe — silent cross-process corruption, the worst failure for a fail-loud project. **`relocatableByWalk<T>` fails CLOSED:** Blit-eligible only if *every* member is a known-relocatable scalar or an explicitly whitelisted arena type; anything unrecognized (any `std::` class without a tempura specialization, any union/reference/raw pointer) is poison.

```cpp
template <typename T> consteval bool relocatableByWalk() {
  constexpr auto ctx = std::meta::access_context::current();
  bool ok = true;
  template for (constexpr auto m : std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^T, ctx))) {
    using M = std::remove_cvref_t<typename [: std::meta::type_of(m) :]>;
    if constexpr (isRelocatableScalar<M>())        { /* ok */ }
    else if constexpr (isWhitelistedArena<M>())    { /* ok: offset_ptr-backed */ }
    else if constexpr (std::is_class_v<M> && std::is_aggregate_v<M>) ok = ok && relocatableByWalk<M>();
    else ok = false;                                // unions, refs, raw ptrs, std:: containers → reject
  }
  return ok;
}
```

**Negative oracle is mandatory** (the feasibility critique's correct demand): the Phase-3 test asserts that `std::string`, `std::vector<int>`, `std::unique_ptr<int>`, and any struct holding them classify **NON-Blittable**, alongside the positive roundtrip test.

### 2.3 The two digests — `layoutId` vs `schemaId`

> **Decision 5 (honored, strongly validated):** `layoutId` (byte-image identity — offsets+sizes+endianness, *excludes names*; gates Blit) vs `schemaId` (name/tag identity, order-independent; drives Structural). Their asymmetry yields append-to-evolve.

The schema-evolution study re-derives exactly this: *"every evolvable wire format separates STABLE IDENTITY from MUTABLE NAME/POSITION."* iceoryx2/zenoh maintainers flag the conflation danger: *"a refactor preserving size+alignment but changing layout passes the size+alignment check yet silently corrupts."* Hence `layoutId` is a **full byte-image hash**, not `(name,size,align)`.

**Built on `src/hash`, not a hand-rolled mixer** (the tempura-fit critique, correct — an ad-hoc 64-bit mix risks `layoutId` collisions, the exact corruption the design exists to prevent). The novel part is *what we feed*; the mixing is solved:

```cpp
// digestLayout: gates the FAST path. Feeds src/hash/fnv.h (verified consteval-usable).
//   per member: offset_of(m).bytes, sizeof, alignof, a primitive type-kind code
//   + endianness byte + an ABI fingerprint (see below); EXCLUDES member names.
consteval std::uint64_t digestLayout(LayoutDescriptor);

// digestSchema: drives the EVOLVABLE path. Feeds the same hasher over the SORTED
//   multiset of (tag, type-kind). Order-independent; EXCLUDES offsets and names.
consteval std::uint64_t digestSchema(SchemaDescriptor);
```

> **REVISIT (scope claim, verified): `layoutId` must include an ABI fingerprint.** Offsets/`sizeof` are computed per-TU by the compiler. Two same-host processes built with different `-std`/libstdc++/flags produce different layouts (`long double` is the classic landmine). So `layoutId` mixes in compiler id+version, `__STDCPP_*` width macros, pointer width, and endianness. **Stated loudly:** Blit interop requires bit-identical layout, which in practice means *same toolchain, same flags, same headers — typically the same binary*. A cross-toolchain same-host pair then correctly *fails* `layoutId` and downgrades to Structural rather than matching by coincidence on a simple struct and corrupting on a `long double` one. A Phase-3 test asserts a `double`-vs-`long double` field difference flips `layoutId`.

**The asymmetry, concretely.** Append a tagged field: `layoutId` changes (Blit correctly refused between versions — the byte image differs), but `schemaId`'s *existing* tag contributions are unchanged, so Structural decode of old tags still works and a new tag is skipped by old readers. Stamp both in the connect handshake (informs: ROS 2 RIHS01 type hash; DDS TypeIdentifier; eCAL descriptor hash): a `layoutId` mismatch silently *downgrades the rung*; a `schemaId` mismatch triggers schema negotiation (§7).

### 2.4 Resources — borrow-only `{Live}`-only types

> **Decision 8 (honored):** resources (`GpuBuffer`) are borrow-only.

> **REVISIT (mechanism, corrected): drop ".retain() removed via requires-clause" — it conflates two mechanisms and the first isn't a real C++ operation** (you can't delete a member via a requires-clause). `{Live}`-only is enforced **purely through tier-set intersection**: a `{Live}`-only message ∩ a network transport = empty → typed error, never a silent device-memory deep-copy. If we additionally want `loan()`/`retain()` to be a *compile* error on resources, those are constrained member templates `requires (MessageTraits<T>::tiers & Tier::Blit) != Tier::None` so calling them on a `{Live}`-only `T` is ill-formed at the call site. One mechanism, stated precisely.

```cpp
template <> struct MessageTraits<GpuBuffer> { static constexpr Tier tiers = Tier::Live; };
```

Informs: Ray RDT (`tensor_transport=nccl`, reference-only lifetime, `wait_tensor_freed`); TensorPipe device-tagged tensors; NCCL borrow-only GPU buffers. The RDT lesson is a *warning*: bolting a collective-scoped transport onto an immutable-copyable model *leaked its constraints into the user API*. We pre-empt by making `{Live}`-only a first-class, statically-checked tier from day one.

---

## 3. The FLAT Envelope and the metadata-ring catalog

> **Decision 9 (honored):** `Envelope<Payload, Metas...> = { Payload data; tuple<Metas...> meta; }`. Rings are an orthogonal set. NEST only at representation/containment boundaries. Envelope is itself a Message (recursive closure).

```cpp
// envelope.h
template <typename Payload, typename... Metas>
struct [[nodiscard]] Envelope {
  Payload data;
  std::tuple<Metas...> meta;
  template <typename M> constexpr const M& get() const;
};
template <typename P, typename... M>
struct MessageTraits<Envelope<P, M...>> {
  static constexpr Tier tiers = (MessageTraits<P>::tiers & ... & MessageTraits<M>::tiers);
  // schema/layout: reflection over the flat aggregate {data, meta...}
};
```

**Why flat, validated.** iceoryx's chunk design (fixed ChunkHeader + *optional adjacent* user-header + payload) and Arrow's `custom_metadata` keep metadata *beside, contiguous, optional* — never a wrapper the byte-mover must unwrap. A flat aggregate is relocatable iff its parts are, so a Stamped/Sequenced message stays Blit-eligible — nesting would break that.

**Ship only the rings a working example consumes** (the simplicity critique, correct — the Open-Closed property makes deferral *free*: adding `Correlated` later is new code by construction):

| Ring | Carries | v1? | Informs |
|---|---|---|---|
| `Stamped` | `steady_clock::time_point` capture time | **yes** (telemetry demo) | DDS SampleInfo; eCAL memfile time |
| `Sequenced` | monotonic `uint64` seq | **yes** (ordering/dedup, §9.1) | iceoryx ChunkHeader seq; RTPS |
| `GroupRanked` | `{rank, world_size}` | **yes** (collective) | NCCL/MPI rank |
| `Reduced` | reduction op tag + count | **yes** (collective) | NCCL `(op,datatype)`; MPI `MPI_Op` |
| `Sourced`/`Topical`/`Correlated`/`Deadlined`/`Traced` | origin/topic/req-id/deadline/span | **admitted, added when a pattern needs one** | iceoryx originId; Zenoh KE; Cap'n Proto question id; DDS deadline; Zenoh forwardable ext |

`Deadlined` ships the moment cancellation needs it (§5.4) — and it *drives* a `stop_token`, never a value nothing consumes.

**Nest only at boundaries** — exactly three legitimate nesters, each a representation/containment change: `Serialized<T>`/`Framed<T>` (the Structural→bytes boundary; informs Cap'n Proto FAR-pointer landing pad, Arrow IPC encapsulated message), `Batched<T,N>` (many-into-one; informs DDS coherent set, Arrow RecordBatch, NCCL `ncclGroupStart`), and tunneling `Envelope<Serialized<Inner>, ...>` (a bridge re-wraps; informs iceoryx-DDS gateway, Zenoh router forwarding). Anywhere else, **add a ring, don't nest.**

---

## 4. The Transport concept, the Scheduler seam, and negotiation

> **Decision 7 (honored):** negotiation = tier-set intersection at subscribe time; empty ⇒ fallible subscribe returns a typed error; the component author decides crash-or-not.

### 4.1 The concept — bytes + locator + a Sender-returning receive + a Scheduler

The execution model is a **first-class part of the substrate**, resolving the feasibility critique's "single biggest gap": `Rendezvous` blocks, `Publisher` never blocks, and `SimTransport` is deterministic-single-threaded — contradictory without a named executor. Every transport exposes a `Scheduler` (the `src/task` concept), and its receive primitive returns a `Sender`. Completion flows through that scheduler — for `SimTransport` the deterministic `SimScheduler`, for `TcpTransport` the `IoUringScheduler`.

**Errors use the established idiom** (the tempura-fit critique, correct): `std::expected<T, std::error_code>` + a `comm_error_category`, matching `src/io_uring/io_uring.h`, so an io_uring-backed transport returns *one* error type with no translation layer.

```cpp
// transport.h
struct Locator { /* opaque: scheme + bytes, e.g. "sim://node/42", "tcp://host:port" */ };

enum class CommError { NoCommonTier, Unreachable, LayoutMismatch,
                       SchemaIncompatible, DeadlineExceeded, Closed, QueueOverflow };
const std::error_category& commErrorCategory();
std::error_code make_error_code(CommError);                 // ADL hook → std::error_code
template <typename T> using Result = std::expected<T, std::error_code>;

template <typename X>
concept Transport = requires(X x, std::span<const std::byte> b, Locator l) {
  // CAPABILITY ADVERTISEMENT — constexpr, so negotiation is compile-time-or-connect-time
  { X::tiers }      -> std::convertible_to<Tier>;     // which rungs this transport offers
  { X::reliable }   -> std::convertible_to<bool>;
  { X::same_host }  -> std::convertible_to<bool>;     // gates Blit eligibility
  { X::max_inline } -> std::convertible_to<std::size_t>;
  // EXECUTION SEAM — all delivery/completion runs on this scheduler (sim or io_uring)
  { x.scheduler() } -> Scheduler;
  // DATA CONTRACT — opaque bytes out; a Sender in (completion is structured async)
  { x.send(b, l) }       -> std::same_as<Result<void>>;
  { x.receive(l) }       -> Sender;                   // completes with the inbound frame
  { x.descriptor() }     -> std::convertible_to<TransportDescriptor>;
  { x.canReach(std::declval<TransportDescriptor>()) } -> std::same_as<bool>;
};

// OPTIONAL accelerated rungs — detected, never required (Phase 3+).
template <typename X> concept LoaningTransport = Transport<X> && requires(X x, std::size_t n) {
  { x.loan(n) } -> std::same_as<Result<LoanedChunk>>;     // iceoryx-style
};
```

### 4.2 Negotiation = tier-set intersection, with auto-degrade

```cpp
template <Message T, Transport X>
consteval Tier negotiatedTier() {
  constexpr Tier common = MessageTraits<T>::tiers & X::tiers;
  if constexpr (!X::same_host) return common & ~Tier::Blit;   // offsets meaningless cross-host
  else return common;
}
template <Message T, Transport X>
Result<Subscriber<T, X>> subscribe(X& tr, Locator l) {
  constexpr Tier t = negotiatedTier<T, X>();
  if constexpr (t == Tier::None) return std::unexpected(make_error_code(CommError::NoCommonTier));
  else return Subscriber<T, X>{tr, l, highestRung(t)};       // Live > Blit > Structural
}
```

**Why intersection + typed-error, validated.** This is MPI's Request-vs-Offered QoS matching and DDS RxO, made a *loud* error per the survey's repeated pitfall: *"QoS as an opaque grab-bag that silently prevents communication"* (ROS 2/DDS) and *"'zero-copy' that silently degrades, logging only RCLCPP_INFO_ONCE"* (rclcpp). The author sees `std::expected`; *they* decide `assert`-or-handle. The Zenoh "SHM intra-host only, silently degrades at the domain boundary" footgun becomes the *explicit* `& ~Tier::Blit` above.

### 4.3 Sim + real backends, side by side

```cpp
// SimTransport: deterministic, injectable — owns a SimScheduler (§4.4), the source of determinism.
struct SimTransport {                       // satisfies Transport
  static constexpr Tier tiers = Tier::Live | Tier::Structural;   // Blit added in Phase 3
  static constexpr bool reliable = true, same_host = true;
  SimScheduler& scheduler();
  LossModel  loss;          // drop prob, partition matrix — seeded, reproducible
  // latency is posted into the SimScheduler's logical timeline, not a passive clock field
  Result<void> send(std::span<const std::byte>, Locator);
  auto receive(Locator) -> /* Sender completing on the SimScheduler */;
};

// TcpTransport: the credible REAL side — backed by the existing io_uring substrate.
struct TcpTransport {                       // Structural only; same_host=false; reliable=true
  IoUringScheduler& scheduler();            // src/io_uring/io_uring_sender.h
  Result<void> send(std::span<const std::byte>, Locator);   // → asyncWrite Sender
  auto receive(Locator) -> /* asyncRead Sender on IoUringScheduler */;
};
```

The harness then drives **identical Sender pipelines** over `SimScheduler` vs `IoUringScheduler` — true apples-to-apples (the completeness critique's correct demand; the design previously hand-waved `TcpTransport`). A partition is a flip in `loss`'s matrix; a slow subscriber is a latency edge posted to the sim timeline.

### 4.4 `SimScheduler` — determinism is an executor, not a struct field

The single component that makes "same seed → identical delivery trace" *true* (the completeness + tempura-fit critiques, both correct: real threads + `steady_clock` are nondeterministic). Modeled on the existing `InlineScheduler` (run-to-completion on the calling context) but with a logical clock and a priority queue keyed by `(logical_time, seq)`:

```cpp
// sim_scheduler.h — satisfies the src/task Scheduler concept (schedule()→Sender, copyable, ==).
class SimScheduler {
 public:
  auto schedule() -> /* Sender that enqueues its op at current logical time */;
  auto scheduleAt(LogicalTime) -> /* Sender for delayed (latency-injected) delivery */;
  void run();                              // drain the event queue in logical-time order
  LogicalTime now() const;
  std::uint64_t& rng();                    // seeded; loss/latency draws are reproducible
 private:
  std::priority_queue</* (time, seq, op) */> queue_;  // single timeline; no OS threads
};
```

Determinism comes from **all time and concurrency flowing through this one scheduler** — latency injection (`scheduleAt`), delivery wakeups, the `Rendezvous` barrier (§5.3, cooperative not OS-blocking), and the harness's measurement clock. This is also the only place SHM crash-reclamation could be safely *deferred to* (no real crash in sim) when Blit eventually lands.

---

## 5. Patterns as generic algorithms, delivery as senders

> **Decision 10 (honored):** patterns = generic algorithms with concept requirements (pub/sub requires nothing; collective requires `Reducible`). Open-Closed: a new pattern is only NEW code.

### 5.1 Delivery rebuilt on `src/task` senders (resolves the open question)

> **The open question, restated:** pub/sub wants async PUSH (never block publisher); a collective wants BARRIER/RENDEZVOUS (all ranks block). These don't reconcile under one policy.

> **REVISIT (substrate, the largest completeness fix): do NOT hand-roll `std::function` callbacks + a `CountdownBarrier`.** The repo *already ships the idiomatic answer* in `src/task`. Building a parallel async model beside a structured-concurrency framework is the exact Ray-RDT-retrofit anti-pattern §9 warns against. The resolution:

- **PUSH (pub/sub)** = a `repeat_effect` loop over `transport.receive(l)`, each frame piped through `then(decode) | then(deliver)`. Latest-wins is a depth-1 history (§9.1). Never blocks the publisher.
- **RENDEZVOUS (collective)** = `when_all` over the world's N per-rank receive senders — `when_all` *is* the barrier: it joins N async completions, then fires once. 
- **Cancellation / partition / deadline** = the existing `stop_token`; `set_error`/`set_stopped` are the error and cancel channels, composing for free.

This makes the resolution *stronger* than the v1 prose claimed: "push vs rendezvous" becomes "**`repeat_effect` vs `when_all` over the same `Scheduler`**" — one substrate, which is precisely the c10d "two faces over one transport" insight the design appeals to.

```cpp
// delivery.h — DeliveryPolicy is a concept over Senders, not std::function.
template <typename D, typename T, typename X>
concept DeliveryPolicy = Transport<X> && requires(D d, X& tr, Locator l) {
  { d.run(tr, l) } -> Sender;                 // the delivery pipeline; completion = set_value/error/stopped
  { D::blocks_publisher } -> std::convertible_to<bool>;   // visible backpressure contract
};

// PUSH: repeat_effect over receive(); latest-wins history; never blocks publisher.
template <typename T, typename X> struct LatestWinsPush {     // rclcpp KEEP_LAST depth-1
  static constexpr bool blocks_publisher = false;
  History<T> history{HistoryKind::KeepLast, 1};               // §9.1
  auto run(X& tr, Locator l) -> Sender;                       // repeatEffect(tr.receive(l) | then(decode) | then(store))
};

// RENDEZVOUS: when_all over per-rank receive senders; releases when world is present.
template <typename T, typename X> struct Rendezvous {         // NCCL/MPI/Gloo Work::wait()
  static constexpr bool blocks_publisher = true;
  auto run(X& tr, std::span<const Locator> ranks) -> Sender;  // whenAll(tr.receive(r)...) on tr.scheduler()
};
```

**Per-pattern, not per-message or global:** delivery is a property of the *communication shape*, which is what a Pattern *is*. A global policy can't serve both (the open question's premise, confirmed). Per-message is wrong because the *same* `Tensor<float>` rides latest-wins telemetry pub/sub *and* a barrier-synchronized allreduce — delivery is contextual to the pattern. So `Subscriber` defaults `LatestWinsPush`; `Communicator` hardwires `Rendezvous` internally.

**Decisive evidence (every surveyed system that does both keeps two delivery shapes):** PyTorch c10d (NCCL collectives `Work::wait()` rendezvous *and* TensorPipe RPC async push — two faces, one transport); Ray's out-of-band collective escape (`ray.util.collective` bypassing the object store because *"forcing collective exchange through immutable copies is ~10× slower"*, and whose RDT retrofit *leaked* because a foreign delivery model was bolted on late).

### 5.2 Pub/sub — requires nothing

```cpp
// pubsub.h
template <Message T, Transport X>
class Publisher {
 public:
  template <typename... Args> Result<void> emplace(Args&&...);   // destination-form (Decision 8)
  Result<void> publish(std::shared_ptr<const T>);                // Live fast path: share, no copy
};
template <Message T, Transport X, DeliveryPolicy<T, X> D = LatestWinsPush<T, X>>
class Subscriber { D delivery_; /* ... */ };
```

Informs: rclcpp ownership routing (`unique_ptr` move = sole consumer; `shared_ptr<const>` = borrow-many); iceoryx loan→write→send; Zenoh KE-matching fan-out. Pub/sub imposes *no* concept beyond `Message` — the survey's finding that a topic bus needs neither membership, barrier, nor reducible payloads.

### 5.3 Collective — requires `Reducible`, runs cooperatively on `SimScheduler`

The collective study's central finding: a collective *demands strictly more* than pub/sub along four axes — static membership, rendezvous, lockstep ordering, reducible payloads. Each is a concept/type, not a runtime hope.

> **REVISIT (mechanism, two fixes): (a) keep the reduction op a TEMPLATED callable, not `std::function`-erased; (b) make associativity a *measured* property, not an honor-system bool.** The `root_finding` precedent records the brick was made constexpr precisely by *templating the callable* to avoid `std::function`; NCCL's `algorithm × T` monomorphization-count engineering is a throughput concern, a NON-GOAL here. And `Op::associative` as a static bool the author can lie about gives false confidence — the harness checks it empirically instead.

```cpp
// collective.h
template <typename Op, typename T>
concept Reducible = requires(Op op, T a, T b) {
  { op(a, b) } -> std::convertible_to<T>;        // closed binary op — that's all the concept enforces
};
// Associativity LICENSES ring-vs-tree reorder. It is NOT in the concept (un-checkable at compile time).
// Ship pre-wrapped ops so users never hand-roll a struct; std::plus/lambdas work directly:
struct Sum  { template<class T> T operator()(T a, T b) const { return a + b; } };
struct Max  { /* ... */ };  struct Min { /* ... */ };  struct Prod { /* ... */ };

template <Transport X>
class Communicator {
 public:
  Communicator(X& tr, Rank self, int world, Rendezvous& boot);   // membership fixed at construction
  int size() const;  Rank rank() const;

  // BARRIER with a deadline — CANNOT deadlock (the completeness critique, correct):
  Result<void> barrier(StopToken = {}, std::optional<LogicalTime> deadline = {});

  template <Message T, typename Op> requires Reducible<Op, element_t<T>>
  Result<void> allReduce(std::span<element_t<T>>, Op, StopToken = {});  // templated op, in-place

  template <Message T> requires Blittable_or_flat<T>
  Result<void> broadcast(std::span<element_t<T>>, Rank root, StopToken = {});  // §9.2
};
```

**The reduction core stays monomorphic over the op via a concept boundary** — ring-vs-tree selected inside by size, the op never type-erased:

```cpp
template <typename Op>
void allReduceCore(std::byte* buf, std::size_t stride, std::size_t count, Op reduce, Algorithm algo);
// Associativity licenses reordering ring↔tree; float + is NOT associative — see §8's deliberate measurement.
```

**Why this is Open-Closed.** Adding `Collective` was: two rings (`GroupRanked` + `Reduced`), one concept (`Reducible`), one class (`Communicator`), one delivery policy (`Rendezvous`) — **zero edits to Message, Transport, or Pub/sub.** This is the survey's universal answer (NCCL/MPI/Gloo/c10d all separate Communicator from bus) reduced to its concept essence.

> **REVISIT (scope, confirmed): do NOT unify pub/sub and collective behind one verb.** The collective study's #1 pitfall: *"forcing them together either over-constrains pub/sub or under-serves collectives."* They share the Transport substrate and `Scheduler`; `Reducible` rides as ordinary capability; they do **not** share a delivery model.

### 5.4 Cancellation, deadlines, and the barrier that cannot hang

The roadmap's old P4 oracle celebrated "barrier deadlocks on rank-count mismatch (loud)" — but **a deadlock is the OPPOSITE of loud** (the completeness critique, exactly right; it's the silent hang the eCAL #1206 watchdog lesson warns of). Fixed: `barrier()`/`allReduce()` take a `StopToken` + optional `deadline`; on a missing rank past the deadline they return `CommError::DeadlineExceeded`, never hang. The `Deadlined` ring (added here) *drives* a `stop_token` via the `SimScheduler`'s logical clock — so on the deterministic transport a stalled rank produces a reproducible, timestamped timeout, not a wall-clock hang.

---

## 6. Discovery and connection establishment

A hard requirement every handshake step (§2.3, §5, §7.3) silently assumed but never designed (the completeness critique, correct — DDS SPDP/SEDP, ROS 2 graph, NCCL `ncclGetUniqueId`+bootstrap, MPI PMIx, zenoh scouting all treat this as first-class).

```cpp
// registry.h — how (topic, schemaId, layoutId, Locator, TransportDescriptor) is advertised + matched.
struct Advertisement { TopicId topic; std::uint64_t schemaId, layoutId; Locator loc; TransportDescriptor td; };
class Registry {
 public:
  Result<void> advertise(Advertisement);
  std::span<const Advertisement> match(TopicId, std::uint64_t schemaId);  // schema-compatible peers
};
```

- **In-proc / SimTransport:** a single `Registry` instance — sufficient and deterministic (honors the NON-GOAL "no RouDi-style daemon; discovery is decentralized or in-process", now *designed*, not hand-waved).
- **Collective bootstrap:** the `Rendezvous` ctor argument is a shared store keyed by a job id (NCCL-style) — N ranks publish their `Locator` under `jobId/rank`, then `when_all` over the gathered set forms the `Communicator`.
- **The connect handshake is a designed message exchange:** exchange `{schemaId, layoutId, TransportDescriptor}`; `layoutId` mismatch → downgrade rung; `schemaId` mismatch → schema negotiation (§7). Not an assumed primitive.

---

## 7. Serialization + schema evolution

> **Decisions 1, 3 (honored):** serialization is a *transport* concern; schema is *reflection-derived* from plain C++ structs — no IDL, no codegen.

### 7.1 The serializer seam — eCAL-shaped, reflection-defaulted

```cpp
// serialize.h
template <typename S, typename T>
concept Serializer = requires(const T& v, std::span<std::byte> out, std::span<const std::byte> in) {
  { S::descriptor() }          -> std::convertible_to<SchemaDescriptor>;
  { S::serializedSize(v) }     -> std::convertible_to<std::size_t>;
  { S::serializeInto(v, out) } -> std::same_as<Result<void>>;     // direct into transport buf
  { S::deserialize(in) }       -> std::same_as<Result<T>>;
};
template <typename T> struct ReflCodec { /* tag-prefixed encode over the reflected members */ };
```

Serialize *into* the destination buffer (eCAL `CPayloadWriter` lesson: `size()` + `serializeInto(span)`, never a private staging copy).

### 7.2 Tag identity — the evolution discipline, P3394 with verified guardrails

> **Decision 3 refined (verified): the tag is a P3394 annotation, but the discipline does NOT block on it.** The schema-evolution study is explicit: order-based identity (zpp_bits, Boost.PFR positional) *"silently corrupts on reorder/insert"*; P2996 *alone* walks declaration order, which *"does not by itself solve evolution."* P3394 annotations are the right seam — **and I verified they compile on the project toolchain** (`[[=Tag{n}]]`, `annotations_of(m)`, `extract<Tag>(a)` all work). But three guardrails are mandatory, each learned from a probe:

```cpp
struct Tag { int n; };   // payload MUST be structural — std::string_view member is REJECTED (verified)

struct ImuSample {
  [[=Tag{0}]] double ax;        // ONE tag per member: a multi-declarator line (ax, ay, az)
  [[=Tag{1}]] double ay;        // REPLICATES the same annotation onto all three → tag collision.
  [[=Tag{2}]] double az;        // The v1 example was internally inconsistent; fixed here.
  [[=Tag{3}]] std::uint64_t t_ns;
  [[=Tag{4}]] double temp_c;    // append → next unused tag; never reuse a retired one
};
```

1. **No feature-test macro** (`__cpp_annotations` reports 0): guard via `__has_include(<meta>)` + a consteval probe that `annotations_of` returns sane results, in `feature_probe.h`.
2. **Annotation payload must be a structural type** — `std::string_view` is rejected; an evolution marker like `Since` must use a `fixed_string`-style structural payload (the repo has `meta/fixed_string.h`), not `std::string_view`.
3. **`type_of(annotation)` is `const Tag`** — the filter must `remove_const` before comparing to `^^Tag`. (This bit me in the probe; the working tag-discipline check below bakes it in.)

```cpp
template <typename T> consteval void checkTagDiscipline() {
  static_assert(tagsAreUnique<T>(),       "duplicate field tag — silent-corruption risk");
  static_assert(tagsRespectReserved<T>(), "reused a retired tag — protobuf 'reserved' rule");
}
// inside tagOf(member): for each a in annotations_of(m), if remove_const(type_of(a)) == ^^Tag → extract<Tag>(a).n
```

**Fallback if P3394 regresses** (it's unstandardized; a later trunk could rename `annotations_of`): the tag map degrades to a user-written `consteval tags()` member returning tag-per-index, *and* the framework is explicitly **pinned to this GCC trunk snapshot** in `feature_probe.h`. The append-to-evolve *lesson* is teachable either way — the annotation is sugar over the tag map.

What this buys that no surveyed IDL has — **evolution discipline as a build error**, not an external `protoc --conform`. Informs: protobuf `(field_number<<3 | wire_type)` + `reserved`; Cap'n Proto strictly-increasing `@N`; the 3-bit wire-type in the tag so an old reader can length-skip an unknown field. Cap'n Proto's name-derived-implicit-id trap (rename silently rebreaks the id) is dodged by making the tag an *explicit* annotation, never inferred from the name.

### 7.3 Structural tier = the record/replay format (capture-first, dynamic-replay deferred)

Informs: MCAP/rosbag2 three-record layering (Schema/Channel/Message); ROS 2 RIHS01; the verified caveat that *recording the schema string is easy; reconstructing a working dynamic deserializer is the hard half (ros2/rosbag2#1801)*.

> **REVISIT (scope, the simplicity critique — correct): v1 is capture-then-typed-replay, NOT a dynamic `DynamicMessage` reader.** Because endpoints are C++-only (Decision 3), a replay build *has the type* — so log `{schemaId, channel, tag-prefixed Structural bytes}` and replay *into the original `T`*. That pays the "Structural encoding IS the replay format" insight at a fraction of the cost. The "decode with `T` deleted from the build" dynamic reader — rosbag2's genuinely-unsolved hard half — is a deferred stretch chapter, not a v1 brick.

- Connect handshake ships the `SchemaDescriptor` **once per channel** (MCAP Channel record), keyed by `schemaId`; steady-state messages carry only `{channel_id, time, tag-prefixed bytes}`.

---

## 8. The instrumented sim-vs-real harness

> **The point of the project:** make tradeoffs *measurable*, side by side. **Compose `benchmark.h`, don't inherit it** (the simplicity critique, correct — `BenchResult` is a flat aggregate, not a base class; subclassing breaks aggregate-ness and the existing print path):

```cpp
// comm_bench.h
struct CommBenchResult {
  BenchResult timing;             // reuses medianOf/MAD/doNotOptimize VERBATIM
  double copy_count_per_msg;      // tier honesty
  double publisher_stall_ns;      // backpressure made observable
  Tier   tier_realized;           // negotiated vs requested
  std::size_t bytes_on_wire;      // tag overhead vs default-elision wins
};
template <Message T, Transport A, Transport B>
CommBenchResult compare(Workload<T>);     // same workload, two substrates
```

> **REVISIT (mechanism, the completeness critique — correct): `benchmark.h`'s auto-batcher fits throughput, not per-message latency across a blocking/async transport — and its "honesty oracle" is one MAD/median line, it cannot measure copies.** So each new axis gets a *defined* mechanism:

- **`copy_count_per_msg`** — an **instrumented allocator + memcpy counter inside `SimTransport`**. This is what makes "tier honesty" real: a Blit claim that silently fell back to Structural is *counted and caught*, not believed (the survey's recurring lie: *"'zero-copy' that silently degrades"* — rclcpp, eCAL, Zenoh SHM boundary).
- **`tier_realized`** — the `negotiatedTier` result, recorded per channel.
- **`bytes_on_wire`** — from `serializer::serializedSize`.
- **`publisher_stall_ns`** — measured on the `SimScheduler`'s logical timeline.
- **Latency, not throughput** — a **per-message timestamp trace through the deterministic `SimScheduler`**, *not* the auto-batcher. We do not claim verbatim reuse where the model differs; we reuse `benchmark.h` for the *steady-state-rate* axes and the trace harness for *per-message latency*.

**What the harness teaches** (each a "marketing claim" we refuse to take on faith): (1) tier honesty via copy-count; (2) sim⟷real delta — the deterministic sim is the *oracle*, the real (io_uring) transport's deviation is the lesson; (3) injected-failure response — sweep `loss` partition probability, plot delivered-fraction + stall (**failure observed + measured**, project scope); (4) **the eager-vs-rendezvous crossover** — sweep size, find the rung crossover *empirically* rather than hardcoding; (5) **non-associativity made visible** — float allreduce in ring order ≠ tree order, turning the un-checkable `associative` claim into a *deliberate teachable measurement* (the tempura-fit critique's better answer than an honor-system bool).

---

## 9. What the research surfaced

1. **`is_trivially_relocatable` (P2786) is unavailable today** (probed), so the Blit gate is a fail-*closed* reflection walk + opt-in trait, with a feature-probe that swaps to the real trait when it lands (§2.2). Getting this wrong would have excluded our own arena containers and mislabeled `std::string` as Blit-safe.
2. **Ordering / duplicate-delivery** — over a lossy `SimTransport` with retransmit, pub/sub can reorder or duplicate. The `Sequenced` ring (v1) optionally drives a reorder buffer / dedup in the delivery layer, gated by a policy. **Delivery-guarantee matrix** per (Transport, DeliveryPolicy): at-most-once/at-least-once, ordered/unordered — stated, and *measured* in the harness. (Was a minor gap; now in v1 scope because the lossy sim is the whole point.)
3. **Broadcast is a first-class collective**, not a `Reducible` special case (it has a root rank + asymmetric rendezvous). `broadcast(root)` is a distinct `Communicator` method; `element_t<T>` and the flat-buffer contract are stated explicitly, with a typed error (not silent exclusion) for non-flat `T`.
4. **History depth / overflow policy** — depth-1 latest-wins is wrong for command queues / transform buffers (DDS KEEP_LAST n / KEEP_ALL). `History{KeepLast(n), KeepAll, DropOldest, DropNewest}` with a **defined fail-loud overflow** (`assert` or `CommError::QueueOverflow` on KeepAll overflow). `send()`'s `Result<void>` error path now has defined semantics.
5. **Cross-process liveness/reclamation** (Zenoh watchdog; eCAL #1206 deadlock-on-holder-death) is a *known-incomplete* brick — **scoped out of v1 with Blit.** When ShmTransport lands, a reclamation protocol is a prerequisite phase, not an afterthought.
6. **`layoutId` needs an ABI fingerprint** — same-host ≠ same-layout across toolchains; `long double` is the landmine (§2.3).
7. **Promise pipelining** (Cap'n Proto) collapses dependent round-trips — high-leverage for robotics control loops, a natural `Correlated`-ring chapter, deferred past v1.
8. **The flat-vtable ABI is a one-way door** (UCX `_v2` shadow table; NCCL per-version shims) — our concept-based, header-only seams avoid the entire class. The lesson: **resist any runtime C-ABI plugin boundary** except at a genuine dynamic-load edge.

---

## 10. Phased roadmap — a measurement by Phase 2

Re-ordered so the instrument justifies each subsequent brick (the project's own benchmark-honesty ethos applied to its own roadmap). **The minimum vertical slice is ONE brick at the flat root** — `comm.h` + `comm_test.cpp` — split into separate headers only when it grows unwieldy (the repo's own pattern; `plot.h` is a 37 KB single file).

| Phase | Insight it pays | Deliverable | Oracle |
|---|---|---|---|
| **P0 · feature probe + traits** | *Bleeding-edge features fail as a red test, not silent UB; two digests yield append-to-evolve* | `feature_probe.h` (consteval-probe P2996/P3394, `#error` with required snapshot); `MessageTraits`, `schemaId` via `src/hash/fnv`, tag-discipline `static_assert` | probe fails loudly if a feature regresses; reorder a field → `schemaId` stable; append → old tags unchanged |
| **P1 · SimScheduler + SimTransport + pub/sub** | *Determinism is an executor; delivery is a sender pipeline; failure is injectable* | `sim_scheduler.h`, `SimTransport`, `comm.h` (Message Live+Structural, Publisher/Subscriber, `LatestWinsPush` via `repeat_effect`), in-proc `Registry` | **same seed → identical delivery trace**; partition matrix drops the right messages; reuse `src/task` senders |
| **P2 · harness + negotiation** | *Tradeoffs are measurable; "zero-copy" is type-checked AND copy-counted; the rung degrades loudly* | `comm_bench.h` (compose `BenchResult`, copy-counter in SimTransport), `negotiatedTier`, typed `NoCommonTier` | `{Live}`-only `GpuBuffer` → network subscribe returns typed error, never deep-copies; harness counts copies |
| **P3 · TcpTransport over io_uring** | *Sim-vs-real is apples-to-apples: identical Sender pipeline, two schedulers* | `TcpTransport` on `IoUringScheduler`; the `compare()` sim-vs-real delta | sim is the oracle; real transport's latency-tail/loss deviation is the measured lesson |
| **P4 · collective** | *A collective demands strictly more; Open-Closed proves it (only new code); a barrier CANNOT hang* | `Communicator`, `Reducible`, templated `allReduce` ring core, `Rendezvous` via `when_all`, `broadcast(root)`, `GroupRanked`/`Reduced` | allreduce == serial reduce; **rank-count mismatch → `DeadlineExceeded`, not deadlock**; float ring≠tree measured |
| **P5 · record/replay (capture-first)** | *Recording bytes is the easy half; the Structural encoding IS the replay format* | MCAP-style Schema/Channel/Message log; typed replay into the original `T` | replay a recorded log into the present `T` — decodes bit-exact |
| **P3.5+ (fenced) · Blit + offset arena** | *Unbounded data stays relocatable; the gate is a fail-closed walk* | **only if P2's crossover shows shm is worth it:** `offset_ptr`/`ShmVector`, `ShmTransport` loaning, reclamation protocol, `BlittableMessage` | Blit roundtrip == Structural roundtrip; **negative oracle:** `std::string`/`vector`/`unique_ptr` classify NON-Blittable |

---

## 11. Non-goals (explicit)

- **Consensus / Raft / leader election.** Failure is *observed*, not *solved*.
- **Cross-language endpoints.** C++-only (Decision 3). No `void*`+string-id dispatch, no IDL, no codegen, no bindings — this licenses concept-based compile-time seams over C-ABI vtables.
- **Production throughput / a real broker.** Insight density over GFLOP/s. No daemon; discovery is in-process (`Registry`).
- **A wire format we own.** Structural rides reflection-derived tag-prefixed bytes; serialization is pluggable (eCAL seam).
- **Schema evolution beyond append + rename.** Append-only tags + retire-via-reserved is the whole policy (Cap'n Proto/FlatBuffers discipline); no XTypes DHEADER/EMHEADER machinery.
- **General GPU collective library.** One allreduce + broadcast over a sim/CPU transport teaches the *shape*; NCCL/Gloo are integration targets, not rebuild targets.
- **Dynamic runtime transport plugins (DLLs).** The flat-vtable ABI is a one-way door (§9.8). Transports are compile-time concept models.
- **A parallel async runtime.** Delivery composes `src/task`; we do not reinvent senders/schedulers/stop-tokens.
- **Blit / shared-memory IPC in v1.** Fenced behind a measured need (§10); the offset-arena + crash-reclamation subsystem is its own project.
- **A dynamic `DynamicMessage` reader (decode with `T` deleted).** Deferred stretch chapter; v1 is capture-then-typed-replay (§7.3).
- **Promise pipelining / capability RPC.** Acknowledged high-leverage (§9.7), deferred past v1.

---

## 12. Remaining open questions for the user

1. **Real transport choice for the sim-vs-real harness (P3):** `TcpTransport` over the existing io_uring substrate is the credible, already-built option. The completeness critique wants exactly this; the simplicity critique notes the *cheapest* "real" is a same-host loopback (TCP-over-localhost or a unix socket), not shm. **Recommendation: TCP-localhost over io_uring** — cheapest real backend that still exercises the full async path. Confirm, or do you want a unix-socket variant too?
2. **Annotation-payload structurality (verified constraint):** evolution markers richer than an integer tag (e.g. a `Since("v2")` version string) need a *structural* payload — `std::string_view` is rejected by the compiler. Acceptable to use the repo's `fixed_string` for such markers, or keep evolution metadata to integer tags only in v1?
3. **Toolchain pinning:** P3394 annotations are unstandardized and have no feature-test macro; the framework is pinned to *this* GCC trunk snapshot (16.0.1 20260226). Is a hard pin (with `feature_probe.h` `#error` on mismatch) acceptable, or do you want the user-written `consteval tags()` fallback wired from day one so the build survives a trunk bump?
4. **Delivery-guarantee default (§9.2):** over the lossy sim, should the default pub/sub policy be at-most-once-unordered (simplest, drops on loss) or at-least-once-with-dedup (the `Sequenced` ring drives a dedup buffer)? The former is a cleaner teaching baseline; the latter is more realistic for robotics. **Recommendation: at-most-once default, at-least-once as an opt-in policy** so the harness can *measure* the difference.
5. **Does the user want a compile-first spike committed before P0?** The codebase discipline ("verified build-before-delete"; "dormant modules need a correctness oracle, not just a green compile") argues for landing `feature_probe.h` + a one-struct end-to-end `comm.h` slice *first*, then writing memory, before committing to the 6-phase plan. I've already verified the load-bearing reflection patterns compile — this would harden that into a checked-in oracle.
