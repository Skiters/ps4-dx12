# Full port to the pinned original — implementation in progress

The authorized reference is the public `shadps4-emu/shadPS4` revision
`e0a466753a1aacbc8db8253be685bf7ab4d9daae`, checked out without modifications at
`F:\PS4Dev\work\shadps4-original-e0a46675`. Its recursive submodule lock is
captured in this task's `work/original-submodule-lock.txt`. The old `0c340b4`
baseline and its gameplay/FPS evidence are not this reference.

The native source remains `F:\PS4Dev\work\shadps4-dx12-src`, branch
`fix/mixed-cmask`, based on `e84e31133320830db6268f3beefe9ddda1fb08cc` with
uncommitted implementation changes. No task commits/pushes or game launches have
occurred. The user-requested experimental snapshot below is installed in the
existing catalogue; the previous default remains selected. Full migration continues.

## Test boundary

No tests, game runs or benchmarks have been executed for this full-port task.
The implementation/registry boundary remains open. Compiling/linking the
integrated Release core and compiling host shaders are allowed before that
boundary. Earlier 221/221 results belong only to the previous task/binary.

Do not run a targeted test merely because a single subsystem now compiles.
All implemented original paths, build/host-shader/dependency/package integration
must be adapted first. Preserve the existing tests and prepare new contents,
ordering and lifetime fixtures while implementing.

## Later production: anisotropic manual sampling, 2026-10-12

### Production after preview three: independent buffer/image versions

The original buffer and image caches keep separate post-write contents. Native
exact-base/full-range raw buffers and R32 float/uint images now preserve those
domains for complete protected 4-KiB pages, one mip/layer/sample, at most 64 MiB,
with original linear-image readbacks disabled. Prewrite contents initialize the
image before both writes. CPU reads expose only the buffer result and retain the
image behind read-only watches. PageWatch forwards actual fault access type;
CPU writes retire uses and invalidate the image, while exact-base GPU buffer
writes discard only its logical identity and retain previous binding leases.
Buffer watch release rearms remaining independent image protection. Remapping,
unmapping and protection changes resolve buffers before image invalidation.

The full-core `separate-buffer-image-owned` fixture prepares simultaneous GCN
vertex buffer zero/image quarter writes, subsequent pixel image loads, actual
protected CPU reads/writes and later exact-base buffer invalidation. Three warm
versions use an independent complete 64-KiB backing/padding oracle. All owned
attachment modes now use two mapped views of one physical allocation, so backing
callbacks cannot recurse through the protected guest pointer. ClampRangeSize
leaves these small fixture buffers unchanged. Fault-kind checks compile actual
read and injected-write expectations. No fixtures execute before the boundary.

Core/checks compiled (`command-20261012-151334-55980.log`, 13.753 s;
`command-20261012-151334-56208.log`, 19.876 s). Ten full-core fixtures compile/link
against fresh production objects in `work/independent-r32-final-<fixture>-compile.log`.
Group 52 stays partial: formatted buffer reads, pressure/incompatible-view
resolution, queued linear-image readbacks, other formats/layouts/subresources,
partial/sub-DWORD and atomic aliases are still open or explicitly rejected.
GPU contents, protected CPU behavior and lifetime acceptance remain unverified.
Registry audit with 52 groups passed (`command-20261012-152839-24340.log`,
21.243 s): 982 files, 13,358 functions, 12,186 dispatch entries, 1,454 associated
functions, 13 explicit mappings and zero semantic closures. Its preceding failure
was a nonexistent page_watch.cpp endpoint; PageWatch is header-only and the
registry path was corrected. Non-deprecation build diagnostics and diff checks
are clear. No tests or games ran.
The immutable third preview excludes this work; migration continues.

User-requested immutable preview four is now installed in the existing desktop
catalogue: `dx12-20261012-full-port-preview-c5683c4914ea`, display name
`DX12 — перенос №4 — 12.10.2026`, executable SHA256
`c5683c4914eaf1ec9a3c2848a7337fd4b678b0a54341a0b7a4313e2e13632157`.
Its 76 manifested files include the exact dirty patch, matching PDB/MAP and
pinned runtime/compiler provenance. Static import/hash checks passed; no runtime
or game execution. Packaging log `command-20261012-152916-93984.log`, 36.348 s.
Twenty save files were copied and hash-checked into an isolated profile from the
selected first preview. Existing desktop, selected core/settings, saves and
previous selected package were verified unchanged. Deliverables live in task
`outputs/DX12-preview-4-20261012`. It includes production groups 47–52; subsequent
work must not modify its immutable package. Full migration continues.

### Production after preview four: formatted image and metadata buffers

Readonly formatted buffers now preserve the original small-buffer distinction:
at most 16 KiB without GPU-modified ownership uses CPU streaming; GPU-modified
or larger exact buffers synchronize the independent R32 image into the resident
arena. Native aligned descriptor extents remain separate from original sharp
size/base. GPU texture copy and retile preserve all padding, then restore the
actual reflected buffer state. Synchronization retains existing dirty ownership
and introduces no new guest write. Initial snapshots, linear/retile/mask storage
use actual device allocation requirements in cache pressure accounting and retire
at the submission fence. Prepared captures retain current placed memory without
replacing a coalesced batch envelope. Active clean tiled independent versions
are excluded from pressure victims, following original GC's unsupported tiled
download workaround. Linear pressure download and GC scheduling stay open.

The real GCN independent-version case now distinguishes formatted load before
CPU buffer reconciliation (quarter image value copies into the GPU arena and
later CPU visibility) from formatted load after reconciliation (zero CPU stream
while image loads still see quarter), over three full-allocation/padding oracles.
Core/checks compiled (`command-20261012-154149-2400.log`, 6.913 s;
`command-20261012-154149-99656.log`, 8.827 s). Ten fixtures were refreshed in
`work/formatted-pressure-final-<fixture>-compile.log`. Registry group 53 is partial;
audit passed (`command-20261012-154214-102576.log`, 21.896 s): 1,457 associated
functions, 13 explicit mappings, zero semantic closures. Non-deprecation
diagnostics and whitespace checks are clear. No runtime execution.

Metadata synchronization now additionally fills HTILE DWORD 0x0000000f or CMASK
ffffffff/dddddddd/eeeeeeee/ffffffff for 1/2/4/8 samples in the native arena only.
The original optional's other-sample-count value is engaged zero and is preserved;
FMASK does not synthesize a value. Live metadata takes precedence over image
copies. Exact typed UAV views/CPU+GPU descriptor heaps and placed memory are
retained before recording, with a UAV barrier and restored buffer state. The
original CPU-stream policy still applies after a small buffer loses GPU dirtiness.

The actual owned readonly-depth-buffer GCN mode prepares three additional warm
HTILE cases: real depth metadata registration, raw GPU writer, formatted RGBA8
load/export, protected CPU read of the complete 4-KiB arena, later CPU zero write
and small formatted stream. Full 64-KiB backing/padding/color/depth expectations
remain independent. Core/checks compile (`command-20261012-154953-58364.log`,
13.045 s; `command-20261012-154953-105784.log`, 18.018 s). GCN fixture links
(`command-20261012-155144-76076.log`, 0.443 s), compile-only. Group 54 stays partial;
GCN CMASK samples, concurrent metadata writers, partial/unaligned ranges, owner
recycling and final GPU/CPU/fence contents remain open. Preview four excludes all
of this later source and remains immutable. Continue the complete migration.

Registry audit with 54 groups passed (`command-20261012-155504-65040.log`,
22.54 s): 982 files, 13,358 functions, 12,186 dispatch entries, 1,458 associated
functions, 13 explicit mappings and zero semantic closures. All ten fixtures
compile/link against metadata production objects in
`work/formatted-metadata-final-<fixture>-compile.log`. No non-deprecation
diagnostics or whitespace failures. No tests/games ran.

The additional actual `formatted-cmask-owned` GCN mode prepares twelve cases
over three warm versions and 1/2/4/8 samples. Real fast-clear-enabled color draws
register owner/sample identity with expanded physical metadata. Large 32-KiB
clean formatted arenas must synthesize the sample pattern; small 4-KiB clean
streams must read physical 0xff, then large arenas synthesize again. Complete
64-KiB physical contents/padding and each color sample remain independently
checked; color-only CPU reconciliation leaves metadata bytes untouched. Fixture
compile/link passed (`command-20261012-155903-60620.log`, 0.454 s), not executed.

R32 SINT now joins FLOAT/UINT independent versions using the existing typed R32
bit-preserving transfer path. The real GCN version fixture prepares all three
numeric views over four warm values, including negative float bits, with float
export alpha independent of integer image defaults. Twelve cases retain CPU
domain, image loads, formatted arena versus CPU-stream, invalidation and complete
padding expectations. Core/checks compile (`command-20261012-160225-68744.log`,
6.841 s; `command-20261012-160225-11436.log`, 3.833 s). Final actual numeric/GPU
acceptance remains open; no semantic closure and no runtime execution.

### Production after preview four: independent R32 mip chains

Independent FLOAT/UINT/SINT image versions now retain valid full mip chains on
one layer/sample and complete page-backed ranges. Formatted buffer synthesis
uses the actual device footprint for every mip/layer, allocates and prepares
all copy/tiler descriptors before recording, initializes the complete output
from the buffer snapshot and replaces only logical texels. This retains padding
and transfers the whole chain even when the shader selects only one mip. All
transient allocations participate in the same accounted submission retirement.

The actual GCN fixture now prepares twenty-four basic cases: one/two mip chains,
three numeric views, four warm bit patterns. The second chain uses the guest
base-level view and physical mip offset through raw-buffer offen instructions;
modified programs have distinct hashes. Twelve additional cases write image
mip zero to half and mip one to quarter while the buffer remains separate, then
require formatted transfer and a real CPU fault to expose both mips. Per-mip
loads, exact whole allocation/padding, CPU/exact-base GPU invalidation and the
original large-clean versus small-clean formatted behavior remain independent
expectations. No GPU execution or semantic acceptance is claimed.

Core/checks compile (`command-20261012-161142-43208.log`, 7.446 s;
`command-20261012-161142-64312.log`, 4.74 s); no non-deprecation diagnostics and
whitespace checks pass. The extended full-core fixture compiles/links
(`command-20261012-161521-87616.log`, 0.441 s), not executed. Group 55 is partial;
arrays/cubes/volume/MSAA, broader physical aliases, partial/simultaneous formatted
writers, linear pressure readback and final actual shader/state/fault/fence
acceptance remain open. Preview four is immutable and excludes this source.

The 55-group inventory refresh passed
(`command-20261012-161913-111540.log`, 22.429 s), retaining 1,458 associated
functions, 13 declared mappings and zero semantic closures. Its ten current
full-core fixtures were refreshed in `work/r32-mips-final-<fixture>-compile.log`.
Continue the complete migration before any final tests or games.

### Production after preview four: independent R32 array layers

Full-page independent R32 versions now retain all physical 2D array layers.
The existing resource/view geometry and formatted transfer enumerate every
native mip/layer subresource and retile into the corresponding guest offsets;
ownership and CPU watch protection cover the complete physical image range.
Single-sample/non-volume, exact-range, format and backing restrictions remain.

The real GCN fixture prepares forty-eight basic cases over one/two layers,
one/two mips, FLOAT/UINT/SINT and four warm values. Explicit MIMG DA/layer
coordinates select the last layer, with raw and formatted buffer offsets derived
from its guest layout. Thirty-six extra cases create different values in first
layer/mip and selected layer/mip, require formatted transfer of both, then read
the first layer separately. Whole 64-KiB contents/padding remain independent
expectations. Color storage moved to the final 4-KiB page to fit both layers.

Core/checks compile (`command-20261012-162758-57872.log`, 6.819 s;
`command-20261012-162758-68220.log`, 3.823 s), with zero non-deprecation diagnostics
and passing whitespace check. Attachment fixture compiles/links
(`command-20261012-162822-27516.log`, 0.438 s), compile-only. Group 56 remains
partial: actual array/cube shader/GPU/fault/fence acceptance, volume/MSAA,
broader/partial/concurrent writers and linear pressure are open. Immutable
preview four excludes these changes. No tests or games ran.

All ten current full-core fixtures refreshed in
`work/r32-arrays-final-<fixture>-compile.log`, compile-only. The 56-group inventory
passed (`command-20261012-162930-42984.log`, 21.844 s): 982 original files,
13,358 functions, 12,186 dispatch entries, 1,458 associations, 13 explicit
mappings and zero semantic closures. Following preparation also adds twelve
basic and twelve divergent-face cube cases: actual Cube sharp with six physical
faces, integer IMAGE_LOAD/STORE DA coordinates selecting face five/zero and
complete physical/padding expectations including four untouched faces. Counts
are now sixty basic and forty-eight divergent mip/layer/face cases. Cube mip
chains, multiple cubes and direction-based sampling acceptance remain open.
The six-face extension compiled/linked (`command-20261012-163136-81488.log`,
0.435 s), not executed.

### Production after preview four: independent R32 vector formats

Independent versions now also permit R32G32/R32G32B32A32 FLOAT/UINT/SINT.
Formatted transfer validates actual footprint row size and passes the native
4/8/16-byte texel size into the existing tiler, preserving every channel and
padding. Expanded 96-bit, packed/normalized and sub-DWORD formats stay outside
this change. Actual typed UAV load/store support is validated before binding.

The full-core GCN fixture prepares 180 basic and 144 divergent mip/layer/face
cases across all three data formats. Reduced RGBA32 and rectangular RG32
dimensions retain complete page-backed chains inside the same allocation.
Every channel starts with the warm bit pattern. Full physical expectations
distinguish raw first-DWORD buffer stores from pinned IMAGE_STORE absent-channel
zeroing, including protected CPU visibility after formatted synchronization.
Program hashes now use separate operation namespaces plus descriptor shape;
this avoids collisions when a cube's six layers expand the shape index.

Core/checks compile (`command-20261012-163437-50260.log`, 7.502 s;
`command-20261012-163437-3052.log`, 4.797 s), no non-deprecation diagnostics,
and whitespace passes. The vector fixture linked before the final rectangular
geometry adjustment (`command-20261012-163504-100128.log`, 0.451 s). Current
fixture relink and inventory refresh follow. Group 57 remains partial; actual
GPU/fault/fence/numeric acceptance and the complete migration remain open.
Immutable preview four excludes this source. No runtime tests or games ran.

All ten final vector fixture executables refreshed in
`work/r32-vectors-final-<fixture>-compile.log`, including final rectangular
geometry (`command-20261012-163719-12552.log`, 0.435 s), compile-only.
The 57-group inventory passed (`command-20261012-163633-5092.log`, 21.792 s):
1,459 associations, 13 explicit mappings, zero semantic closures.

### Production after preview four: independent R32 volume slices

Complete R32-family volume domains now retain separate buffer/image versions.
Existing 3D native footprints include all Z slices of each mip; the tiler's
dispatch depth and guest per-mip Z layout transfer the whole volume into the
prior buffer snapshot. Physical padding and every native channel are preserved.
No array DA flag is used for actual 3D IMAGE_LOAD/STORE XYZ coordinates.

The GCN fixture adds seventy-two basic/seventy-two divergent cases, totaling
252 basic/216 divergent cases. Base volumes have two slices; two-mip volumes
shrink four slices to two, with distinct mip-zero/Z-zero and selected mip/Z-one
values. Initialization/oracles enumerate each mip's actual depth. Reduced
rectangular dimensions keep the complete volume within page-backed geometry.
Full physical bytes/padding, untouched slices, formatted transfer, real protected
CPU access and CPU/exact-base GPU invalidation remain independent expectations.

Core/checks compile (`command-20261012-163930-50596.log`, 6.902 s;
`command-20261012-163930-109056.log`, 3.846 s), no non-deprecation diagnostics,
and whitespace passes. The volume fixture compiles/links
(`command-20261012-164006-111036.log`, 0.44 s), not executed. Group 58 is partial;
actual GPU/CPU/protection/fence acceptance, multisampling, expanded/packed/
normalized/sub-DWORD and partial/concurrent aliases and linear pressure remain
open. Immutable preview four excludes these changes. No tests or games ran.

All ten volume fixture executables refreshed in
`work/r32-volumes-final-<fixture>-compile.log`, compile-only. The 58-group inventory
passed (`command-20261012-164102-68524.log`, 21.859 s), with 1,460 associations,
13 explicit mappings and zero semantic closures.

### Production after preview four: independent RGB32 native expansion

Full-page independent RGB32 FLOAT/UINT/SINT domains now use the existing
RGBA32 native allocation and sixteen-byte footprint while retile writes only
twelve guest bytes per texel. Native alpha is skipped and guest sampled alpha
retains constant-one mapping. Padding and adjacent texels remain preserved in
the prior buffer snapshot; exact domain watches and accounted fences are shared.

The GCN fixture adds thirty-six basic/twenty-four divergent RGB32 cases over
one/two array layers and a two-slice volume, three numeric views/four warm
values. The total is 288 basic/240 divergent cases. Every RGB channel starts
with the warm bits; full physical/padding expectations distinguish twelve
guest bytes from sixteen native bytes and raw first-DWORD from full image-store
component results. Larger RGB cube/mip chains do not fit this fixed fixture.

Core/checks compile (`command-20261012-164245-40860.log`, 6.866 s;
`command-20261012-164245-42988.log`, 3.805 s), no non-deprecation diagnostics,
and whitespace passes. The RGB32 fixture compiles/links
(`command-20261012-164432-97228.log`, 0.439 s), not executed. Group 59 remains
partial: actual conversion/shader/CPU/fault/fence acceptance and MSAA/formats/
partial/pressure integration stay open. Immutable preview four is unchanged.

All ten RGB32 fixtures refreshed in `work/rgb32-final-<fixture>-compile.log`,
compile-only. The 59-group inventory passed
(`command-20261012-164517-45828.log`, 21.736 s), retaining 1,460 associations,
13 explicit mappings and zero semantic closures.

### Production after preview four: independent RGBA8 normalized versions

Full-page RGBA8 UNORM/SNORM versions now retain the same independent image and
buffer domains. The native image quantization and typed/raw view lowering
remain in use; formatted synchronization preserves exact four physical bytes
and prior buffer padding without introducing guest writes. CPU faults/writes,
exact-base GPU invalidation and original small-clean streaming remain shared.

The actual GCN fixture adds 56 basic/48 divergent normalized cases, totaling
344 basic/288 divergent cases. Independent warm encodings include negative
SNORM; quarter/half/one physical bytes are 64/128/255 UNORM and 32/64/127 SNORM.
Matching normalized formatted buffer views decode them, including SNORM half's
129 export. Complete image/arena/CPU bytes and untouched allocation/padding,
distinct domain writes, full mip/layer/face/Z transfers and protection are
prepared. Numeric/device rounding/capability acceptance remains unverified.

Core/checks compile (`command-20261012-164726-10864.log`, 6.853 s;
`command-20261012-164726-96812.log`, 3.801 s), zero non-deprecation diagnostics,
and whitespace passes. Attachment compiles/links
(`command-20261012-164812-61620.log`, 0.445 s), not executed. Group 60 is partial;
RGBA8 integer/SRGB, 16-bit/packed/sub-DWORD, MSAA/partial/concurrent and pressure
acceptance remain open. Preview four excludes these changes. No tests ran.

All ten RGBA8 normalized fixtures refreshed in
`work/rgba8-normalized-final-<fixture>-compile.log`, compile-only. The 60-group
inventory passed (`command-20261012-164959-57644.log`, 21.69 s), with 1,460
associations, 13 explicit mappings and zero semantic closures.

### Production after preview four: independent RG16/RGBA16 versions

Full-page RG16/RGBA16 UNORM/SNORM/FLOAT versions now use the same separate
image/buffer ownership. Actual four/eight-byte footprints and retile preserve
sixteen-bit physical components; formatted-buffer sharp stride/record counts
retain the complete original extent and matching normalized/half decoding.
Raw DWORD writes and dmask R absent-channel zeroing remain distinct.

The real GCN fixture adds 168 basic/144 divergent cases across both data
formats/three numeric views, totaling 512 basic/432 divergent cases. Warm data
includes exact positive/negative IEEE half encodings and independently quantized
UNORM/SNORM components. Quarter/half/one encodings are 3400/3800/3c00 FLOAT,
16384/32768/65535 UNORM and 8192/16384/32767 SNORM. CPU writes use actual
sixteen-bit representation. All-channel/full physical/padding expectations,
complete mip/layer/face/Z transfer and real protected visibility are prepared.

Core/checks compile (`command-20261012-165209-62792.log`, 6.845 s;
`command-20261012-165209-102156.log`, 3.82 s), zero non-deprecation diagnostics,
and whitespace passes. Attachment compiles/links
(`command-20261012-165250-105424.log`, 0.442 s), not executed. Group 61 is partial;
actual numeric/capability/shader/GPU/CPU/fault/fence acceptance and integer/
SRGB/sub-DWORD/packed/MSAA/partial/pressure work remain open. Preview four
excludes these sources. No tests or games ran.

All ten integer fixture executables refreshed in
`work/small-integer-final-<fixture>-compile.log`, compile-only. The 64-group
inventory passed (`command-20261012-172408-46264.log`, 22.506 s), with 1,463
associations, 13 explicit mappings and zero semantic closures.

### Production after preview four: sub-DWORD image texels

Complete R8/RG8 UNORM/SNORM/UINT/SINT and R16 UNORM/SNORM/UINT/SINT/FLOAT
versions now retain the same independent buffer/image domains. Generic tiler
CAS writes preserve one/two-byte logical texels; complete image transfers keep
neighbors/padding. Linear copy buffers and budgeted allocation requirements
round to DWORDs, including native footprints ending in one/two-byte rows.
Matching formatted stride/records keep original short one/two-byte view limits.

The fixture adds 364 basic/312 divergent cases, totaling 1044 basic/888 divergent.
Dimensions derive from actual format bits so every complete chain is page-backed.
Warm writes and first-subresource image expectations use actual texel byte count,
preserving adjacent texels rather than overwriting a DWORD. Raw/CPU DWORD stores
still change four bytes. Negative normalized/integer, exact half, scaled reads,
full/short/long view and complete physical/padding/neighbor expectations remain
independent. Actual byte-view bounds/dirty/capability/CAS acceptance is unverified.

Core/checks compile (`command-20261012-172741-15388.log`, 7.602 s;
`command-20261012-172741-39372.log`, 4.893 s), zero non-deprecation diagnostics,
and whitespace passes. Attachment compiles/links
(`command-20261012-173035-82012.log`, 0.438 s), not executed. Group 65 is partial;
shifted/partial buffer writers, packed/SRGB/MSAA, ambiguity/concurrency/stages,
linear pressure and final shader/GPU/CPU/fault/fence acceptance remain open.
Immutable preview four excludes these changes. No tests or games ran.

All ten final half fixtures refreshed in
`work/half-versions-final-<fixture>-compile.log`, compile-only. The 61-group
inventory passed (`command-20261012-165548-58280.log`, 21.707 s), retaining
1,460 associations, 13 explicit mappings and zero semantic closures.

All ten sub-DWORD fixtures refreshed in
`work/subdword-image-final-<fixture>-compile.log`, compile-only. The 65-group
inventory passed (`command-20261012-173155-30232.log`, 21.942 s): 982 files,
13,358 functions, 12,186 dispatch entries, 1,463 associations, 13 explicit
mappings and zero semantic closures.

### Production after preview four: independent RGBA8 SRGB versions

SRGB numeric image views now retain independent image/buffer versions. Pinned
storage image views use UNORM while sampled views decode SRGB RGB. Native
storage/SRGB alias lowering remains in place; retention accepts the view without
changing those rules. Matching formatted buffers use UNORM because their numeric
field is only three bits, unlike the image's four-bit SRGB value nine.

The actual GCN fixture adds 28 basic/24 divergent cases, totaling 1072 basic/912
divergent. Image quarter/half stores keep UNORM bytes 64/128; independent sampled
reads expect red 13/55 and formatted-buffer reads 64/128. Warm expectations use
the piecewise SRGB decode formula. Full/short/long buffer views, protected CPU
encodings, all components and complete physical/padding/untouched subresources
remain in the oracle. Operation hash shape reserves noncolliding numeric/mip
fields. Actual shared-stage storage-read classification remains open.

Core/checks compile (`command-20261012-173935-102820.log`, 7.175 s;
`command-20261012-173935-20456.log`, 3.988 s), zero non-deprecation diagnostics
and whitespace passes. Attachment compiles/links
(`command-20261012-174141-99240.log`, 0.442 s), not executed. Group 66 is partial;
SRGB casts/decode/GPU/CPU/fault/fence acceptance, packed/MSAA/partial/shifted
writers, pressure and full migration remain open. Preview four is immutable.
No tests or games ran.

All ten SRGB fixtures refreshed in `work/srgb-image-final-<fixture>-compile.log`,
compile-only. The 66-group audit passed (`command-20261012-174654-24764.log`,
21.758 s), retaining 1,467 associations, 13 mappings and zero semantic closures.

### Production after preview four: independent packed 2:10:10:10 versions

UNORM/SNORM/UINT/SINT packed image/buffer versions now retain independent domains.
Unsigned typed and signed raw-storage lowering remain authoritative. Conversion
to a signed sampled expansion retains the exact guest packed backing and written
mask. Formatted synchronization merges only those written guest bytes over the
captured independent buffer version. It avoids re-encoding SNORM decoded floats,
which would collapse the distinct -512/-511 encodings, and preserves padding.

Current sampled descriptor legality is checked through its canonical writable
ownership descriptor. Expanded transfer requires the raw backing/mask; all GPU
allocations/descriptors remain leased through the actual fence. A 2D mask merge
stays within 256 X groups/1024 rows for the 64-MiB scope and uses existing UAV
barriers. Inherited backing is already accounted, while newly reserved output
and captured buffer bytes retain normal transient accounting. Other packed
representations continue through full native subresource retile.

The actual GCN fixture adds 119 basic/102 divergent cases, totaling 1191 basic/
1014 divergent. Quarter/half/one ten-bit encodings and independent warm RGB/alpha
packing cover normalized/integer conversion. An extra SNORM warm value keeps
RGB -512/alpha -2 bits through full image synchronization. Full/short/long views,
all mip/layer/face/Z versions and complete protected CPU/physical/padding contents
remain prepared. Actual GPU/casts/merge/fault/fence equivalence is unverified.

Core/checks compile (`command-20261012-175343-72452.log`, 13.99 s;
`command-20261012-175343-21884.log`, 17.01 s), no non-deprecation diagnostics;
attachment links (`command-20261012-175447-30884.log`, 0.455 s). Exact runtime
SM5.1 tiler shader compiles offline (`command-20261012-175407-67284.log`, 2.641 s),
including regenerated core source. Whitespace passes. Group 67 is partial; other
packed orders/16-bit/float/MSAA/partial/shifted/concurrent/pressure and full
acceptance remain open. Preview four excludes this work. No tests or games ran.

All ten packed fixtures refreshed in `work/packed10-image-final-<fixture>-compile.log`,
compile-only. The 67-group audit passed (`command-20261012-175557-41584.log`,
21.718 s), retaining 1,467 associations, 13 mappings and zero semantic closures.

### Production after preview four: independent packed16 and R11 float versions

Packed16 UNORM storage/sampling now retains independent domains for complete
2D/mip/array/cube ranges. Raw R32 writable storage, two-byte guest texels and
existing typed native format conversion keep original data-format/swizzle rules.
Packed16 volume conversion remains explicitly guarded.

The GCN fixture adds 80 basic/64 divergent cases across 5:6:5, 1:5:5:5, remapped
5:5:5:1 and 4:4:4:4. Independent component packing and red positions expect
quantized quarter 66/68 and half 132/136. Pinned formatted-buffer lowering does
not support packed16 formats; the fixture uses supported R16_UINT plus real GCN
LSHRREV/AND/CVT/MUL rather than introducing an unsupported original read format.
Warm/whole physical checks preserve two-byte neighbors and all allocation padding.
Core/checks compile (`command-20261012-180122-111508.log`, 7.109 s;
`command-20261012-180122-107672.log`, 3.945 s); attachment links
(`command-20261012-180136-104324.log`, 0.456 s), compile-only. Group 68 is partial.

Canonical R11G11B10 FLOAT independent versions now also accept raw 10:11:11 and
remapped 11:11:10 sharps. Existing typed UAV load/store capability checks remain;
four-byte generic retile and original descriptor remapping are reused. The
fixture adds 56 basic/48 divergent cases, bringing totals to 1327 basic/1126
divergent. Independent unsigned-float exponents encode warm zero/one/half and
negative-clamped-zero, and quarter/half/one stores in the actual low 11-bit or
high 10-bit red position. Matching formatted FLOAT loads, full/short/long views,
all mip/layer/face/Z versions and complete CPU/physical/padding stay prepared.
Core/checks compile (`command-20261012-180328-56916.log`, 7.007 s;
`command-20261012-180328-112488.log`, 3.896 s); attachment links
(`command-20261012-180341-107100.log`, 0.449 s). Non-deprecation diagnostics zero,
whitespace passes. Group 69 partial; actual typed support/casts/quantization/
conversion/retile/GPU/CPU/fault/fence, R9 shared exponent, old-device fallbacks,
MSAA/partial/shifted/concurrent/pressure and final acceptance remain open.
Preview four is immutable. No tests or games ran.

All ten current fixtures refreshed in `work/packed-formats-final-<fixture>-compile.log`,
compile-only. The 69-group audit passed (`command-20261012-180530-40752.log`,
21.89 s): 982 files, 13,358 functions, 12,186 dispatch entries, 1,470 associations,
13 explicit mappings and zero semantic closures. It still marks 765 functions
for mapping/adaptation, 210 changed functions for review and 567 dispatch entries
for review. Unchanged lexical source is not semantic acceptance.

### User-requested current snapshot five

Core154/groups 53–69 and prior work are packaged in the existing desktop catalogue
as `dx12-20261012-full-port-preview-8849f997245d`, display
`DX12 — перенос №5 — 12.10.2026`, SHA256
`8849f997245d940de270fafe4a81265bd07c59207671464bbae99a1fea6590c2`.
Packaging passed (`command-20261012-180651-36908.log`, 36.509 s), with 76 manifested
files, static PE imports/dependencies, exact dirty/untracked source, pinned native
compiler/CRT/licenses and matching PDB/MAP. Twenty save files were copied from
the still-selected preview-one isolated profile and hash-verified. Installed
desktop/settings/default selection and old packages stayed unchanged.
User deliverables are `outputs/DX12-preview-5-20261012`. No runtime or games ran.
Snapshot five is immutable; full migration continues beyond this source boundary.

### Production after preview five: logical partial buffer writes

GPU shader/copy writes now notify independent image identities by their original
logical sharp/copy destination address. This fixes the protected resident path,
which previously skipped preparation and could leave an exact-base old image
active. Native DWORD/page envelopes no longer select that invalidation base.
Shader notifications follow preflight and precede prewrite captures/preparation;
copy notifications follow all exact-region validation and precede coalescing.

Exact-base simultaneous writers now accept shorter/longer raw-buffer extents,
preserving complete prewrite image snapshots. A DWORD-shifted buffer-only writer
retains the previous image, as pinned InvalidateMemoryFromGPU only considers an
exact image base. Such retained-image writes require actual deferred buffer
ownership before execution, including when the image is not bound by this draw.
Logical retirement releases stale read-only image witnesses and rearms remaining
versions while actual GPU allocation leases survive. Sub-DWORD original sharps
and formatted concurrent writers remain explicitly guarded.

All 1327 basic/1126 divergent prepared cases now repeat full, shortest valid
DWORD-store and extended raw writer sharps: 3981 basic/3378 divergent. Existing
formatted read bounds stay independent. Every case also performs a real DWORD-
shifted write, protected CPU result read and old image-quarter observation,
followed by exact-base write/reload to zero. Whole physical/padding/tail/neighbor
and untouched subresource expectations cover partial commits and image retention.

Core/checks compile (`command-20261012-181459-99576.log`, 12.806 s;
`command-20261012-181459-49036.log`, 19.485 s); attachment links
(`command-20261012-181520-52856.log`, 0.454 s), no non-deprecation diagnostics,
whitespace passes. Group 70 is partial. Actual dirty ranges/residency/protection/
copy/shader/GPU/CPU/fault/fence, non-base simultaneous image writes, broader
non-independent ownership/DMA/pressure and full acceptance remain open. No tests
or games ran. Immutable preview five excludes this later production change.
All ten logical partial-write fixtures refreshed in
`work/logical-partial-final-<fixture>-compile.log`, compile-only. The 70-group
audit passed (`command-20261012-181739-86832.log`, 21.549 s), retaining 1,470
associations, 13 mappings and zero semantic closures.

### Production after preview five: mixed page ownership

PageWatch::TrackOwned retains exact existing ownership/mode-transfer fast paths
and acquires an aligned buffer range spanning read-only image witnesses and
writable gaps. Crossing old ranges split; outside protections/generations stay
unchanged. New gaps must be committed PAGE_READWRITE, retained pages must match
their recorded OS mode. Map nodes/vectors and all validation precede protection
changes. New owner pages become NOACCESS without exposing old protected pages;
transition rejection restores preceding watched modes. Replacement nodes publish
only after success. GuestBufferAccess uses the new ownership API.

Prepared page-watch cases cover interior splitting, unchanged outside RO pages,
lookup, RO conversion, a partial-release hole, whole mixed-gap reacquisition,
full permission restoration, original untracked read-only rejection and changed
watched-permission rejection without partial mutation. Full-core GCN partial/
shifted fixtures use this same production API. Fault/backing/owner lifetime
behavior remains unverified; no scenario has executed.

Core/checks compile (`command-20261012-182151-62812.log`, 7.697 s;
`command-20261012-182151-79908.log`, 4.345 s), including the page-watch executable;
attachment links (`command-20261012-182205-78304.log`, 0.451 s). Non-deprecation
diagnostics zero, whitespace passes. Group 71 partial; actual Windows rollback/
allocation failure/racing faults/generations/cross-owner lifetimes/remaps and
shader/GPU/CPU/fault/fence/pressure/full acceptance remain open. Preview five is
immutable and excludes this later source. No tests or games ran.
All ten mixed-page fixtures refreshed in `work/mixed-page-final-<fixture>-compile.log`,
compile-only. The 71-group audit passed (`command-20261012-182449-88864.log`,
21.674 s), retaining 1,470 associations, 13 mappings and zero semantic closures.

### Production after preview five: exact sub-DWORD write ranges

ShaderBufferRequest retains original write adjustment/length independently of
its DWORD native envelope. Validated WriteRange drives logical GPU invalidation,
preparation dirty marking and later bind-time re-marking. Native descriptor and
PushData alignment stay intact; unused aligned bytes cannot become CPU dirty.
Known copy spans retain their existing exact semantics. Original DownloadMemory
intersects page tracker results with exact gpu_modified_ranges, so widening these
physical commits to whole pages would also change the pinned behavior.

Every one of 3981 basic/3378 divergent prepared cases adds actual STORE_BYTE at
image+selectedOffset+1 with size1 and naturally aligned STORE_SHORT at +2/size2.
Real opcode values were checked against the pinned frontend. A preceding formatted
copy makes resident GPU quarter bytes differ from clean physical CPU zeros;
protected reads and complete physical comparison require exact 0x35/0x7a35 while
unused DWORD bytes stay in their CPU domain and the image remains quarter. Non-
base simultaneous image writers and original formatted offset guards remain.

Core/checks compile (`command-20261012-182936-44456.log`, 12.542 s;
`command-20261012-182936-30488.log`, 19.239 s); final attachment links
(`command-20261012-183301-104996.log`, 0.449 s), no non-deprecation diagnostics,
whitespace passes. Group 72 partial; actual byte/halfword DXIL lowering, dirty
bounds/neighbor/CPU/fault/fence, misaligned halfword original behavior, broader
CPU upload granularity/cache ownership/DMA/pressure and full acceptance remain
open. Preview five is immutable and excludes this later source. No tests or games ran.

All ten full-core fixtures were refreshed in compile-only mode after group 72.
The 72-group inventory passed (`command-20261012-183432-21896.log`, 21.824 s),
1,471 associations, 13 explicit mappings and zero semantic closures.

### Production after preview five: actual CPU stores over GPU-only formatted arenas

Readonly image copies/metadata synthesis now retain a conservative per-mapping
GPU/CPU divergence marker. CPU invalidation records real touched 4-KiB pages
before watch/proof early returns, independently of snapshot comparison holes.
Image-watch retirement cannot erase these actual-store markers. Preparation
uploads their complete CPU pages unconditionally before byte comparison and
clears them only after successful uploads; remaining GPU-only pages persist.
Exact logical GPU writeback spans are unchanged. Finish comments distinguish CPU
snapshot proof from equality of all GPU arena contents.

Byte-component prepared GCN cases first witness quarter through an actual large
raw BUFFER_LOAD_DWORD, byte mask and UINT conversion, then store volatile zero
over unchanged CPU zero and require the same raw read to show zero before any
image refresh. Raw reads cannot hide upload failures behind the small CPU stream
or formatted image synchronization. Refresh the cached image from CPU afterwards
before extended formatted zero checks. Group 73 remains partial:
same-value stores, page bounds, races/reentrancy, exception recovery, metadata
and wider remap/ownership/DMA/pressure behavior need final execution/review.
Core/checks compile (`command-20261012-184626-111628.log`, 12.922 s;
`command-20261012-184626-97696.log`, 19.41 s), zero non-deprecation diagnostics.
Final fixture compilation pending. No tests, application or games launched; preview five
excludes this later source and full implementation remains open.
Generic GuestBufferAccess now records dirty CPU pages only for actual write
faults. Its prepared production fixture fills four GPU-only pages, declares just
four GPU-written bytes, verifies a real CPU read reconciles those bytes without
forced uploads, then stores an unchanged byte on page two. Full GPU readback
requires exactly page two from CPU and all three untouched GPU pages unchanged;
the forced-upload counter must advance by exactly 4096. Latest compile pending.
Actual-store marker generations additionally retain notifications delivered by
physical-alias callbacks during Upload. Preparation revisits the marker set
before exposing its GPU view. The prepared reentrant case notifies a same-value
third-page store while uploading page two and requires both CPU pages with the
other two GPU-only pages intact. Current core/checks compile
(`command-20261012-185347-79404.log`, 12.688 s;
`command-20261012-185347-84384.log`, 19.5 s); final all-ten refresh/audit pending.
Final all-ten full-core fixtures compiled/linked against core160 (attachment link
`command-20261012-185539-72572.log`, 0.452 s), zero non-deprecation diagnostics,
whitespace passed. The 73-group inventory passed
(`command-20261012-185451-44760.log`, 21.904 s), 1,473 associations/13 mappings/
zero semantic closures. These are compile/static results; nothing executed.

### Production after preview five: zero clear follows original image domain

The old hash-specific 0x8b355b5a zero-vector physical CPU fill is removed.
Pinned IsComputeImageClear recognizes a compatible cached image before ordinary
buffer binding and clears that GPU image; without a compatible image, normal
GCN shader dispatch executes. Zero now uses existing native image candidate,
physical bits, clear-format and ownership validation instead of bypassing those
checks and clearing physical guest padding. Full independent image/buffer and
attachment ownership, actual recognition/fallback/dirty/fence/padding execution,
performance and full acceptance remain open. Group 74 partial, compile pending;
no test/application/game launched and immutable preview five excludes this change.
The compatible-image zero path additionally supports canonical writable UAV
images with a whole single-sample allocation, complete mip/layer selection and
no expanded guest format. Native raw/integer views use UINT zero, typed float/
normalized views FLOAT zero. Exact positive-zero bits avoid losing negative-zero
float encodings. Both descriptor heaps and the GPU image survive through retirement;
clear commands set image pending and apply a UAV barrier, without buffer writes.
Twelve prepared GCN two-buffer compute clear shapes carry the previously special
hash and a preceding independent R32 FLOAT image/buffer version. Sampled first/
next texels must become zero while warm physical buffer/padding bytes stay intact;
the next exact-base raw/image writer restores the regular quarter scenario.
Latest compilation pending; actual frontend recognition/GPU/storage/padding/
buffer/fault/fence execution and broader nonzero/sampled/padded/MSAA clears remain open.
Current core/checks compile (`command-20261012-190806-68764.log`, 7.23 s;
`command-20261012-190806-82856.log`, 4.117 s), zero non-deprecation diagnostics.
All ten full-core fixtures compile/link against this source; no execution.
Whitespace passes. Current inventory pending; immutable preview five excludes it.
The 74-group inventory passed (`command-20261012-191046-84596.log`, 20.269 s),
1,474 associations/13 mappings/zero semantic closures. Full-core attachment
fixture links (`command-20261012-190948-113212.log`, 0.447 s). No execution.
Preparing an immutable sixth snapshot from core162/groups 70–74 and prior source;
the existing installed desktop and selected previous version remain unchanged.
Sixth immutable snapshot is now installed as
`dx12-20261012-full-port-preview-0e76d14b1685`, display
`DX12 — перенос №6 — 12.10.2026`, core SHA256
`0e76d14b1685ad3afd1af17ab7b8e65064f167035be85ffb80a2e8b2277c414e`.
It contains core162/groups 70–74 and all prior production changes. The package
passed static hashes/imports/dependency manifest checks
(`command-20261012-191301-38156.log`, 36.232 s), 76 files with exact dirty/
untracked source, matching PDB/MAP, pinned runtime/dependency licenses/provenance.
Twenty saves copied from the still-selected first preview's isolated profile;
all copy hashes verified. Installed desktop/default selection/settings and old
packages remain unchanged. No application/test/game was launched. User outputs
are in `outputs/DX12-preview-6-20261012`; main implementation remains in progress.

### Production after preview six: complete image-clear resource ranges and padding

Pinned IsComputeImageClear clears all levels/layers of its matching resource,
independently of the bound view and logical-versus-physical guest padding. Native
RTV/positive-zero UAV compatibility no longer requires WholeGuestImage. Temporary
clear views reset first mip/layer and select all layers before creating each mip
descriptor, preserving stored image/view identity and existing family validation.
Single-sample zero storage clears retain complete 1D/3D extents. Copy coverage
restrictions remain unchanged; native image readback retile preserves padding.

Prepared padded RGBA8 RTV geometry is width17/pitch32/height9, two mips and three
layers, with the cached view restricted to mip1/layer1. Full GuestImageReadback
must show exact yellow for every active texel and unchanged 0xa6 padding. R32
FLOAT GCN zero-clear shapes expand to 84 warm/extent/mip/layer/cube/volume cases
with sampled first/next zero and separate physical buffer/padding expectations.
Group 75 partial; compile pending. Actual range descriptors, clear/retile/readback,
partial views, nonzero/integer/sRGB/packed/expanded formats, MSAA and broad
ownership/full acceptance remain unverified. Preview six is immutable and excludes
this later source; no tests, application or game launched.
Core compiles (`command-20261012-191708-29380.log`, 7.3 s). The first checks build
failed because this fixture's Require accepts source_location instead of a
message string; two assertions now use its actual signature. Final checks pass
(`command-20261012-191734-43504.log`, 3.115 s); attachment fixture links
(`command-20261012-191744-45036.log`, 0.456 s), compile-only. All-ten/audit pending.
All ten full-core fixtures refreshed compile/link only. The 75-group inventory
passed (`command-20261012-191833-27700.log`, 21.68 s), 1,475 associations/13 mappings/
zero semantic closures. No non-deprecation diagnostics or whitespace errors.

### Production after preview six: nonzero wide storage-image clears

Complete-resource R32/RG32/RGBA32 writable UAV images now accept nonzero clear
vectors. The shared storage clear lease/API retains complete mip/layer views,
both descriptor heaps and GPU resource. Integer views receive the clear float
vector's exact union bits, matching pinned VkClearValue; raw R32 float storage
also uses those bits, preserving signed zero. Typed float views use FLOAT clear.
Narrow/packed representations retain the positive-zero-only restriction.
Each of 84 prepared R32 FLOAT GCN clear shapes additionally clears to half,
requires first/next sampled red128 and unchanged physical buffer zero/warm bytes,
then restores the ordinary quarter state. Group 76 partial; compile pending.
Actual union-bit/float/sign/NaN/integer semantics, narrow/packed/expanded/non-UAV
promotion, view legality, original ownership/fence/buffer domains and final
acceptance remain unverified. Immutable preview six excludes this later source.
Large actual raw DWORD-to-float GCN reads additionally require resident GPU
buffer zero after both zero/half image clears, before formatted sync can copy the
image domain. Current core/checks compile
(`command-20261012-192225-73596.log`, 7.287 s;
`command-20261012-192225-8012.log`, 4.141 s); final fixture/all-ten/audit pending.
Prepared direct production clear/readback fixture covers 27 data-format/numeric/
dimension combinations (R32/RG32/RGBA32; UINT/SINT/FLOAT; 2D array/1D array/volume),
three mips and a restricted cached view. Exact 1.5/-0/-3.25/0.25 input union bits
and complete 0xa6 padding are required after GPU readback. Nine warm R32 cases
additionally clear to negative zero and require 0x80000000 at every texel.
Final compilation/refresh/audit pending; these cases have not executed.
Final checks compile (`command-20261012-192650-71056.log`, 3.186 s), attachment
fixture links (`command-20261012-192405-102084.log`, 0.476 s), all ten fixtures
refreshed compile/link only. The 76-group inventory passed
(`command-20261012-192728-73288.log`, 22.386 s), 1,475 associations/13 mappings/
zero semantic closures. No non-deprecation diagnostics/whitespace errors or execution.

### Production after preview six: nonzero native typed narrow image clears

Writable native typed storage views additionally accept nonzero image clears.
Integer views use input VkClearValue union bits; floating/normalized views use
native FLOAT clear and its format conversion. Packed/raw narrow representations
and sRGB retain positive-zero restrictions until exact packing/conversion is
adapted. Existing complete-view descriptors, image ownership, leases/barriers
and separate physical buffer domains remain unchanged.
Prepared 81 cases cover R8/RG8/RGBA8/R16/RG16/RGBA16 × supported UNORM/SNORM/
UINT/SINT/FLOAT numeric modes × arrays/1D/volumes, three mips and restricted cached
views. Exact chosen normalized/integer/sign/half-float/negative-zero texel bytes
and unchanged 0xa6 guest padding are required after full GPU readback.
Group 77 partial; compile pending. Device typed-UAV capability, native/original
conversion/rounding/clamp/narrowing equivalence, complete-resource descriptors,
retile/CPU/ownership/fence execution and broad acceptance remain unverified.
Preview six is immutable and excludes this later source; no test/app/game ran.

### Production after preview four: formatted buffer larger than image

Readonly formatted buffers at an independent image's exact base can now have
a larger physical extent. Candidate choice uses pinned unique-candidate or
exact original sharp-size preference separately from native DWORD alignment.
The unchanged shader view prepares its complete requested range; snapshot,
retile and replacement copy only the entire image payload, preserving the
buffer tail. Image version, buffer dirty ranges/CPU visibility, metadata
precedence and reflected native SRV/UAV state ordering remain distinct.

Each of 512 basic GCN cases adds a large-clean and GPU-modified larger sharp,
with at least twenty KiB and at least four KiB beyond the image. Large clean
views must consume the image even when its own size is below sixteen KiB;
physical CPU bytes remain separate. GPU-modified views must expose full image
component encodings after protected CPU read. Whole physical/padding oracles
include the unchanged tail. The 432 divergent-subresource cases remain.

Final core/checks compile (`command-20261012-165905-80156.log`, 7.045 s;
`command-20261012-165905-107136.log`, 5.372 s), no non-deprecation diagnostics,
and whitespace passes. Attachment compiles/links
(`command-20261012-170009-59700.log`, 0.444 s), not executed. Group 62 is partial:
shorter/non-base/ambiguous/concurrent aliases, actual candidate/shader/dirty/
GPU/CPU/fault/fence semantics and linear pressure remain open. Preview four
excludes this source. No tests or games ran.

All ten extended-view fixtures refreshed in
`work/formatted-extended-final-<fixture>-compile.log`, compile-only. The 62-group
inventory passed (`command-20261012-170150-66528.log`, 21.749 s), retaining
1,460 associations, 13 explicit mappings and zero semantic closures.

### Production after preview four: short formatted image views

Short readonly formatted sharps can synchronize a complete independent image
at the same base. A deduplicated readonly resident helper prepares every image
byte while the ordinary shader view keeps its original limits. Helpers append
after existing ordinary/DMA/graphics auxiliary requests, with matching native
SRV/UAV access, preserved binding indices and no guest-write marking. Full
image copy/retile then fits a proven valid prepared native arena, matching the
pinned original's complete image tiling when its arena fits beyond the sharp.

Each 512 basic GCN case adds small/large-clean and GPU-modified short views that
end after the selected texel. <=16-KiB clean views still stream CPU zero while
image quarter stays independent; larger clean views read image quarter. Before
CPU reconciliation short GPU-modified views transfer the whole image and expose
exact component encodings through real CPU faults. Full physical/padding
expectations cover bytes beyond the shader view; 432 divergent cases remain.

Core/checks compile (`command-20261012-170611-76348.log`, 6.801 s;
`command-20261012-170611-61248.log`, 5.3 s), zero non-deprecation diagnostics,
and whitespace passes. Attachment compiles/links
(`command-20261012-170749-62816.log`, 0.452 s), not executed. Group 63 is partial:
actual bounds/residency/streaming/dirty/shader/GPU/CPU/fault/fence and auxiliary
acceptance remain open. Non-base offsets retain the original assert/unsupported
scope; unmatched ambiguity and image-overlapping simultaneous writers stay
guarded. Preview four excludes these changes. No tests or games ran.

All ten short-view fixtures refreshed in
`work/formatted-short-final-<fixture>-compile.log`, compile-only. The 63-group
inventory passed (`command-20261012-170950-55584.log`, 21.802 s), retaining
1,460 associations, 13 explicit mappings and zero semantic closures.

### Production after preview four: small-component integer image versions

Complete RGBA8/RG16/RGBA16 UINT/SINT domains now retain the same independent
buffer/image storage. Existing typed integer views and formatted decoding keep
actual byte/halfword encodings; full transfer/padding/CPU ownership stay shared.
Integer image stores use actual integer literals. Prepared image/buffer pixel
shaders issue V_CVT_F32_U32/I32 then V_MUL_F32 reciprocal scaling, with decoder
fields/opcodes checked against frontend definitions. Shader hash shape includes
the numeric view to distinguish those changed instruction bodies.

The fixture adds 168 basic/144 divergent integer cases: 680 basic/576 divergent
with existing full/short/long buffer views, negative sign-extension warm values,
actual component CPU edits and independent complete physical/padding bytes.
Static review also corrected earlier image-load color exports: G/B are now
explicitly zero for the R-only color oracle, while full physical comparisons
still cover every guest channel. Earlier multi-channel warm G/B exports would
otherwise differ from the expected color. No runtime execution is claimed.

Core/checks compile (`command-20261012-171538-49380.log`, 6.86 s;
`command-20261012-171538-35628.log`, 3.844 s), zero non-deprecation diagnostics,
and whitespace passes. Attachment compiles/links
(`command-20261012-172136-48624.log`, 0.45 s), not executed. Group 64 partial:
actual cast/capability/shader/GPU/CPU/fault/fence acceptance and sub-DWORD,
SRGB/packed/MSAA/partial/concurrent/pressure work remain open. Preview four
excludes these sources. No tests or games ran.

### Production after preview three: depth/stencil shader planes and clears

ResourceBindings tracks known depth/stencil SRV planes separately. Native draw
validation rejects a sampled plane writable during the draw before recording;
the other plane may write using independent subresource states. Retaining a
resource for lifetime does not mark shader access. DSV flags follow actual draw
operations, independently of dormant guest view hints and beginning clears.

A read-only plane's beginning clear uses a separate writable DSV before shader
loads and attachment reads, then transitions into the actual combined read state.
That clear is not repeated in BeginRenderPass or OM. Guest ownership and selected
readback still include the cleared plane. Render-pass use now checks actual
Options18 RenderPassesValid alongside the existing tier, supplies matching SDK
read-only flags, and declares absent stencil NO_ACCESS. This follows the
[Microsoft render-pass specification](https://microsoft.github.io/DirectX-Specs/d3d/RenderPasses.html)
and [clear states](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-cleardepthstencilview).
The SDK places RenderPassesValid in Options18, not Options12.

The new `plane-feedback` check mode prepares twelve HLSL cases: depth load with
stencil increment or stencil load with depth write, three warm versions, with
and without beginning clear of the sampled plane. Independent complete guest
plane/padding and RGBA32F sample expectations check the post-clear version and
require competing sampled-plane writes to reject before recording. These
programs compile/link only; none execute and no runtime parity is claimed.

Core/checks compiled after plane tracking (8.09/13.384 s, logs
`command-20261012-142002-99652.log`, `command-20261012-142011-45260.log`).
All ten full-core fixtures were refreshed after the ResourceBindings layout
change in compile-only mode: task `work/shader-plane-final-<fixture>-compile.log`.
The later clear/render-pass changes compile in the core/checks (7.274/4.066 s,
`command-20261012-142645-17052.log`, `command-20261012-142645-22304.log`).
Non-deprecation warnings/errors are zero in these latest logs. Registry group
50 remains partial/review pending with zero semantic closures. Guest descriptor
equivalence, MSAA/mips/layers, pending HTILE and final GPU lifetime acceptance
remain open. Immutable preview three is unchanged; the full migration continues.

### Production after preview three: depth/stencil SRV and DSV view ranges

The native depth/stencil path now distinguishes actual sampled mip/layer/plane
ranges from the selected DSV mip/layers. Known 2D/array/MSAA/cube ranges receive
validated extents, including remaining mips. Unknown depth views/UAVs remain
conservative. Descriptor replacement removes its previous slot's range; retained
COM objects alone do not imply shader access. Guards reject only an intersecting
writable attachment range. Per-subresource barriers leave unrelated states
intact, and full uniform single-mip attachments retain global transitions.

The `depth-view-ranges` check prepares twelve cases over three warm versions:
same mip/different layers and same layer/different mips, a replaced descriptor,
exact complete guest contents and padding, sampled RGBA32F values, preservation
of unrelated states, and rejection after moving the DSV onto the sampled range.
Compile/link only. Combined stencil/MSAA/cube and actual guest descriptor
integration, lifetime and performance acceptance remain open.

Core/checks compiled (`command-20261012-143912-14692.log`, 9.642 s;
`command-20261012-143912-41276.log`, 13.565 s). A preceding build failed on one
extra parenthesis in the uniform-state predicate; named predicates fixed it.
All ten full-core fixtures were refreshed in compile-only mode, task
`work/depth-range-final-<fixture>-compile.log`. Registry audit with 51 groups
passed (`command-20261012-144019-41232.log`, 21.933 s): 982 files, 13,358
functions, 12,186 dispatch entries, 1,447 associated functions, 13 explicit
mappings and zero semantic closures. No runtime checks; preview three remains
unchanged. Continue all remaining implementation before final acceptance.

### Production after preview three: buffer writers and depth/stencil planes

Native Draw now captures complete prewrite depth/stencil versions when shader
buffer writes overlap a read-only attachment plane. Supported aligned depth
uses the existing GPU buffer snapshot/detile path; combined planes, MSAA and
other layouts use coherent CPU copies. Writer classification includes actual
depth/stencil operations, masks, explicit clears and rasterizer discard, rather
than treating an enabled stencil test as a write. HTILE registration preserves
the actual pending mask across snapshot replacement; a clear overlapping the
buffer writer is rejected before guest writes.

The other, nonoverlapping plane may write. Such draws acquire no whole-image
ownership; after fence retirement and buffer-owner reconciliation, readback
commits only selected writable planes. Unselected planes are neither read nor
overwritten by Resolve. Whole-image CPU equality proofs are cleared after a
partial commit, because the native read-only plane holds an older version.
Mixed DSVs transition actual D3D12 planes independently through the existing
subresource barrier machinery. Uniform planes keep the global fast path.
Graphics shaders with reflected UAVs use the existing OM/explicit-clear path
to retain ordinary store/atomic ordering without render-pass restrictions.

The actual full-core GCN fixture has four prepared modes:
`readonly-depth-buffer`, `mixed-depth-buffer`, and their `-owned` variants.
They cover Z16/Z32, one/four samples, optional stencil, either plane's buffer
writer, full/partial descriptors, both view hints, three warm CPU edits and
consumed HTILE. Vertex stores precede attachment reads; independent complete
allocation oracles include pass/fail colors, attachment writes, exact buffer
stores and padding. Target-plane writes/clears and a new pending HTILE
allocation overlapping a depth writer prepare rejection-before-write checks.
These are compiled/linkable preparations, not executed tests or parity proof.

Core compilation/linking passed (`command-20261012-133004-11536.log`, 4.867 s),
checks compiled (`command-20261012-133004-86296.log`, 3.765 s), and the updated
full-core GCN attachment fixture linked (`command-20261012-133021-56216.log`).
All ten full-core fixtures were then refreshed in compile-only mode; task logs
are `work/depth-plane-final-<fixture>-compile.log`. Non-deprecation diagnostics
in the current core/checks/fixture logs were zero; whitespace validation passed.
One fixture compile error used a guessed stencil reference member; the actual
`StencilRefMask::stencil_test_val` field fixed it. No compiler/ABI change.

Inventory passed (`command-20261012-133116-64140.log`, 22.140 s): 982 upstream
files, 13,358 functions, 12,186 dispatch entries, 1,446 functions associated
with 47 groups, 13 explicit mappings and zero semantic closures. Actual contents,
GPU ownership/pressure, device-specific early/late tests, mixed shader feedback,
same-plane simultaneous writers, UAV/attachment aliases and broader physical
aliases still require implementation/review and final acceptance. Partial-plane
readbacks currently record complete transfer storage before choosing commit
planes. All this source work postdates immutable preview three and is not
included in that installed package. Continue the full migration without tests
or an intermediate stopping boundary.

### Production after preview three: image/depth and binding lifetime

Production subsequently extended the same depth/stencil prewrite mechanism to
shader image UAV writers. Snapshot upload does not resolve a current owner
acquired after capture, and cannot overwrite a GPU-owned or pinned candidate.
Actual current shader writes therefore retain their ownership; mixed attachment
writeback still selects only its nonoverlapping written plane. Four additional
GCN modes (`readonly-depth-image`, `mixed-depth-image`, and `-owned` variants)
prepare real vertex R32 image stores, aligned 32x32 Z32 backing, 8x8 coverage,
optional stencil increments, warm edits, consumed HTILE, exact contents/padding
and competing-write rejection. These are compiled, not executed.

Binding leases now pin native cache identities across intermediate Finish and
later uploads. Budget trimming/eviction/invalidation cannot delete pinned
entries; actual placed texture/sample allocation leases accompany descriptors
and attachments. Early depth-feedback pointers also carry a pin. Counters use
independent shared storage so token destruction after device-loss/cache shutdown
cannot dereference a freed Entry. The existing exact GPU texture fixture prepares
post-fence invalidation rejection while either shared lease reference remains,
then acceptance after the last release. Actual pressure/shutdown acceptance is open.

Core compilation/linking passed (`command-20261012-135405-101220.log`, 9.172 s),
checks compiled (`command-20261012-135405-105384.log`, 15.274 s), and the extended
full-core attachment fixture linked (`command-20261012-135624-108632.log`).
All ten full-core fixtures were refreshed after Entry layout changes, in
compile-only mode (`work/binding-lifetime-final-<fixture>-compile.log`).
Non-deprecation diagnostics in current core/checks/fixture logs: zero; whitespace
validation passed. Inventory passed (`command-20261012-135826-84696.log`,
21.835 s): 49 groups, 1,447 associated functions, 13 explicit mappings and
zero semantic closures; upstream counts remain 982 / 13,358 / 12,186.
The original root stayed clean and installed preview three still hashes to
`8797e98ab8074a07b1675a06a0ed22a0fef06800e530a7511f592ccc951426c2`.
This additional implementation also postdates that package. Continue the full
migration without an intermediate test/commit/stopping boundary.

### Installed experimental preview three

The latest user-requested immutable experimental package is preview three,
`dx12-20261012-full-port-preview-8797e98ab807`, SHA256
`8797e98ab8074a07b1675a06a0ed22a0fef06800e530a7511f592ccc951426c2`.
It includes all production work through the GPU image snapshots and the
36 prepared anisotropy/cube cases. Integrated core compilation/linking passed
(`command-20261012-125459-99960.log`, 15.767 s), checks compiled
(`command-20261012-125857-100072.log`, 3.259 s), and all ten full-core fixtures
were refreshed in compile-only mode. The 46-group inventory passed
(`command-20261012-130126-70180.log`, 22.719 s), with zero semantic closures.
The subsequent user packaging build passed (`command-20261012-130544-74304.log`,
1.454 s). Packaging/static verification passed
(`command-20261012-131013-62324.log`, 37.642 s): 76 manifested files, 15 PE
import/delay tables, matching PDB/MAP and exact tracked/untracked source patch.
Twenty save files were copied from the currently selected first preview profile.
Settings, selection, installed desktop and previous selected package were checked
unchanged. The original stayed clean and no app/game/test was executed.
The package is in the existing desktop catalogue; user-facing instructions are
in this task's `outputs/DX12-preview-3-20261012`. It is an intermediate snapshot;
full implementation and final acceptance continue without a stopping boundary.

### Manual anisotropy and cube extensions

After its cube extension, core compilation/linking passed in 4.056 s
(`command-20261012-123909-102844.log`) and checks compiled in 3.350 s
(`command-20261012-123909-110700.log`). All ten full-core fixtures compiled and
linked only; GCN fixture link log `command-20261012-124016-72292.log`.
Static registry refresh passed in 20.262 s (`command-20261012-124309-100236.log`):
982 source/build files, 13,358 functions, 12,186 dispatch entries, 1,441 functions
associated with contracts, 13 explicit mappings and zero semantic closures.
The pinned original remains clean and immutable preview two's executable hash
still matches `7e7092ff4b78f03ffc20d9b6e292aec2cfc8e03f8f8915daa6d0d14e238380ad`.

Manual shared-storage sampling now accepts AnisoPoint/AnisoLinear ratios 2/4/8/16.
It computes texel-space major/minor derivative lengths, selects LOD from the
minor footprint bounded by the maximum ratio, and filters evenly spaced taps
along the major normalized gradient. Each tap retains mip interpolation, address
modes, border handling, comparison and nonzero-weight reduction. Zero footprints
use one centered tap; a zero minor derivative uses the maximum ratio. Explicit
LOD/LZ remains isotropic and now includes the previously omitted sampler bias.
No metadata ABI change or additional resource binding is introduced.

The implementation follows the approximate line scheme described in the primary
[Khronos texture chapter](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/textures.adoc).
Hardware-specific anisotropic taps and rounding are implementation dependent;
matching a particular Vulkan GPU still requires differential acceptance. Explicit
LOD isotropy and sampler-bias behavior are also specified by
[Microsoft sample_l](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sample-l--sm4---asm-).
No bit identity, accepted speed or completed sampling parity is claimed.

Twenty-four prepared GCN cases cover point/linear, all four limits, both axes,
zero/degenerate/negative/skew gradients, a three-tap ratio, mip interpolation,
LOD clamps, unnormalized/wrap/mirror, reduction and explicit LOD/LZ bias. Their
independent double-precision CPU oracle tolerates 1e-5 only for output samples;
all other allocation/padding bytes are compared exactly. Sampler-limit change
and restoration also prepare cache checks. They were compiled, never executed.

Core compilation/linking passed in 3.709 s, log
`F:\Codex-Shared\devtools\logs\command-20261012-122228-82564.log`.
Checks compiled in 3.271 s (`command-20261012-122501-94596.log`), and the updated
full-core GCN fixture compiled/linked (`command-20261012-122507-61128.log`).
Native bias caps, extreme/nonfinite gradients, comparison/array/1D/volume cases,
integer derivative anisotropy, static inactive-tap compiler cost and broader
graphics/resource adaptations remain open. Immutable preview two excludes this
later implementation. The main task continues without an intermediate stopping
boundary. Registry now has 45 groups; no semantic closure or runtime verification.

This implementation was subsequently extended to read-only anisotropy when
point XY or non-linear mip filters cannot use D3D12's fixed all-linear anisotropic
filter. All-linear read-only sampling retains the hardware path. Shader
specialization now includes newly manual sampler states even when the cached
stage was originally hardware sampled, and preserves full raw values for
already-manual stages. Shared bias constants are statically asserted against
the D3D12 SDK and applied before sampler/instruction bias is added to LOD.
Cube sampling also retains the original `FixGuestImageCoords` 2D-array lowering:
XY minus one, face minus `2*floor(face/8)`, followed by view-relative layer
clamping without inter-face filtering. No new cube resource representation is
invented. There are now 36 prepared GCN cases, including separate read-only
destinations, hardware/manual cache transitions, extreme bias cancellation and
seven cube/shared/read-only/multiple-cube/edge/mip cases. These remain unexecuted.

Before the cube extension the core compiled/linked in 8.628 s
(`command-20261012-123333-51344.log`), checks compiled in 9.314 s
(`command-20261012-123333-55204.log`) and the expanded GCN fixture linked
(`command-20261012-123424-69832.log`). No non-deprecation compiler diagnostics
were emitted. The cube extension requires the next incremental builds. Bias
handling is implemented for native API bounds; actual Vulkan device-cap
correspondence and all differential GPU acceptance remain open.

## Later production: GPU buffer/image snapshots, 2026-10-12

For a writable buffer overlapping a sampled image, dword-aligned supported
layouts now copy the resident buffer before current writes into an immutable
default-heap snapshot. The existing GPU detiler consumes that resource directly;
no CPU image-byte snapshot, comparison or guest-memory watch is inferred. Source
placed allocations and copy/tiler resources are retained until their actual users
retire. Unique captures share an address/size key and are limited to 64 MiB per
preparation batch; decoded formats, odd-byte tails, stencil, MSAA and larger
captures retain the coherent CPU path. Two writers still fail explicitly.

New texture entries include rounded snapshot width in their transfer reservation.
Transfer input growth on reused entries now uses the same retired-pool/last-use
eviction policy while preserving the entry being uploaded. GPU-to-CPU transitions
replace a default input heap with an upload heap after previous users retire.
The two alias fixtures prepare GPU input assertions, no CPU image snapshot,
GPU-to-CPU sampling through a separate output buffer, exact allocation/padding,
a CPU edit and return to GPU capture. They remain unexecuted. Global transient
peak memory, guarded aliases, wider formats, actual contents/lifetimes/GPU debug
acceptance and performance remain open.

Core compilation/linking passed in 15.767 s
(`command-20261012-125459-99960.log`). Checks initially failed because a fixture
used a nonexistent `GuestTextureCache::Find`; the actual entry inventory is now
used instead. Failure log is preserved (`command-20261012-125459-69736.log`).
The corrected checks compiled in 7.062 s (`command-20261012-125621-104124.log`),
and the additional transition oracle compiled in 3.259 s
(`command-20261012-125857-100072.log`). No test/application/game was launched.
Registry now has 46 groups, zero semantic closures. These sources postdate
immutable preview two; main implementation and acceptance boundaries remain open.

## Integrated so far

- Updated common kernel/thread/file-system/input changes and the original NP,
  signaling, network and camera code. Removed the obsolete second socket/NP
  implementation rather than keeping duplicate HLE registrations.
- Migrated the original network settings and callsites into the existing settings
  system. Backend-specific settings still need a complete semantic review.
- Added the original indirect-count PM4 packet and connected indirect shader
  specialization, plus the original GCN reciprocal/log clamp changes.
- Retained the native memory/coherence, covering-buffer envelopes, descriptor and
  allocation lifetime mechanisms.
- Restored the exact shared AMD tiling helpers and original image-layout tables
  under `video_core/guest_image_layout.h`. Native layout handles all defined tile
  modes, thick/volume slice padding and micro/macro mip transitions for currently
  supported formats. CB/DB attachments retain register-declared physical slices.
- Added CPU and native HLSL counterparts of the original tiling addressing.
  Tiled uploads now use GPU detile. Readback uses GPU retile, a compact byte bitmap
  and transactional CPU commit; it requires the full logical-byte count and
  preserves allocation padding. Transfer resources, descriptors, root signature
  and PSO are held by leases until the recorded users retire. Existing linear
  uploads retain the copy-queue path; compute/copy dependencies for tiling remain
  in the direct queue's actual fence domain.
- Removed the experimental partial-render switch and its draw/dispatch bypasses.
- Fixed the zero-argument fault logger's format-security warnings without
  suppressing diagnostics.

The new transfers are compiled, not yet proven by contents or GPU validation.
Do not label these changes a completed port or verified optimization.

## Build evidence

The existing `build-migration-core` tree, LLVM 22.1.3 clang-cl, VS 18 environment
and Release configuration are retained. The accurate compilation database is
exported there. The external source directory now points to the exact pinned
checkout's `externals`, replacing the prior dependency directory.

Integrated Release compilation/linking with pinned dependencies passed in
129.353 seconds. Raw log:
`F:\Codex-Shared\devtools\logs\command-20261012-014445-35280.log`.

The production tiling HLSL also compiled with `/Ges /O3 /WX`, without warnings;
raw compiler log:
`F:\Codex-Shared\devtools\logs\command-20261012-013858-106904.log`.
The core captures this same HLSL through its CMake configure dependency.

The MSAA upload/extraction path is now integrated into this same cache. It uses
sample-frequency typed pixel uploads and typed Texture2DMSArray.Load extraction,
including separate depth/stencil transfer. Integer values stay integer; sRGB
physical bytes use UNORM transfer views of typeless storage. SRV/RTV/DSV dimensions
and single-layer array views follow the actual MSAA resource shape. Ordinary
floating/normalized resolves use native average resolve, and integer resolves
transfer sample zero. Mixed attachment sample conversion, writable MSAA images,
compressed metadata and multisample destination backing variants remain open.

Transfer preparation is split from recording: readback allocates every resource,
descriptor, shader program and tiler lease before commands reference them.
Submission leases retain placed-allocation ownership as well as COM resources.
Cache budgeting now accounts actual D3D12 allocation size, detile pools and a
retile/readback reservation; retired staging can be trimmed without dropping the
authoritative image or CPU watch proof.

FP64 and pixel shader stencil export now use actual D3D12 capability bits, rather
than an unconditional native-profile disable. Sampled component mapping uses
the shared Image.DstSelect remapping of packed guest formats.

Pinned CPU patches, the module loader's static CPU patch branch, AMD reciprocal
tables and the MP3 header/frame decoder are now retained verbatim. The tables
are embedded by CMake and the CPU target links the existing zstd dependency.

Latest incremental Release build passed in 4.616 seconds; raw log:
`F:\Codex-Shared\devtools\logs\command-20261012-021035-29536.log`.
All eight production MSAA shader variants compiled with `/Ges /O3 /WX`.
The large rebuild after integrating CPU resource dependencies passed in 59.092
seconds (`command-20261012-020646-92432.log`). A const RECT mismatch in the native
resolve API caused an intermediate compile failure; it was corrected and the
affected target rebuilt successfully. These are compilation results only.

## Registry and next work

`tools/porting/inventory.py` derives file/function/dispatch rows from both actual
trees and verifies the original revision. `tools/porting/contracts.json` supplies
reviewable group contracts. Syntactic equality and compilation are explicitly
separate from final semantic verification. Relocated verbatim helpers are mapped
by signature and source hash. Shader helpers are included as source functions;
parser errors remain visible and require review rather than silent exclusion.

Before adding the shader-helper inventory, the refreshed scan contained 982
original files, 13,362 functions and 12,186 dispatch rows: 11,291 same-source
unverified functions, 156 changed functions and 1,915 requiring mapping/adaptation.
These numbers are structural counts, not semantic coverage. Closed final
semantic contracts remain zero.

Open work includes full function/dispatch/dependency contract reconciliation,
mixed-sample and writable MSAA adaptation, compressed metadata, non-DXGI format
conversions and BC volumes, remaining buffer/image aliases, graphics and shader capability/lowering
gaps, host passes/output/cache warmup and package/catalogue metadata. The original
subgroup-clock path versus the native profile also needs review. Original
on-chip geometry and stream output are themselves unfinished; classify the actual
upstream behavior instead of claiming a completed implementation.

Once all implementation contracts are closed: build the whole core; run the full
suite, contents/semantic/validation regressions; fix failures; then perform the
authorized pinned Vulkan reference and final DX12 gameplay acceptance through
the existing desktop launch path. Preserve old immutable core packages, isolated
saves/profiles and neutral release of bounded XInput commands. Only after exact
binary/source/runtime identity and acceptance may a version be called the full
port and verified commits be pushed normally to the configured upstream.

## Hardware shader clock integration in progress

Native `S_MEMTIME` emits the original `OpReadClockKHR` only when both the device
and loaded converter support the actual hardware clock path. The native path on
an unsupported profile reports an explicit error instead of using the original
constant fallback. Intel/other-adapter clock implementations remain open; the
AMD path is described below and still needs runtime acceptance.

A separate Mesa worktree at the pinned recursive-submodule revision
`b628375fb1fd99476bfbaaa0532deb6fe3a6870e` retains the existing linked-stage,
barycentric and native compiler adaptations and adds NIR-to-DXIL clock lowering.
The driver extension uses `NvGetSpecial` global-timer low/high operations, with
high/low/high reconstruction and rollover retry. The real system NVAPI library
is probed for opcode support. Applicable PSO creation uses local-thread extension
state and resets it after creation and on exceptions. The named counter-UAV
binding is reflected explicitly and distinct from ordinary guest descriptors.
PSO cache identity includes extension mode and the system NVAPI binary digest.

The complete compiler patch, linked ABI header, source revision, ABI-reference
revision and build options are retained under `native-port/vendor/mesa-*`.
The main core now uses headers from the pinned recursive dependency and the
vendored linked header instead of relying on an unrelated old source overlay.
The old Mesa checkout/build and installed core packages were not modified.

The separate compiler built with MSVC 19.51.36260, matching the existing ABI;
raw successful incremental log: `command-20261012-022924-74468.log`.
The first new core clock integration compiled successfully in 8.360 seconds
(`command-20261012-022722-85108.log`). A later rebuild includes the explicit
driver-binding classification and pinned-header paths; record its outcome
separately. No GPU execution or clock semantic tests have run. Package/runtime
closure and full-port acceptance remain pending.

Clock-source requirements were checked against the primary
[SPV_KHR_shader_clock specification](https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_shader_clock.html).
It permits variable tick rates and requires the same clock source within scope;
this does not establish runtime equivalence without the final GPU acceptance.

The pinned-header and explicit binding rebuild passed in 57.005 seconds
(`command-20261012-023006-97512.log`). The refreshed structural registry contains
982 files, 13,358 functions and 12,186 dispatch rows: 11,316 same-source
unverified, 149 changed and 1,893 needing mapping/adaptation. There are 142
contract-associated functions and zero closed semantic contracts.

## Alternate sample backings integrated, verification pending

The original mixed-sample path changes color backing samples to match depth, or
to one sample when color counts differ without depth. The native cache now
retains alternate resources without introducing a second guest image cache.
The active backing is copied through a pixel shader before switching. The
pinned `ms_image_blit.frag` uses `gl_SampleID` for an MSAA source, not an
unconditional sample-zero fetch; the new HLSL follows the same per-sample input.
A single-sample source is broadcast. Integer values retain integer types.

All layer copy leases are prepared and retained before commands are recorded.
Source/destination resources, placed allocation leases, immutable descriptors,
root signature and PSO stay alive through the existing direct-queue fence.
Shader access and readback restore the declared guest sample count. Native
resolve uses the active source count and a single-sample destination backing,
including when the guest destination declares MSAA. GPU image-copy shortcuts
restore guest samples before copying compatible images.

Backing allocations contribute their actual D3D12 allocation sizes to the
existing resident accounting. Pressure can trim retired inactive placed
backings whose allocation has no other lease while preserving active authority.
The core compiled in 8.141 seconds (`command-20261012-023735-28400.log`), and all
six float/uint/sint × single/MS HLSL copy variants compiled with strict FXC
warnings-as-errors (`command-20261012-023851-*.log`). This is compilation only.
Out-of-range sample behavior, actual contents, failure/reuse/pressure and
driver validation require the deferred final suite. Writable MSAA and compressed
metadata remain open.

## AMD clock device/compiler integration, acceptance pending

AGS SDK `086c47ed8e427199510887230718cd7c086f9702` supplies real ShaderClock and
ShaderRealtimeClock intrinsics. The native factory creates AMD devices through
AGS when building a backend and retains the AGS context, module and original
device reference until the backend's native children and owned device reference
retire. Cleanup failures and remaining device references are reported as
lifetime errors. SDK header/runtime and the compiled vendored header are checked
by SHA-256. The private build's AGS DLL is copied beside the core; installed
immutable packages and desktop defaults are unchanged.

The compiler adds the SDK's two-phase compare/exchange encoding at its driver
reserved UAV. Subgroup scope selects ShaderClock and wider scope selects
ShaderRealtimeClock. Native compilation selects NVAPI or AGS through a new
thread-local selector export, restores it on every exit, and preserves the
original runtime-conf ABI. DXIL cache identity includes the selection; relevant
PSO cache identity includes the runtime digest. Named AMD extension bindings are
classified separately from guest resources and require the actual initialized
device support before PSO creation.

Mesa rebuilt successfully in 5.689 seconds
(`command-20261012-024600-96176.log`). The integrated core and pinned AGS dependency
compiled in 74.052 seconds (`command-20261012-024920-66000.log`). Backing-pressure
accounting compiled in 4.449 seconds (`command-20261012-024054-52236.log`). These
remain compilation results, with no GPU or device-teardown verification.
Primary API/source reference:
[AMD AGS SDK at the pinned revision](https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/tree/086c47ed8e427199510887230718cd7c086f9702).

## Private runtime/compiler integration and writable MSAA

The core and standalone build now build Mesa from the exact recursive
`b628375fb1fd99476bfbaaa0532deb6fe3a6870e` source plus the checked vendored patch,
using MSVC 19.51.36260 and the existing MD ABI. Patch and linked-header hashes
are checked before use; tracked changes outside the previous generated contract
are rejected. A disconnected ExternalProject update exposed concurrent patch
steps in the standalone build. The always-run source check now follows configure,
and patch application has a process lock. Old compiler sources/builds remain
unchanged.

The core loads `exe/native-compiler` by default. CMake copies the newly built
converter, pinned DXC/DXIL and AGS runtime every build, with their licenses.
`vendor/dxc-native.json` fixes the compiler, validator, executable and API header
hashes for DXC v1.8.2403.2. Immutable package/catalogue integration is still open.

The original spdlog dependency fetches fmt 12.1.0 with its pinned tarball hash
before the otherwise unused fmt 12.0.0 submodule is added. The actual core
provider is `build-migration-core/_deps/fmt-src`; standalone fixtures now use
those headers, retaining the original dependency workflow and fmt ABI.

Writable multisample shader images now lower to one single-sample UAV texture
per guest sample, retaining array layers. Prepared storage leases extract every
MSAA sample before guest execution, retain all resources/descriptors/allocations,
and restore each sample afterwards in the existing direct-queue fence domain.
Sample selection uses the guest sample count; atomics retain upstream sample-zero
semantics. Transfer footprints align each sample to 512 bytes. Byte-compatible
color families transfer through UINT views to retain float/normalized raw bits.
Packed-float/depth exceptional payloads, attachment/storage aliases, unsupported
adapter formats/counts and all sample/lifetime acceptance remain open.

Compilation records: runtime integration 54.715 s
(`command-20261012-025523-17120.log`); writable MSAA 33.657 s
(`command-20261012-030450-90904.log`); raw transfers/loader 13.646 s
(`command-20261012-030724-89596.log`). These are not contents tests.

## FSR1, post-process and raw VideoOut views

Both original FFX headers are retained verbatim. EASU and RCAS use the original
half-precision implementation, constants, gather/load callbacks, HDR processing
and attenuation. They compile offline with the pinned DXC, shader model 6.2,
native 16-bit operations, HLSL 2018 and warnings-as-errors. HLSL 2018 is required
by the original header's vector conditional syntax. Actual device support is
queried before activation.

Each presentation slot retains a lease for input, intermediate/output images,
descriptor heap, root signature and both PSOs until its fence retires. FSR
settings reach the real GuestPresenter call path. The final display shader
follows the pinned post-process gamma/sRGB equation and HDR passthrough; the
old invented contrast sharpening/PQ conversion was removed.

VideoOut preserves raw 8/10/16-bit channels into a matching typeless resource
and typed SRV. Component mapping performs the required channel permutation and
alpha-one view, with 10-bit sRGB degamma in post-process. The old CPU float/10-bit
conversion to RGBA8 was removed. FP16 channel ABI, source/HDR behavior, precision
and actual GPU contents still need acceptance.

The native presentation viewport follows the pinned FitImage rounding and
centering, with black borders. FSR output uses the fitted image extent. Client
resize drains the presentation queue, releases retired backbuffer/FSR leases,
recreates buffers and updates displayed output dimensions. Failed ResizeBuffers
reacquires the existing buffers before reporting the failure. GUI display
integration, splash, screenshots and the remaining presenter state paths are
still open.

Integrated FSR/VideoOut compiled in 8.526 s
(`command-20261012-031652-83384.log`). Initial HLSL-standard and component-mapping
type errors were fixed; their failed build logs remain in task work. Resize
compiled in 10.329 s (`command-20261012-033320-106252.log`), and failure recovery
in 4.204 s (`command-20261012-033553-49732.log`). No GPU tests were run.

## Metadata owner identity and attachment slice views

CMASK/FMASK/HTile registration is in the existing native image cache, using a
monotonic image UID. Lookup checks a live owner and matching metadata address.
Rebinding the same owner preserves clear masks; replacing, evicting or unmapping
an owner removes stale registration. The obsolete independent HTile runtime
map was removed. Depth metadata registration respects `tile_surface_enable`;
compatible depth recreation from a cached color image starts with the original
no-clear mask.

Attachment slice views now share their physical cache entry and create explicit
RTV/DSV ranges, preventing a fresh owner/clear state on each view change. CMASK
retains physical partial-clear handling: uniform and mixed 0/F codes for
uncompressed MSAA materialize every sample and preserve other texels/padding.
Compute clear recognition keeps the pinned metadata-reader/XOR heuristic.
CMASK writers execute to preserve physical bytes; HTile/FMASK retain the
original tracked-clear behavior. Compressed CB/FMASK/DCC and additional CMASK
addressing remain open; no raw FMASK decoder is claimed. The pinned original
FMASK operation invokes fast-clear elimination and retains its common shader
identity substitution.

Metadata compiled in 10.806 s (`command-20261012-032523-63272.log`); explicit
attachment ranges/ownership compiled in 10.333 s
(`command-20261012-033041-106388.log`). Existing metadata/depth fixtures now
exercise the real cache. Prepared regressions cover owner retirement/reuse,
rebind state, partial and uniform 2/4/8/16-sample CMASK, exact unchanged bytes,
aspect fitting and resize-back. They have not been executed.

The standalone Release tree is reconfigured against the exact original recursive
dependencies and native core fmt provider. The obsolete epoll fixture now calls
the original HostEpoll implementation. Its timeout, persistent wake/drain and
socket-readiness checks are retained. Missing pixel-format and fixture diagnostic
link dependencies were connected. All standalone targets compiled/linked in
4.433 s (`command-20261012-033022-96388.log`) after the initial failed builds;
updated metadata/presenter fixtures compiled in the subsequent build. No CTest,
fixture executable, game or benchmark has run.

The latest structural scan contains 982 original files, 13,358 functions and
12,186 dispatch rows: 12,311 same-source unverified, 150 changed and 897 needing
mapping/adaptation. There are 1,163 contract-associated functions and zero
closed semantic contracts. The lower unmatched count includes the verbatim FFX
headers; it is not proof of completed runtime coverage. The full implementation,
registry and immutable package boundary remains open. Continue implementation;
do not start final testing from these compilation results.

## Screenshot runtime connected, acceptance pending

Original filename generation, conversion, HDR/PQ/Rec.2020 tone mapping, PNG
writing, duplicate-request handling and local notifications are now a common
`video_core/screenshot` implementation. Their pure helper bodies are retained
from the pinned presenter, with API-neutral format names and native log class.

The actual request counters reach GuestPresenter. Game-only requests retain
the source PixelFormat/PQ flag and copy the raw guest snapshot at its original
extent before FSR or gamma/post-process, matching the pinned source. With-overlays
requests copy the actual native output after UI drawing. The generic presenter
also exposes a fitted-output capture for semantic fixtures.

Readback footprints, staging buffers, callbacks and retention capacity are
prepared before recording. Region bounds are checked before copy commands.
Captured buffers/callbacks remain retained until the presentation queue fence
completes; subsequent frames and Drain collect retired captures. PNG IO does
not introduce a wait after normal frames. Source-ready versus prepare/present
timing, IO failure/retry, HDR bytes, file/notification behavior and all GPU
contents/lifetime acceptance remain open.

The core compiled in 9.971 s (`command-20261012-034310-52336.log`) after correcting
the API-neutral enum names. Screenshot fixtures compiled/linked in 6.411 s
(`command-20261012-034407-40832.log`). They cover raw-source, fitted-output and
overlay contents and deferred callbacks, and have not run. The final CMASK
fixtures previously compiled in 2.340 s (`command-20261012-033819-99984.log`).

The immutable original checkout is still clean. Diff whitespace checks pass;
Git emits only the repository's expected LF-to-CRLF conversion notices. No
tests, game, benchmark, package installation, commit or push has been performed.

## RGB96 and native ImGui integration, acceptance pending

RGB32 Uint/Sint/Float uses the existing image cache with typeless RGBA32 native
storage and a 12-byte physical guest layout. GPU detile expands the missing
alpha; retile writes only RGB through the existing written-byte mask. MSAA
transfer/footprint sizing uses 16-byte host texels and raw UINT views. SRV
mapping and UAV read lowering restore typed alpha one, and RGB destination-alpha
blend factors use the missing channel's original value. CMASK wide clear storage
also avoids reading past the two-word source for 96-bit pixels.

The original RGB96 tiler uses scalar uvec3 arrays, divides the complete tiled
address by 12, and omits RGB96 low address bits in display/thick microtiles.
Native display/thick formulas preserve those original cases. Reconciliation
of macro address divisibility and scalar buffer indexing is still open; this
implementation is not yet a semantic coverage claim. Packed 16-bit formats,
format aliases, exceptional values and all GPU acceptance remain open.

RGB96 initial upload/readback, NaN/sign bits, typed writes, mip/layer/padding,
linear storage, absent-alpha swizzles and blend fixtures have been prepared.
They have not run. The transfer shader compiled offline with pinned SDK FXC
cs_5_1, strictness, optimization and warnings-as-errors. Core RGB96 builds
passed in 9.952 s (`command-20261012-035334-102884.log`) and 3.856 s
(`command-20261012-035430-3132.log`). All standalone targets compiled in 9.981 s
(`command-20261012-035444-84944.log`). A new fixture's enum/bitfield compile
error was fixed; its subsequent build passed in 2.694 s
(`command-20261012-035638-51344.log`).

The game now draws inside the original `Display##game_display` dock item at
the ImGui command-list position, with native scissor clipping, layer ordering,
black background and ELF splash selection through the existing PNG reference
path. Its callback restores the UI descriptor heap and render state. FSR and
debug output resolution use the fitted game region. The native overlay API is
still available for standalone fixtures. Raw game screenshots retain their
source stage; with-overlays screenshots follow full composition.

The old PNG retirement assumption that every presentation drains the queue was
false for ungated native presentation. PNG resources/descriptors/Inner now remain
retained through the actual last UI submission fence. ImGui's own three
vertex/index render slots wait their actual submission values independently
of DXGI backbuffer indices. Shutdown waits the last UI submission, joins PNG
decode work, retires pending images and destroys all native image objects before
backend teardown. Remaining CPU references expose a cleared image ID and can
be released after renderer shutdown. New decode requests require a running
worker. Arbitrary added 32 MiB/8192/16-Mpixel PNG restrictions were removed in
favor of representable STBI inputs and native texture extents.

UI device objects rebuild for the actual SDR/HDR RTV format after retirement.
Presenter changes its format/HDR fields only after buffer and color-space
creation succeeds, with old buffers/color space restored after failure.
Prepared compositor fixtures cover clipped game pixels, layer order, hidden
game content, repeated-frame metadata and actual increasing submission fences.
GUI post-process prepare/present timing, fractional DPI placement, widget/cache/
network UI behavior, error paths and final contents/lifetime verification remain
open. No tests or gameplay were run.

Core UI retirement compiled in 10.918 s (`command-20261012-040004-76344.log`),
composition in 10.578 s (`command-20261012-040328-110528.log`), format handling
in 10.149 s (`command-20261012-040511-61328.log`), and image shutdown in 4.363 s
(`command-20261012-040633-55864.log`). All standalone targets with compositor
fixtures compiled/linked in 5.942 s (`command-20261012-040753-103620.log`).
These are compilation results only; the implementation/registry/runtime/package
boundary remains open. Continue implementation before final testing.

## Native Big Picture, pipeline preload and present modes — 2026-10-12

The original Big Picture sources and CLI launch flags are integrated into the
existing native core. SDL's renderer is explicitly Direct3D 12. The unused Vulkan
platform initialization/surface path is removed. The shared PNG manager selects
the real SDL upload/destruction provider while this context is active; it joins
decode work and clears GPU texture IDs before backend destruction. Game icon
textures retire before the SDL renderer, and the original existing emulator
handles both same-process and relaunched games. Native HDR/FSR/RCAS, present-mode
and pipeline-cache settings are exposed again. Vulkan JSON/TOML cache settings
migrate into the native settings group without loading a Vulkan renderer.

Pipeline storage now has native directory/archive implementations of the
original Save/Load/ForEachBlob/Open/FinishPreload/Close contracts. Native storage
uses a separate per-game namespace and checksum envelopes. Its IO worker drains
accepted requests before close/quick-exit and archive finalization. Native
reflection, aliases, specialization, SRT walker bytes, permutation indices,
SPIR-V binaries and actual compute/graphics fixed state persist through separate
blobs. WarmUp loads metadata/binaries and creates actual linked DXIL and PSOs;
runtime lookup reuses the same stable Info objects and sparse permutation
indices. Executable/compiler/profile identity rejects incompatible metadata.
The original raw InfoPersistent serialization cannot be reused because the
native structure contains a dynamic image-write list; native serialization
records its fields instead. Deserialization checks byte extents, container
capacities and conflicting permutation indices.

Native presentation now uses the requested mode: Fifo interval 1, Mailbox flip
queue interval 0 without tearing, Immediate interval 0 with tearing when DXGI
supports it, otherwise Fifo. Unknown modes retain the original Mailbox default.
DXGI interval/tearing behavior follows the primary Microsoft Present and
variable-refresh-rate documentation linked in the contract group. Actual GPU
fences still own the UI/source/FSR/capture resources independently of scanout.

The last UI registration build passed in 4.483 s
(`command-20261012-041001-86864.log`). Big Picture's first build found an invalid
pointer static_cast; the corrected build passed. Cache/preload Release build
passed in 30.625 s (`command-20261012-042321-30260.log`), and cache/present-mode
integration in 11.708 s (`command-20261012-042533-83996.log`). A bounded-reader
capacity check initially assumed SmallVector had max_size; the conditional
check corrected that compile error. Core then passed in 5.077 s
(`command-20261012-042742-86696.log`). All standalone targets compiled/linked in
6.468 s (`command-20261012-042817-100772.log`); warning kinds were the existing
CRT and filesystem deprecations, with no compilation errors. Diff whitespace
check passed (`command-20261012-042916-106528.log`). No tests or games ran.

Latest structural inventory: 982 files, 13358 original functions and 12186
dispatch entries; 12371 same-source-unverified, 162 changed-requires-review and
825 requiring mapping/adaptation. 1259 functions have contract associations;
zero semantic contracts are closed. These counts are not runtime equivalence.
Big Picture context switching, cache persistence/reload/corruption/concurrency,
present scheduling/backpressure and all final contents/lifetime acceptance remain
unverified. RGB96 indexing, packed formats, aliases, additional clock capabilities,
registry and runtime/package integration remain implementation work. Continue
before the global testing boundary.

## Packed images, RGB96 addressing and shader replacement — 2026-10-12

Source comparison confirms every original RGB96 mip extent/offset is a multiple
of 12 bytes. The original host detiler and buffer-image mip offset therefore
match the native full-address division/multiplication by 12; pipe-interleaved
texel addresses still require that floor. Linear layouts retain direct byte
addressing. Final mip/layer/padding contents remain unverified.

Packed UNORM 565/5551/4444 images now have native sampled/attachment formats and
R32_UINT storage words. Production SPIR-V unpacks/quantizes actual normalized
components. GPU transfers preserve physical two-byte guest words and rotate raw
format 18 or reverse format 19 nibbles for native B views. Packed sampled and
writable backings cannot borrow an incompatible SRV. MSAA transfer shaders pack
the native words through R16_UINT buffers. Twelve packed storage/sample fixtures
are compiled; attachment feature support, blend/logic/clear, direct raw format-18
conversion, NaN/OOB and ownership conversions remain open.

Original shader-list/editor controls, ISA/SPIR-V dumps, patch paths/names and
collection settings now use the native frontend. Canonical/effective patch words
remain separate. Reload runs on the GPU command thread, retires submitted users,
and compiles every affected real PSO before updating a stable stage and its ID.
Failures retain the old stage/PSOs; completion updates the list even when an
editor closes. Graphics records own their input semantics for recompilation.
Metadata schema is version 2. Cache lifecycle serialization and resource-count
validation prevent concurrent reopen during close and malformed specialization
comparisons. Reload, preload, persistence and concurrency fixtures are being
prepared; they have not executed.

Core RGB96 build passed in 8.735 s (`command-20261012-043101-78844.log`), packed
formats in 11.952 s (`command-20261012-043756-39752.log`), and embedded packed
transfer shaders in 8.798 s (`command-20261012-044304-107316.log`). FXC initially
reported a possible uninitialized value in the packed helper; a single initialized
return fixed it, and strict offline shader compilation passed. All standalone
targets with packed fixtures compiled/linked in 10.489 s
(`command-20261012-044211-18144.log`). Shader-debug integration compiled in
15.536 s (`command-20261012-044812-46904.log`), standalone targets in 3.823 s
(`command-20261012-044947-83464.log`), and completion/error handling in 11.502 s
(`command-20261012-045101-100132.log`). These are compilation results only.

Latest structural inventory: 982 files, 13358 original functions, 12186 dispatch
entries; 12369 same-source-unverified, 171 changed-requires-review and 818 requiring
mapping/adaptation. 1304 functions have contract associations across 15 groups;
zero semantic contracts are closed. Full implementation/registry/runtime/package
integration remains active. No tests or gameplay were run. Continue implementation.

## Further implementation: persistence, ownership and range contracts

The real directory/archive storage fixture and real GCN shader reload/preload
fixture were compiled and linked against current core production objects only.
Lifecycle locking serializes Open/FinishPreload/Close; specialization equality
and deserialization validate all resource counts and stable stage IDs. The reload
fixture prepares two affected PSOs, changed GPU values, failed-compilation
rollback, canonical restore and actual cache preload. None has executed.
The UI now handles a missing renderer and enqueue failures as replacement errors.

Big-picture custom profiles use the original recursive game scanner without
uploading SDL textures into the native ImGui context. Arbitrary 64-MiB index,
indirect and scratch limits and the 4096-dispatch-chunk limit were removed;
checked multiplication, D3D representability and original mapped-range clamping
remain. Core profile scan passed in 7.644 s (`command-20261012-045937-94080.log`),
range work in 6.636 s (`command-20261012-050143-60964.log`), and all standalone
targets in 4.770 s (`command-20261012-050151-47404.log`).

Packed 2D backing changes now use real GPU retile/detile and sample-preserving
transfers. The same entry, guest ownership, UID and metadata owner survive.
Replaced resources and pools remain until the actual entry fence. New physical
layout checks precede protected-memory reuse; new readback reserve participates
in the budget. The initial build failed on an allocator method name, corrected
to the existing Allocate API. Core compilation passed in 5.439 s
(`command-20261012-050933-43984.log`); all standalone targets including sixteen
protected-page backing fixtures passed in 9.692 s
(`command-20261012-051059-51888.log`). These fixtures cover two directions,
direct raw format 18, native packed-word expectations, mips/layers/samples,
padding, CPU visibility and retirement, but have not run.

Native direct/indirect index bindings now honor original max_index_size and
ClampRangeSize without reducing draw count. Converted primitives copy only the
valid prefix and zero missing indices. Zero-length IBVs follow native unbound
behavior. Core compilation passed in 7.106 s
(`command-20261012-051352-101296.log`); actual index contents remain open.

Aggregate buffer batches no longer have a 2-GiB architecture cap. Individual
raw descriptors retain checked element counts; current DXGI usage/budget now
govern the automatic cache/arena policy. Explicit user caps and opt-in memory
performance policy remain. DMA secondary tables grow with required partial
pages and retain their old live prefix through the DIRECT fence. Actual resource
capacity is published in metadata; guest-domain bounds guard shader lookup.
The first build exposed the pinned Sirit's missing OpArrayLength API; actual
capacity metadata replaced it without editing immutable dependencies. Corrected
core compilation passed in 6.538 s (`command-20261012-051749-60544.log`), and
all standalone targets in 11.657 s (`command-20261012-051857-83804.log`). The
2496-MiB aggregate fixture and actual decommit/recommit table-growth fixture are
compiled only. Large raw views, pressure, shader results and lifetime acceptance
remain open, along with the rest of the full-port registry/runtime/package work.

Compute wave size now follows real adapter and converter capabilities. The
Mesa patch exports a real 64-lane compiler path and advertises feature bit 4;
both paths emit SM6.6 wave metadata. The frontend selects its compute profile
before the original LowerWave64BallotPass and keeps the selected size in cache
identity. The checked patch SHA-256 is now
23f2106b9a7c7999b0726eed5750e890d233918110bcc69fa315a18e8434ff7c.
Core compilation passed in 34.843 s (`command-20261012-052509-36412.log`),
and all standalone targets in 26.268 s (`command-20261012-052601-80732.log`).
The full-core wave64 GCN fixture compiled/linked in 2.958/0.433 s
(`command-20261012-052829-111240.log`, `command-20261012-052832-102608.log`)
after correcting the fixture's Archive API usage. It prepares 128-thread
ballot/MBCNT/upper-lane/dynamic/divergent contents and padding checks. No fixture
has executed. Variable-size graphics waves and original wave32 restrictions,
all final acceptance and the complete implementation boundary remain open.

## Further implementation: render execution, desktop identity and BC images

Original UNORM/SRGB MIN/MAX normalization now handles the zero/one RGB and alpha
factors independently. Raw ReplaceOp selection determines the original stencil
reference even for an unreachable ReplaceOp; explicit stencil clear suppresses
subsequent draw writes. Rasterizer discard and both-face culling retain execution
of VS/HS/DS/GS and their memory side effects through D3D12's non-rasterized stream.
The stream setting participates in memory/disk PSO keys; pipeline metadata is now
v3 (PipelineVersion 2). Explicit depth/stencil clears override read-only DSV hints.
Core builds passed in 7.433 and 6.396 s (`command-20261012-053441-73968.log`,
`command-20261012-053706-16692.log`). All standalone targets compiled in 2.654 s
(`command-20261012-053502-58704.log`) after correcting fixture includes/types.
Prepared actual GPU fixtures cover blend contents, vertex UAV writes under
discard, 16/32-bit bounded index reads, padding and cold/warm draws. No execution.

Empty guest viewports now use the original one-pixel viewport/scissor fallback;
the draw no longer returns before executing its shaders. The actual vertex-UAV
fixture and pure viewport fixture cover this case. Mixed viewport winding was
subsequently corrected as described below. Point polygon fill remains
incorrectly mapped to wireframe. Primary
Vulkan rasterization/vertex-postprocessing specs confirm clipping and culling
precede point/line polygon modes: simply emitting the three original vertices as
points loses clipped polygon vertices. Wide lines, point size and those complete
rasterization contracts remain active implementation work.

PS4Desktop now derives backend from the validated selected core and accepts both
native DX12 and original Vulkan packages. Separate profiles retain graphics
settings and redirect saves/addons; stale graphics normalization no longer
disables transferred FSR/RCAS/HDR. Inherited SHADPS4 diagnostic/budget/capture flags
are cleared before explicit capture settings. New v2 manifests include source
revision/dirty patch/recursive submodules, compiler/profile identity and all
runtime hashes; v1 packages remain usable for rollback. The launch record retains
the selected metadata and hashes captured before process creation. Packaging
requires an explicit runtime file list and validates its provenance files; it
does not recursively copy an arbitrary build tree. No packaging/catalogue/default
change occurred. Desktop builds passed with zero warnings/errors in 5.736 and
0.808 s (`command-20261012-054334-23732.log`,
`command-20261012-054907-46772.log`). Prepared desktop fixtures have not run.

Sampled BC1..BC7 volumes and BC dimensions unsupported by native compressed
resources now retain the original compressed guest representation and decode
into actual RGBA8/RGBA32_FLOAT native textures. The real decoder uses the unchanged
format decoders in the pinned Mesa dependency. It is exposed by the local ABI and
feature bit 8; the cache retains its configured compiler DLL. Original guest
tiling/mip/slice/padding, immutable watches, CPU-change detection, native transfer
fences and actual allocation footprints remain in the production path. sRGB
bytes stay encoded for native SRV decode; signed/HDR values retain float channels.
Readonly compressed-source preservation is not a GPU-result proof.

The current checked Mesa patch SHA-256 is
df3e856b6b57e2885044f0b6579cd8b8eecf2b3e99724c4021037d5189d44155;
the local ABI header SHA-256 is
0995dca79faacc4fc592406644b1897282e5ba1925b5df6471f2259390702870.
Core builds passed in 33.571 and 28.713 s (`command-20261012-055211-87876.log`,
`command-20261012-055830-72184.log`). An initial fixture compile used a nonexistent
Stage::Compute enum; a real shared DXC compute compilation entry corrected it.
All standalone targets compiled in 5.375/29.419/2.646 s
(`command-20261012-055627-111892.log`, `command-20261012-060034-61960.log`,
`command-20261012-060204-86128.log`). Offline strict DXC compilation passed for
all 1D/2D/3D acceptance shader variants (`command-20261012-060203-67164.log`,
`command-20261012-060203-67832.log`, `command-20261012-060203-73704.log`).
The compiled actual GPU fixture compares expanded texture fetch/filter/mips with
independent hardware BC arrays, including signed/sRGB/HDR, partial blocks,
linear/macro/thick layouts, cold/warm/CPU changes and immutable source padding.
No fixture executed. Large-volume pressure, full BC precision/alias acceptance,
the remaining format/rasterization mappings and whole implementation boundary
are still open. Continue implementation; do not run tests/gameplay yet.

## Further implementation: arithmetic, protected mappings and actual guest flips

The original legacy multiply/MAC/MAD, ordered/unordered float comparisons,
MIN3/MAX3/MED3/SAT and subgroup LDS acquire/release ordering are restored in the
single GCN frontend and IR. The linked Mesa compiler retains runtime-data use
accumulated before linking. The current checked Mesa patch SHA-256 is
5fea8ac7499c56eb5ff674ed6d9dcf1b7a068d77b4c5d9891a38075d410b5712;
the local ABI header remains
0995dca79faacc4fc592406644b1897282e5ba1925b5df6471f2259390702870.
The full-core arithmetic fixture is compiled only; actual shader/LDS acceptance
and remaining compiler dispatch review are open.

Native front-face selection now follows the guest register directly. Both
original Vulkan and D3D12 determine winding after viewport transformation;
native shader Y conversion already preserves original framebuffer coordinates.
The previous extra XOR and mixed-viewport CullNone workaround were removed.
The prepared full-core GCN fixture covers every two-viewport sign combination,
both front modes, front/back/no culling, reversed vertices, front-face pixel
input, independent stencil operations, depth and guest padding, cold and warm.
Core compilation passed in 6.577 s (command-20261012-064629-61752.log), all
standalone targets in 4.049 s (command-20261012-065029-105840.log; 17 existing
deprecated C-runtime warnings). Corrected winding fixture compile/link passed
in 3.074/0.443 s (command-20261012-065119-81060.log,
command-20261012-065122-23252.log). No execution.

Windows SplitRegion again snapshots original per-page protection overrides
before unmapping and restores each neighbouring remap through the original
QueryProtections/MapRegion logic. Native physical aliases remain intact.
Core compilation passed in 4.187 s (command-20261012-065427-92864.log).
The corrected actual address-space fixture compiled/linked in 1.006/0.438 s
(command-20261012-065453-69516.log, command-20261012-065454-89372.log), preparing
READONLY/NOACCESS/GUARD checks, physical contents, 64/16-KiB splits and alias
lookup. The fixture uses the existing const find API rather than instantiating
the original broken const page-table indexing helper. No runtime acceptance.

GPU-completion capture now observes actual nonblank VideoOut Flip in both
backends. Native observation moved from frame preparation/submission to Flip,
retaining its ready fence. Both workers retain outside-marker counters and poll
before interval snapshots, including pending pre-start frames completed inside
the interval and excluding older completions. Duplicate IDs, failed/removed
fences, callback/file/capacity failures invalidate capture. The native CSV path
is explicit, Unicode-capable and captured in the desktop launch snapshot.

The immutable original observer now passively unwinds the exact Flip-to-Present
return PC and reads Request.index through the preserved RBX pointer, excluding
blank and unrelated nested presents. Offline proof binds the reviewed Flip body
SHA-256 ba1004021a77999a9458cc9d282c350310e185cb476fb39a08c3a38bf2510667,
resolved Presenter call target, PE unwind range, original exe/MAP/PDB and observer
hashes. Original source and executable remain unchanged. The W4/WX layer build
passed in 1.732 s (command-20261012-070434-85552.log); static proof generation in
2.558 s (command-20261012-070546-85668.log). Core builds passed in 10.557/10.462 s
(command-20261012-070434-111372.log, command-20261012-071023-35264.log).
The actual delayed DIRECT-fence capture fixture compiled/linked only in
1.809/0.436 s (command-20261012-071034-112952.log,
command-20261012-071036-59888.log). Desktop builds passed with zero warnings or
errors in 3.752/0.840 s (command-20261012-070433-75244.log,
command-20261012-071022-100124.log). Runtime unwind/ticket/interval acceptance,
loaded DLL and adapter identities and the final two gameplay launches are open.

The latest static inventory (command-20261012-065719-112808.log, 17.739 s) has
24 groups, 982 files, 13358 functions and 12186 dispatch entries: 12381 functions
are same-source but unverified, 198 require changed-body review and 779 require
explicit mapping/adaptation. Dispatch totals are 11619 same-but-unverified and
567 requiring mapping/review. 1365 functions are contracted; zero semantic
contracts are closed. Missing referenced paths and duplicate group IDs now fail
inventory generation. Static counts are not evidence of runtime equivalence.
Whole implementation/registry/runtime/package integration remains open. No
tests, game launches, packages, catalogue/default changes, commits or pushes
have run for this full-port task. Continue implementation.

## Implementation continuation: capture, storage and CP integration

VideoOut retains the original flip-coupled vblank cadence by default. Independent
vblank is an explicit native extension controlled by
SHADPS4_DX12_INDEPENDENT_VBLANK; the acceptance launch snapshot disables it.
Native and original capture record the actual adapter identities. Native module
loads and the desktop collector retain actual paths, hashes and package compiler
provenance, including transient loads. Warm shader-cache launches require only
the runtime DLLs actually needed; absence of a compiler load does not invalidate
a cache hit. The desktop collector owns and drains the acceptance child process.
Duplicate capture devices, blank markers and capture-file failures invalidate
the measurement. The original observer preserves earlier capture records.
Runtime acceptance of these paths remains pending. The desktop build passed
with zero warnings/errors (command-20261012-073807-80800.log); the immutable
original observer's static binding was regenerated
(command-20261012-074841-22124.log). Neither executable was launched.

Packed R11G11B10/R9G9B9E5 MSAA storage now uses actual relaxed-format-casting
capabilities and ID3D12Device12 allocation/resource creation with the same cast
list. Raw sample transfer preserves stored bits, including NaN payloads, before
guest tiling. The prepared full-core packed_float fixture includes retained
backings, layers, mips, single/four samples and padding. It compiled/linked only
(command-20261012-074829-60648.log, command-20261012-074831-67876.log). Older
device interfaces, optional writable R9 storage and actual adapter acceptance
remain open. Unsupported polygon Point now fails explicitly rather than silently
using wireframe; native point expansion and wide-line behavior remain required
implementation work.

Non-TC CMASK addressing now follows the exact pinned AMD AddrLib pipe and mask
element equations for P2 and the original base/alternate P8/P16 PS4/Neo pipe
configurations, including linear and tiled slice geometry. Effective alt_tile
selects the pipe configuration. The native partial-clear extension retains
unaffected samples, slices and padding. The existing fixture gained fixed
address vectors, cache-boundary and full-layout bijection cases; it was not run.
TC/FMASK-derived CMASK addressing and remaining metadata ownership are open.
The AMD MIT notice and pinned source attribution are deployed by the build;
final package integration must include this notice. Core/checks compilation
passed (command-20261012-080345-27364.log,
command-20261012-080554-67528.log). The preceding fixture compile failure was
an extra GuestPixelOffset argument and was corrected before the successful build.

CP CopyBuffer and copy HLE now share the existing precise buffer preparation and
dirty-range path. The original u32 region size is no longer constrained by a
64-MiB coalescing heuristic; active resources remain subject to actual allocation
and residency limits. Zero copies are inert, unrelated cached mappings are
retained and guarded CP DMA preserves its pending list. GDS writes share dirty
ownership; CPU GDS reads resolve overlapping GPU data, while internal-array
commits do not invoke guest physical-backing callbacks. The existing copy fixture
retains older union/retry coverage and prepares a 68-MiB unaligned CP transfer,
GDS chaining/read, padding and unrelated residency checks. Core/checks builds
passed (command-20261012-081300-50828.log,
command-20261012-081344-91540.log). Actual copy contents, ordering, aliases and
original cold CPU-copy optimization remain unverified.

Compatible simultaneous color targets at one guest address can share one array
texture when their active layers are disjoint. Storage grows to the maximum
layer count before preparation, preserving each view's range. Submission checks
actual mip/layer intersections rather than rejecting every repeated resource
pointer. Partial attachment readback and snapshot update now deduplicate by
shared cache entry with an explicit recorded-row mask. Intersecting or
incompatible attachments, backing growth across separate draws, feedback and
color/depth aliases remain open. The existing graphics-resource fixture retains
all older branches and adds reordered layer views, single/four samples, exact
contents/padding and debug validation. The new full-core gcn_attachment_alias
fixture invokes actual GCN shaders, Liverpool registers and GuestRasterizer::Draw,
including eager guest readback. Core/checks builds passed
(command-20261012-082715-110700.log, command-20261012-082715-19224.log); the
full-core fixture compiled/linked (command-20261012-082724-30804.log,
command-20261012-082727-64920.log). No runtime fixture was executed.

Inventory source classification distinguishes explicit original constant stubs,
partial limitation markers and unsupported aborts without labeling original
implemented functions as closed. Calling-convention recovery improves conservative
registration/caller links. Declared cross-file function mappings validate actual
unique definitions, path membership and symbol selection, reject conflicts and
retain signature/semantic review. They do not certify equivalence. The three
original AMD format helpers are explicitly bound to their native out-of-line
definitions; all 126 table integer entries match for the original domain 0..41.
Invalid out-of-table input remains outside the original defined contract.

Latest static inventory (command-20261012-083456-109292.log, 18.805 s): 29
groups, 982 files, 13358 functions and 12186 dispatch entries; 12380 functions
are same-source but unverified, 203 require changed-body review and 775 require
mapping/adaptation. Dispatch remains 11619 same-but-unverified and 567 requiring
mapping/review. There are 1376 contracted functions, three declared cross-file
mappings, 6785 conservative caller links and zero closed semantic contracts.

All the changes above remain integrated_review_pending. Whole implementation,
function/dispatch reconciliation, runtime/package integration and final Release
acceptance remain open. The full-port testing boundary has not been reached;
no tests, gameplay launches, packages, catalogue/default changes, commits or
pushes have run. Continue implementation, without a partial completion label.

The original CP cold CPU-copy path is now integrated as well: non-GDS source
and destination ranges with no modified GPU buffers and no cached source image
use host copying after backing/right checks and destination image reconciliation.
Clean buffer allocations remain resident; repeated invalidation of an already
dirty CPU page preserves the remaining equality proof. Native memmove retains
defined overlapping copies outside the original memcpy-defined domain. Source
image detection conservatively includes intersecting image/stencil allocations.
Copy HLE retains its ordered GPU path. The existing production guest-buffer
fixture prepares a protected cold destination, unchanged neighbouring pages,
one-page comparison and actual GPU-cache readback. The large CP fixture now
prepares both cold CPU and GPU-modified-source copies above 64 MiB, retaining
the earlier GDS/dirty-union/retry cases. Core/checks compilation passed in
15.837/14.420 s (command-20261012-084023-80912.log,
command-20261012-084023-105240.log). Checks retained existing CRT deprecation
warnings; no fixture was executed. Actual cold-copy ordering, image ownership
and contents remain unverified.

The declared mapping pass exposed an AST namespace-recovery error after the
native SCOPE_EXIT macro; it did not silently accept a missing GDS definition.
Inventory now parses that known scope-exit syntax as an equal-width lambda while
retaining original bytes for signatures, hashes and line offsets. Normalized
files retain raw parse-error counts and the normalization marker. The CP/GDS
definitions are now explicit unique mappings. Generation passed
(command-20261012-084536-94116.log, 18.914 s) after the rejected endpoint
(command-20261012-084339-78920.log). No semantic closure follows from this repair.

Common HLE review restored three original behaviors that had diverged in the
native base: all four network clock IDs retain epoch time from system_clock's
registration anchor plus steady elapsed time and one-nanosecond getres;
sysmodule ID validation retains the full u32 error bits rather than converting
INVALID_ID to bool/1; FindGameByID checks current/direct directory and archive
roots before its recursion-depth bound and stops inside another valid game root.
The common definitions and existing ABI/NID callers remain original. Core builds
passed (command-20261012-084646-48956.log, 4.377 s;
command-20261012-084756-63200.log, 5.046 s). Runtime HLE acceptance is unrun.

Original shader stage/hash/optional-permutation naming is now a common
Shader::GetShaderName helper. Actual native shader dumps/patch lookup and
graphics/compute register dumps use it, preserving original fixed-width
hexadecimal names and stage labels. The register-dump paths no longer invent
unrelated decimal names. The original GetShaderName is explicitly mapped to the
common definition. Core compilation passed
(command-20261012-085019-15880.log, 4.414 s); editor/dump/patch runtime acceptance
remains pending.

Signed 10:10:10:2 sampled single-sample textures now retain their four-byte guest
storage while expanding to actual sixteen-byte RGBA32_FLOAT/SINT host resources.
RGB/alpha sign extension and signed normalized conversion precede native
sampling; original component remapping, tiling, pitch, layers, mips and 1D/3D
dimensions remain in the existing descriptor/upload path. Host allocation/views
use the expanded format family; unchanged warm samples and immutable raw guest
readback/padding remain in the existing cache contract. Signed expansion does
not load the compressed Mesa decoder. Writable/attachment/MSAA packed formats
still fail explicitly because expanded float rendering would lose per-fragment
signed normalized quantization. Exact writes/blending remain implementation work.

The new signed sampled fixture prepares actual GPU-buffer readback of expanded
texture data, all signed 10-bit values and alpha endpoints, typed SRV creation,
linear/tiled 1D/2D/3D mips/layers, warm/change reuse, immutable guest contents and
debug checks. It does not substitute immutable CPU-source readback for GPU-data
comparison. Core compilation passed
(command-20261012-085418-88832.log, 5.655 s); checks compilation passed
(command-20261012-085722-45132.log, 5.271 s), including the new fixture. Actual
GCN sample/filter/gather/LOD/swizzle and original-adapter equivalence remain
unrun. This group is integrated_partial_review_pending, not a completed format
subsystem. The whole implementation/registry/runtime/package boundary remains
open; no tests, gameplay, packages, catalogue/default changes, commits or pushes.

Latest inventory (command-20261012-085809-109800.log, 18.978 s) has 33 groups,
982 files, 13358 functions and 12186 dispatch entries: 12385 same-source but
unverified functions, 201 changed-body reviews and 772 mapping/adaptation gaps;
11619 same-but-unverified dispatch entries and 567 mapping/review entries.
1395 functions are contracted, six definitions are explicitly mapped and zero
semantic contracts are closed. The native rasterizer has zero normalized AST
parse errors, with ten raw errors retained as evidence. Original source behavior
classification remains 2880 explicit constant stubs, three unsupported aborts,
210 partial/review markers and 10265 functions requiring semantic review.

The cold CP source-image gate now uses a lazy merged index of original guest
image and stencil allocation intervals instead of scanning every cached image
for each DMA packet. Cache insertion, eviction and invalidation dirty the index;
unchanged ownership/fence updates retain it. A rebuild is transactional, followed
by logarithmic half-open overlap queries. The public entry collection no longer
permits external container mutation. The signed sampled fixture prepares exact
begin/end and post-invalidation index checks as well as the GPU-data coverage.
Core/checks builds passed (command-20261012-090505-95308.log, 6.219 s;
command-20261012-090505-71188.log, 8.332 s). The initial compile rejected a single
auto declaration with integer and iterator types; they were split before both
successful builds (earlier command-20261012-090441-102208.log and
command-20261012-090441-50804.log). Runtime alias/predicate acceptance is unrun.

The cold-copy image predicate now follows the original SafeToDownload ownership
condition instead of treating every cached sampled image as GPU-authoritative.
Separate lazy indexes cover all cached guest allocations and GPU-owned
allocations. Entry::SetGpuOwned invalidates the ownership index on actual
changes; all production and fixture ownership writes use it. Coherent sampled
and eagerly reconciled images can use the CPU copy path, while unresolved GPU
owners retain the ordered GPU path. Insertion/eviction invalidate both indexes.
Prepared fixtures cover immutable-image exclusion and real clear/readback
ownership transitions. Core/checks compilation and linking passed
(command-20261012-091143-76920.log, 15.961 s;
command-20261012-091143-52964.log, 14.275 s). These are compile-only results;
implementation/registry/runtime/package completion and final acceptance remain
open. No tests, gameplay, package/catalogue/default changes, commits or pushes.

Signed packed 10:10:10:2 shader storage now uses the existing coherent UAV
runtime with exact four-byte R32_UINT words, explicit sign extension, normalized
clamping/nearest-even quantization, integer component encoding and original
mip/layer/sample bounds. Sint/Snorm writable interpretations share raw backing;
the existing unsigned R10G10B10A2 typed storage/view behavior is preserved.
Persistent pipeline metadata now uses v4 identity for the adapted shader
semantics. Twenty new emitter/runtime fixture cases cover all four numeric
variants in tiled/linear/MSAA/1D/3D paths and warm reuse, without execution.

Signed sampled expansion now runs in the existing GPU detiler, including
distinct MSAA samples. Actual expanded RGBA32_UINT transfer views preserve the
float or signed component bits before typed sampled views; the original guest
snapshot/padding remains immutable. The fixture uses a real GPU transfer buffer
for multisample readback, with per-sample expected values and retained leases.
FXC offline compilation passed with /O3 /WX for the modified tiler and unsigned
four-component MSAA upload. Core/checks compilation passed after preserving the
unsigned paths (command-20261012-093045-83936.log, 8.289 s;
command-20261012-093045-61204.log, 7.909 s). The main runtime already lowers 1D
images to height-one 2D consistently with the original emitter; that existing
lowering was inspected and retained, not rewritten.

Signed integer 10:10:10:2 color output is integrated into real fragment exports,
using a UINT signature and explicit signed channel encoding with the actual
packed R10G10B10A2 RTV. Original common/frontend numeric/export remapping stays
signed. Native sampled lookup rejects borrowing this four-byte attachment as a
sixteen-byte expanded sampled image and uses existing image reconciliation.
The original full-core shared-MRT fixture retains its unsigned mode and now
also prepares signed-color GCN exports with negative endpoints, signed two-bit
alpha, disjoint array layers, one/four samples, slot permutations and padding.
Core compilation/linking passed (command-20261012-093504-102324.log, 5.687 s);
checks compilation passed (command-20261012-093538-28488.log, 6.414 s);
full-core fixture compilation/linking passed
(command-20261012-093540-30928.log, 3.033 s;
command-20261012-093543-66832.log, 0.463 s). No fixture has executed.

Snorm attachment blend/quantization, mixed numeric/storage aliases, host
expanded-format sample capability fallbacks, real sampler/shader acceptance
and original differential behavior remain open. These integrations do not
close the full implementation/registry/runtime/package boundary. No tests,
gameplay, packages, catalogue/default changes, commits or pushes were performed.

### Signed packed GPU backing conversion (2026-10-12, compilation only)

Compatible signed packed UAV/Sint attachment backing now converts to expanded
Snorm/Sint sampled storage using the existing GPU retile/detile path. Expanded
views retain the original GPU guest words and written-byte mask. Numeric view
changes and reverse packed conversion use those original words; normalized
negative-minimum encodings are not reconstructed from equivalent float values.
Expanded GPU readback copies that backing and transactionally merges logical
bytes into existing guest padding. Separate persistent resident accounting keeps
raw resources alive after temporary format-transfer retirement. CPU reupload
discards stale raw backing after prior users retire.

Eight additional prepared PageWatch/GPU fixture modes cover both signed numeric
variants, linear/tiled layouts and one/two samples; six view transitions include
expanded reinterpretation and reverse writable conversion. They prepare checks
for actual expanded single-sample GPU values, exact original guest words,
mips/layers/samples/padding, zero intermediate CPU faults/resolves, raw backing
lifetime and final CPU visibility. No mode executed. Existing packed16 fixture
cases remain enabled. Direct 1D/3D conversion, mixed unsigned/signed aliases,
simultaneous views, arbitrary physical layouts and sample-capability fallbacks
remain open.

Core compilation/linking passed (command-20261012-094859-45656.log, 10.794 s).
Checks compilation/linking passed (command-20261012-094859-79860.log, 13.341 s).
The preceding static inventory passed (command-20261012-093959-78972.log,
23.480 s): 35 contract groups, 982 original files, 13,358 functions, 12,186
dispatches; 12,383 same-source unverified, 203 changed requiring review, 772
requiring mapping/adaptation; 11,619 same dispatch unverified and 567 requiring
mapping/review. There were 1,400 contracted functions, six declared mappings and
zero semantic closures. This inventory predates the new backing-conversion
group. Original checkout status was clean (command-20261012-093959-59468.log);
native whitespace check passed (command-20261012-093959-39384.log). No tests,
gameplay, packages, catalogue/default changes, commits or pushes were performed.

### Packed numeric/shape transitions and disjoint shader views (2026-10-12)

The backing conversion now supports direct 1D/3D footprints and compatible
Unorm/Snorm/Uint/Sint transitions, preserving the correct R32, R10G10B10A2 or
expanded RGBA32 typeless allocation family. The prepared PageWatch fixture now
has 32 packed10 modes: signed-only/mixed transitions, linear/tiled storage,
one/two samples and direct 1D/3D shapes. It checks packed or expanded GPU bytes
and guest depth/mip/layer/sample padding while preserving the original raw
normalized endpoint encoding. Existing packed16 cases remain intact.
Compilation passed for shapes (command-20261012-095112-50712.log, core 6.200 s;
command-20261012-095112-86000.log, checks 5.776 s) and numeric transitions
(command-20261012-095241-102812.log, core 6.115 s;
command-20261012-095241-78568.log, checks 5.690 s).

Common resource preparation now permits compatible packed10 sampled/writable
views of disjoint subresources. It prepares sampled resources first, retaining
their descriptors/resources through existing conversion/cache fences, then
leaves the authoritative cache entry on the final compatible writable backing.
Reuse validates current entry metadata instead of relying only on metadata
saved before conversion. Ownership/readback lists deduplicate cache entries.
Existing subresource overlap rejection remains; overlapping feedback and
simultaneous writable representations from different numeric families still
need adaptation.

Eight new actual emitter/DXIL/runtime fixture modes prepare all four sampled
numeric interpretations with signed writes, disjoint mips/layers, writer-first
image metadata and three warm dispatches. They check sampled values, written
words, untouched storage/output and debug messages. Core compilation/linking
passed (command-20261012-095709-103516.log, 6.411 s); checks compilation/linking
passed (command-20261012-095709-5172.log, 5.842 s). No fixture executed. Full
frontend alias classification, graphics attachment feedback, arbitrary layouts,
host sample fallbacks, Snorm attachments and final differential acceptance
remain open. The registry has 37 unique groups, with no semantic closure.
No tests, gameplay, packages, catalogue/default changes, commits or pushes were
performed. The full implementation/runtime/registry/package boundary remains open.

### Native device settings, GPU markers and removal diagnostics (2026-10-12)

Original global/game graphics controls now reach native device creation: explicit
DXGI adapter selection, debug validation/core/sync/GPU controls, crash diagnostics,
host/guest markers, RenderDoc and pipeline-cache settings. Legacy Vulkan JSON/TOML
keys translate to native keys with native-value precedence. DXGI enumeration order
differs from Vulkan; D3D12 sync/GPU validation requires the native core debug layer.
Unavailable explicitly selected adapters fail instead of silently selecting another.

Guest PM4 scopes/inserts and host presentation/FSR/ImGui events encode the pinned
Microsoft PixEvents legacy GPU payload with actual ARGB colors and full labels.
Logical guest scopes close at submission and reopen on the next command list.
The MIT header/license provenance is retained; no PIX CPU runtime is required.
Requested DRED breadcrumbs/page faults are enabled before device creation.
Submission, scheduler waits and presentation failures report available removal
reason, breadcrumbs, contexts and allocation evidence before rethrowing.

The prepared hardware configuration fixture covers default/validation/invalid
adapter modes, LUID selection, long colored labels, scopes across submissions,
GPU copy completion and debug messages. It has not executed; DRED device-loss,
PIX decoding, settings persistence and differential acceptance remain unverified.
Core and checks compilation/linking passed (command-20261012-101431-15916.log,
15.355 s; command-20261012-101432-44192.log, 13.360 s). Static inventory passed
(command-20261012-101430-97904.log, 26.876 s): 39 unique contract groups, 982
original files, 13,358 functions, 12,186 dispatches; 12,383 same-source unverified,
209 changed requiring review, 766 requiring mapping/adaptation; 1,421 contracted
functions, 12 declared mappings and zero semantic closures. No tests or games ran.

### User-requested intermediate snapshot (2026-10-12, preparation)

The user explicitly requested a current compiled core for their own manual check
while the full migration continues. This authorizes an experimental intermediate
package before the full implementation boundary, without changing final acceptance.
Fresh incremental core compilation/linking passed
(command-20261012-102052-86776.log, 1.397 s); desktop publish passed
(command-20261012-102051-16724.log, 8.822 s). Packaging uses the existing manifest
and desktop launch/catalogue implementation with isolated data and copied saves.
The user then directed this snapshot into the existing installed desktop app.
The normal catalogue gains one immutable experimental package; previous packages
and the selected default are preserved. An optional manifest IsolatedProfile flag
routes launches through the same NativeGameLaunch policy into a per-version profile
copied from the current backend profile. Installed original saves remain unchanged.
Source dirty patch, dependency/compiler identities, runtime hashes and matching
debug symbols accompany this snapshot. Its static package verification does not
establish game readiness, performance or complete original-function parity.

The snapshot is installed as
`dx12-20261012-full-port-preview-95d72e06d1ac` in the existing
`%LOCALAPPDATA%/PS4Desktop/cores` catalogue. Core SHA256:
`95d72e06d1acf7dc1a780086dbc382039d5e1895a9486c57f19004eda7459a7e`.
All 76 manifested files were copied and hash-verified; static PE imports resolve
to packaged DLLs or Windows system/API-set dependencies, with no Vulkan import.
Twenty save files were copied into the per-version profile; original save hashes
and installed settings hash were unchanged. The installed desktop executable at
`C:/Users/skei/AppData/Local/Programs/PS4Desktop-DX12/PS4Desktop.exe` now uses the
compiled manifest/profile support. Its previous EXE/PDB are retained in this task's
`work/installed-desktop-preview-backup`; publish passed
(command-20261012-102534-56964.log, 1.661 s). Packaging/static checks passed
(command-20261012-102631-77588.log, 23.598 s). No app/core/game test was executed.
Prepared profile fixtures cover copied saves, version-profile reuse, unchanged
shared saves/settings and rollback to the baseline backend; they have not run.
User-facing launch instructions and package/import identities are saved in this
task's `outputs/DX12-current-preview-20261012`. This preview is not final acceptance.

### Coherent mixed packed10 storage and diagnostic correction

Mixed format-9 Unorm/Snorm/Uint/Sint shader views containing signed interpretations
now share authoritative R32_TYPELESS/R32_UINT storage. The actual resource pass,
whole-pipeline frontend, shader specialization and common texture binding propagate
the raw packed representation; emitted loads/stores interpret and quantize each
numeric view. Unsigned-only storage retains typed R10 backing. Pipeline metadata
uses v5 and stage/pipeline record version 3 after adding cross-stage signed-view
inputs. Prepared coverage includes 36 classification cases, four coherent GPU
emitter modes and all 16 numeric reader/writer pairs in actual GCN graphics stages,
with shared/separate/restored aliases and DXIL compilation. None has executed.

DRED reports now encode UTF-16 names/contexts as UTF-8 without mixing wide and
narrow stdio. Actual DIRECT/COPY queues, lists and allocators have stable names;
context acquisition and worker fence failures report removal diagnostics. Core
compile/link passed (command-20261012-102828-38164.log, 5.273 s), checks compiled
(command-20261012-102918-5920.log, 4.433 s). Subsequent packed10 core/check builds
passed (command-20261012-104452-23932.log, 19.713 s;
command-20261012-104452-45164.log, 21.246 s). Full-core frontend, storage, reload
and attachment fixtures were refreshed using CompileOnly. Checks incremental
compile/link passed (command-20261012-105513-52208.log, 3.177 s).

Static inventory passed (command-20261012-105512-75752.log, 20.271 s): 40 groups,
982 files, 13,358 functions, 12,186 dispatch entries; 12,383 same-source unverified,
210 changed requiring review, 765 requiring mapping/adaptation, 1,431 associated
with contracts and 13 explicit mappings. No semantic contract is closed. Filtered
and MSAA numeric-alias contents, arbitrary image/buffer/depth/color feedback,
signed normalized attachments, point/wide-line rasterization and other remaining
contracts still require completion and final acceptance.

The old scene-depth-bypass.enabled filesystem override was removed from the
actual draw path: depth comparison, stencil and bounds now follow guest state
regardless of an unrelated working-directory file. This correction and the new
GCN pair fixtures have compiled/linked (command-20261012-105754-85948.log,
link 0.456 s); core compilation passed (command-20261012-105722-108852.log,
6.732 s). The delivered preview
predates these changes and retains its recorded hash; main migration continues.

Nearest Uint/Sint sampling now follows manual typed loads for both read-only and
shared writable resources, preserving integer components and border ZERO/ONE
bits instead of rejecting ordinary integer storage sampling. Integer linear,
reduction, comparison-depth, degamma and custom-border cases still require their
separate original/host feature review. The reference profile is unaffected.
Eight actual GCN graphics cases prepare ordinary R32/packed10 Uint/Sint nearest
sampling with shared/separate resources, sampler reflection and DXIL checks;
they have not executed. Initial core/checks compilation passed
(command-20261012-110100-90456.log, 3.843 s;
command-20261012-110100-66912.log, 3.922 s). Final small source cleanup and the
nearest GCN cases compiled/linked (command-20261012-110235-31384.log, link
0.452 s); cleanup core/checks passed (command-20261012-110200-46352.log,
3.771 s; command-20261012-110200-111832.log, 3.172 s). Registry had 41 groups.

### R32/RGBA8 shader format reinterpretation

Compatible non-depth format-4 and format-10 shader views now use common
R32_TYPELESS/R32_UINT backing when the actual alias group mixes R32 and RGBA8.
Resource classification, whole-pipeline reflection, specialization, SPIR-V
packing, texture descriptors, ownership and GPU backing conversion are integrated.
R32 values are bit representations; RGBA8 components retain their own numeric
interpretation and quantization. sRGB decode follows raw-byte unpacking.
Cross-stage cache relationships explicitly distinguish different formats, so
splitting a group returns its writer to ordinary typed storage. Serialized
pipeline metadata is v6; stage/pipeline record version is 4.

Prepared coverage adds 120 native/reference classification cases, thirty GCN
cross-format numeric pairs with shared/separate/restored identities and seven
GPU emitter modes with read-after-write, three warm mip/layer passes, reversed
descriptors, exact R32 bits including a signaling-NaN word and full padding.
Core/checks compiled (command-20261012-111333-2288.log, 18.696 s;
command-20261012-111333-17140.log, 18.901 s), then cache correction/fixture updates
compiled (command-20261012-111853-37776.log, 7.817 s;
command-20261012-111853-56396.log, 5.821 s). All ten full-core fixture executables
were refreshed using CompileOnly after this Info ABI change; task logs are
`work/raw32-<fixture>-compile.log` (link logs command-20261012-112048-28828 through
112142-80008). No fixture was executed. Modified compilation diagnostics contain
no non-deprecation warnings; complete raw compiler logs are retained.

The registry has 42 groups. Cross-format GPU contents/barriers/NaN acceptance,
filtered sRGB, MSAA/1D/volume/atomic aliases, broader physical layouts and
buffer/depth/color feedback remain open, alongside signed-normalized attachments
and point/wide-line rasterization. The user preview remains the earlier immutable
snapshot in the existing installed application, with its recorded profile/hash.

Static inventory passed (command-20261012-112041-18920.log, 20.827 s): 982 files,
13,358 functions, 12,186 dispatch entries; 12,383 same-source unverified, 210
changed requiring review, 765 requiring mapping/adaptation, 1,433 associated with
contracts and 13 explicit mappings. The 42 groups close no semantic contract.
Whole-tree diff whitespace check passed; the pinned original checkout and
recursive submodules remain clean. No test, app/core/game launch, commit or push
has run in the current full-port task.

Direct VS PointList without PointSize export now retains its native one-pixel
point topology even when polygon point-fill control is selected. Prepared state
coverage keeps sized-point and polygon-to-point gaps explicit; larger points,
GS/tessellation point output and wide lines remain incomplete. Core/checks
compiled (command-20261012-112750-23732.log, 4.380 s;
command-20261012-112750-101332.log, 3.295 s); whitespace check passed. No test ran.

Clock source review reconfirmed pinned EmitMemtime returns constant 1 when its
shader-clock capability is false. Native still requires a hardware implementation
when a real clock is needed; Intel/other-adapter capability correspondence and
the original no-clock fallback remain open, with no new fallback/code change.
The published Microsoft D3D11.3 specification describes its cycle input as debug
only, so it is not a production native-clock solution:
https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#7.15
The retained Intel atomic-extension sample/header provides no clock declaration;
that older sample does not establish availability in newer framework versions.

The one-to-one anisotropy limit now maps AnisoPoint/AnisoLinear to corresponding
point/bilinear filtering in manual shared-UAV/special-addressing loads. This also
supports the pinned sampler post-operation that clears the anisotropy ratio to
one while retaining its XY filter enums. Four GCN gradient cases prepare both
filters with zero/nonzero gradients. Core/checks compiled
(command-20261012-113614-109716.log, 3.774 s;
command-20261012-113614-63164.log, 3.131 s). The GCN executable was refreshed
without execution (command-20261012-113711-60120.log, 0.472 s). GPU acceptance
is pending; actual multiple-tap anisotropy remains incomplete.
Registry now has 43 groups with no semantic closures. The preceding point-state
inventory passed (command-20261012-113050-25304.log, 20.150 s) with the same
function/dispatch counts; it predates this final sampler change.

The second user-requested intermediate package includes all current production
changes through unit anisotropy, unit PointList, raw R32/RGBA8 aliases, coherent
packed10 numeric views, integer nearest sampling and DRED UTF-8 diagnostics.
Its incremental Release build passed (command-20261012-114625-96336.log,
1.435 s); diff whitespace validation passed. The current 43-group static
inventory passed (command-20261012-114624-96024.log, 20.638 s), with no semantic
closures. Packaging reuses the installed desktop and copies the currently
selected isolated profile into a new version; no game or test is executed.

The second package is installed as
`dx12-20261012-full-port-preview-7e7092ff4b78`, displayed as
`DX12 — перенос №2 — 12.10.2026`. Core SHA256:
`7e7092ff4b78f03ffc20d9b6e292aec2cfc8e03f8f8915daa6d0d14e238380ad`.
Packaging/static checks passed (command-20261012-114724-16684.log, 24.697 s):
76 manifested files, 15 PE import/delay-import tables, resolved packaged/system
dependencies and no Vulkan import. Its 20 save files came from the currently
selected `95d72e06d1ac` preview's isolated profile. That package and source saves
were hash-checked unchanged; settings and installed desktop EXE stayed unchanged.
User-facing instructions and provenance are in the task's
`outputs/DX12-preview-2-20261012`. No app/game/test was launched by the agent.

### Separate buffer/image prewrite versions — after snapshot two

Pinned `Rasterizer::BindResources` binds buffers then textures for each stage.
`BindBuffers` invalidates cached images for raw buffer writes;
`RefreshImage` uploads the preceding buffer contents through
`ObtainBufferForImage` before the draw/dispatch executes. This uses separate
resources and does not establish in-dispatch live buffer/image coherence.
`SynchronizeMemoryFromImage` handles formatted read-only buffer views separately.

Native execution now accepts an overlapping read-only view with either one
buffer or one image writer. For a buffer writer, prior owners reconcile and the
complete sampled guest image is captured before buffer preparation. Duplicate
address/size captures share immutable storage. Texture upload consumes those
bytes, releases prior guest-memory witnesses and cannot install a watch proving
that mutable guest backing still equals the snapshot. For an image writer,
already prepared read-only buffer resources and leases keep the preceding version;
image ownership/readback alone updates the overlapping guest bytes.
Two simultaneous writers still fail. Writable buffer overlap with active bound
color/depth/stencil attachments also fails, including conservative read-only
depth-attachment cases, so sampled snapshots cannot conceal competing owners.
GPU detile directly from resident buffer snapshots, guarded physical aliases,
broader GCN/graphics/depth/volume/MSAA equivalence and performance remain open.

Five prepared metadata-to-DXIL/GPU cases cover both directions, full/partial
tiled overlap, a preceding actual compute buffer writer, three warm submissions,
a CPU edit, complete allocation/padding, snapshot-witness retirement and rejection
of two simultaneous writers before execution. All are compiled, none executed.
Final core/checks compilation passed (command-20261012-120447-112656.log,
6.805 s; command-20261012-120507-24096.log, 2.856 s). Checks initially exposed a
missing relative header include in the new native-port fixture; explicit source
include paths fixed it without changing the compiler/ABI. Non-deprecated build
warnings: zero. Whitespace validation passed. All ten full-core fixture
executables were refreshed in compile-only mode, with no execution.

The current 44-group inventory passed (command-20261012-120609-71656.log,
22.158 s): 982 upstream files, 13,358 functions, 12,186 dispatch entries,
1,441 contracted functions, 13 explicit mappings and zero semantic closures.
The original checkout stayed clean. This post-package implementation is not in
the immutable second preview. Full implementation and final acceptance remain open;
no commits or pushes have been made.

### Latest complete image clear implementation, after snapshot six

Production covers all resource mip/layer/volume clear ranges independently of
the restricted cached view. Typed 8/16/32-bit normalized, integer and float UAV
clears accept nonzero values; R32 raw float clears preserve VkClearValue union
bits and negative zero. Raw RGBA8 nonzero clears use temporary native typed
clear then GPU retile/detile into the existing R32 image, preserving its identity,
CPU snapshot, guest padding and independent physical/resident buffer versions.
Actual dimensional UAV store support and temporary texture/scratch memory budget
are checked before recording; descriptor heaps and transfers retire by GPU fence.
Prepared coverage includes 420 actual GCN zero/nonzero HLE shape cases, 120
storage clear API cases and nine additional signed-zero warm cases. None run.
Core/checks compile (`command-20261012-194730-10376.log`, 7.500 s;
`command-20261012-194808-92876.log`, 13.893 s); all ten full-core fixtures refresh
compile/link only. The 78-group inventory passed
(`command-20261012-194919-32020.log`, 21.846 s), zero semantic closures.
Whitespace and non-deprecation diagnostics clean. Full implementation/acceptance
remain open; immutable preview six predates these latest changes.
Post-snapshot-seven production also supports sampled packed16 volumes using GPU
RGBA32 expansion with exact retained two-byte backing/masks, GPU packed10/16
nonzero clear packing, native SRGB RTV clear conversion and clear promotion of
compatible cached typed images without UAV. HLE clear now retains independent
CPU/buffer ownership even when the cached image was previously read-only; direct
protected-mode transfer does not open an unprotected store window.
Prepared coverage totals 4,077 basic/3,474 divergent buffer/image combinations,
1,260 post-writer HLE clear combinations plus 504 cold read-only HLE cases,
234 storage-clear API cases, nine negative-zero/twelve packed10/nine SRGB warm
endpoint cases and eight packed16 volume round trips. None executed.
Current production core compiles (`command-20261012-202340-76568.log`, 6.791 s);
final core/checks refresh (`command-20261012-202536-26676.log`, 1.446 s;
`command-20261012-202536-52676.log`, 3.898 s) and all ten full-core fixture links
pass. Strict SM5.1 HLSL /Ges /WX passed
(`command-20261012-200916-110500.log`, 3.654 s), with round_ne in assembly.
The 83-group inventory passed (`command-20261012-202723-49364.log`, 20.986 s),
1,476 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Pressure/priority linear readback, broader ownership/geometry/MSAA/BC,
graphics/metadata/full upstream review and final runtime acceptance remain open.
Post-snapshot-eight production adds synchronous compact mip-zero downloads for
independent linear image pressure retirement. Actual physical backing samples
are validated before copies; source lifetime and mapped assembly survive through
the fence. Physical CPU commits retain independent resident-buffer bytes and
update comparison baselines without creating CPU-store upload markers.
Twelve compact 2D/1D-array/volume API cases and protected buffer commit/failure/
same-value-store cases compile only. Core/checks passed
(`command-20261012-204512-52180.log`, 4.701 s;
`command-20261012-204512-90676.log`, 3.434 s), all ten full-core fixture links
passed and non-deprecation diagnostics/whitespace are clean. The 84-group audit
passed (`command-20261012-204541-57212.log`, 20.983 s), 1,478 associations,
13 mappings, zero semantic closures. Legacy queued/priority downloads and cache
refresh source domains remain open; no runtime checks or game launches ran.
Immutable preview eight predates these changes. Main migration continues.
Legacy submission/fence download queuing now covers supported independent
single-sample images. Storage binding queues once; HLE clears preserve an
existing queue but do not invent a new binding. Successful physical commit keeps
both the image and old raw arena live, and updates the CPU comparison baseline.
Eight actual GCN linear single/array and base/two-mip-chain cases are prepared,
including two initial values, OnFence/OnSubmit, stale-download prevention and
same-value CPU stores. Core/checks passed
(`command-20261012-205142-96268.log`, 7.186 s;
`command-20261012-205142-67668.log`, 4.134 s), all ten full-core fixtures link
only; no execution. The 85-group audit passed
(`command-20261012-205204-26276.log`, 21.217 s), 1,478 associations/13 mappings/
zero semantic closures. Diagnostics/whitespace clean. Ordinary attachment and
broader format ownership, priority lifetime/pressure behavior and full acceptance
remain open. These source changes are absent from immutable preview eight.
Priority worker retirement now supports explicit drain through callback capture
destruction, caller-side exception propagation and destructor join. Its prepared
GPU fixture covers retained readback, caller-lock release, callback failure with
following successful retirement and shutdown completion. Repeated storage
bindings notify the download queue even when ownership was already acquired;
HLE clear remains separate from that binding notification. Current core/checks
pass (`command-20261012-205725-13820.log`, 8.662 s;
`command-20261012-205725-90248.log`, 14.609 s); all ten full-core links were
refreshed after this ownership-reuse correction. No execution. The 86-group audit
passed (`command-20261012-205540-75252.log`, 21.149 s), zero semantic closures;
diagnostics/whitespace are clean. Priority image/pressure integration remains open.
Supported independent linear pressure eviction now defers map/physical CPU
commit onto the priority worker, with a bounded 64 MiB readback/assembly account
and synchronous fallback. The first adaptation retains its GPU wait, releasing
the source texture lease before eviction. Pending physical ranges retain actual
CPU-store markers even before the deferred write. Image retirement removes only
protection outside resident buffer witnesses and other protected images, without
a restore/rearm window. Derived/base destructors drain before callback fields
disappear. Protected real-GPU readback/CPU-store and image-witness gap cases are
prepared; repeated cold read-only HLE clear cases now also retain their original
read-only descriptor across consecutive clear values. No execution.
Current production core compiles (`command-20261012-210909-43040.log`, 7.258 s);
final core/checks refresh passes (`command-20261012-210949-46216.log`, 1.438 s;
`command-20261012-210949-73640.log`, 2.983 s), with all ten full-core fixture
links refreshed. The 87-group audit passed
(`command-20261012-211028-81512.log`, 21.245 s), zero semantic closures;
non-deprecation diagnostics/whitespace clean. Nonblocking GPU pressure, broad
queued ownership/formats, unmap/remap and runtime acceptance remain open. These
changes remain outside immutable preview eight. Main migration continues.
RGB32 and read-only wide typed clear promotion now retains native RGBA32
identity while transferring only three physical RGB32 components. Direct
RGB32 UAV/RTV clears preserve absent alpha as float/integer one. Prepared
coverage adds 45 wide read-only/RGB32 API cases (279 total), nine read-only
negative-zero cases (18 total), independent host-alpha footprint checks and
correctly sized 96-bit GCN clear destinations. No execution. Core/checks pass
(`command-20261012-211449-113132.log`, 7.280 s;
`command-20261012-211449-98512.log`, 4.269 s), all ten full-core fixture links
refresh. The 88-group inventory passed
(`command-20261012-211530-103464.log`, 21.117 s), zero semantic closures;
diagnostics/whitespace clean. Expanded/BC/MSAA promotion and full GPU acceptance
remain open; immutable preview eight predates this work.
Expanded signed packed10 and normalized packed16 volume clears now GPU-pack
new exact physical backing/masks and detile into the same expanded RGBA32 image.
Old backing retires by fence; persistent new backing is accounted separately
from transient clear resources. Prepared coverage totals 299 storage-clear API
cases, 24 packed10 warm endpoints, 1,056 cold read-only cases with consecutive
clear values and 1,368 post-writer HLE clear combinations. No execution.
Core/checks pass (`command-20261012-212043-107544.log`, 7.320 s;
`command-20261012-212043-91344.log`, 4.270 s), all ten full-core links refresh.
The 89-group inventory passed (`command-20261012-212146-51176.log`, 21.470 s),
zero semantic closures. Diagnostics/whitespace clean. Packed16 sampled 1D,
BC/MSAA/attachment coverage and full runtime acceptance remain open.
Sampled normalized packed16 now expands native 1D arrays through RGBA32 while
retaining exact physical words/masks, full mip/layer layout and inverse transfer.
The tiler and compatibility checks accept that geometry consistently. Eight new
GPU round-trip API cases and four read-only 1D clear cases are prepared (303
storage-clear API cases total); no execution. Production core/checks pass
(`command-20261012-212609-94456.log`, 4.219 s;
`command-20261012-212609-87468.log`, 2.944 s), and all ten full-core fixture
links refresh in `work/packed16-1d-final-<fixture>.log`. The 90-group inventory
passed (`command-20261012-212651-77224.log`, 21.115 s): 1,479 associations,
13 explicit mappings, zero semantic closures. Diagnostics/whitespace clean.
General 1D ownership, broad queued formats/attachments, nonblocking GPU pressure
and full runtime acceptance remain open. Migration continues after packaging
the user-requested current source snapshot.
User-requested immutable preview nine is installed in the existing desktop
catalogue: `dx12-20261012-full-port-preview-c1c2189e4466`, display name
`DX12 — перенос №9 — 12.10.2026`, SHA256
`c1c2189e4466daa0ccd095954e50a3d3850fa43b98d36bdde005d3352088f49b`.
It includes source through group 90/core 190. Its 76 manifested files, PE
imports, compiler/dependency identities and cumulative dirty patch passed static
verification. Twenty save files were copied/hash-checked from the still-selected
first preview's isolated profile; previous packages/settings/desktop remain
unchanged. Packaging passed (`command-20261012-213055-74428.log`, 36.069 s).
Instructions/provenance are in task `outputs/DX12-preview-9-20261012`.
No tests or game launches occurred. Full migration continues after this snapshot.

Priority image backing commits now accept submitted/in-flight fences. Physical
register/map/unmap-remap/host mapping notifications cancel overlapping queued
generation tickets before any CPU read/resolve/write, preserving replacement
bytes even at the same address. Budget fallback waits for the actual submitted
fence. Four external-GPU-fence cancellation fixtures are prepared; no execution.
Core/checks pass (`command-20261012-213437-105464.log`, 12.591 s;
`command-20261012-213437-74840.log`, 18.121 s), all ten full-core fixtures link.
The 91-group audit passes (`command-20261012-213511-64300.log`, 21.599 s),
1,480 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. These source changes postdate immutable preview nine; source/cache
residency integration and the rest of the full migration continue.
Independent single-sample linear pressure retirement now records/submits without
an unconditional GPU wait. Shared actual-allocation source residency survives
cache erasure until the copy fence, and is included in cache/priority budgets.
Ordinary collection can release source storage while worker CPU commit waits
for the execution mutex; cache exhaustion can wait/collect GPU sources without
draining that worker under the mutex. Retirement-registration failure retains
resources through a fallback fence wait. A prepared actual GPU fixture destroys
the source cache/allocator before signaling its externally gated copy, requiring
exact compact contents/padding and retired byte accounts. No execution.
Core/checks pass (`command-20261012-214149-55972.log`, 11.995 s;
`command-20261012-214149-43960.log`, 20.378 s), all ten full-core links refresh.
The 92-group audit passes (`command-20261012-214239-41560.log`, 21.388 s),
zero semantic closures; diagnostics/whitespace clean. Other ownership/formats,
heap/budget/CPU/worker/device-loss behavior and full runtime acceptance remain
open. These source changes also postdate immutable preview nine.
Legacy linear downloads now promote eligible ordinary storage/color attachment
ownership into the existing independent-image domain and notify from every color
binding, including already-owned MRT reuse. Compact physical CPU writes retain
the preceding raw arena. Actual native backing sample count is checked before
queueing. HLE clears/copy operations alone still do not enqueue downloads;
tiny tiled independent normal resolution is allowed under the original width
at-most-eight eligibility. Pressure GC retains its tiled exclusion.
Four GCN 56x16/64-pitch shared-three-layer MRT permutation/warm cases prepare
deduplicated fence/submit downloads, unchanged raw arena, full padding, empty
fence behavior and same-value CPU store uploading only its 4 KiB page. No
execution. Core/checks pass (`command-20261012-214757-101036.log`, 7.282 s;
`command-20261012-214757-64648.log`, 4.223 s); all ten full-core fixtures link.
The 93-group audit passes (`command-20261012-214819-61788.log`, 21.081 s),
zero semantic closures; diagnostics/whitespace clean. Broader queued formats/
ownership and full GPU/runtime acceptance remain open. Migration continues.
Compact exact packed image downloads now capture canonical GPU guest words and
gather mip-zero texels into the pinned compact array/depth pitch order, avoiding
CPU float quantization. Exact expanded packed formats can enter the supported
legacy queue/normal independent resolver; expanded pressure remains synchronous
until its additional source residency is integrated. Actual readback allocations
and canonical source are retained through the GPU copy fence. Twenty-four compact
API cases prepare signed packed10 and four packed16 formats across 2D/1D/volume
geometry, with fixed packed-word oracles and preserved padding/tail. No execution.
Core/checks pass (`command-20261012-215200-41852.log`, 8.708 s;
`command-20261012-215200-47528.log`, 10.039 s), all ten full-core fixtures link.
The 94-group audit passes (`command-20261012-215249-95184.log`, 21.196 s),
zero semantic closures; diagnostics/whitespace clean. Wider ownership/format/
memory/GPU acceptance remains open, and full migration continues.
All cache admission now uses one overflow-checked residency total including
in-flight pressure sources: ordinary reservation, packed conversion and typed
clear admission share it. New actual sample backings reserve their allocation
before creation, preserving the current entry/bindings. All 24 compact GPU
fixtures prepare source-account inclusion and post-fence zero-retirement checks;
no execution. Core/checks pass (`command-20261012-215513-111024.log`, 8.650 s;
`command-20261012-215513-3060.log`, 14.722 s), all ten full-core links refresh.
The 95-group audit passes (`command-20261012-215638-103528.log`, 21.059 s),
zero semantic closures; diagnostics/whitespace clean. Heap/actual pressure,
sample/format transitions and full GPU/runtime acceptance remain open.
Image prewrite source selection now follows pinned ObtainBufferForImage:
capture a resident GPU arena only when the requested 4 KiB pages have actual
outstanding guest GPU buffer modifications; otherwise stage physical CPU,
including after a compact image download while the clean raw arena stays older.
Sampled/depth snapshots share this predicate. Read-only image/metadata synthesis
does not mark GPU modifications, and exact dirty/download spans are unchanged.
Protected/GPU GCN fixtures prepare false/true/post-download-false checks and a
fresh read-only image alias requiring CPU quarter versus old raw zero before
the next writer, followed by exact CPU raw download/padding. No execution.
Core/checks pass (`command-20261012-220208-109120.log`, 12.234 s;
`command-20261012-220208-69080.log`, 18.171 s), all ten full-core links refresh.
The 96-group audit passes (`command-20261012-220318-35808.log`, 20.952 s),
1,483 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Partial modification/page tracking, deferred physical commit ordering,
broader ownership/geometry and full runtime acceptance remain open.
R32 FLOAT 2/4/8-sample images now accept full-resource native RTV clears from
read-only/storage/render-target cached usages without changing their descriptors.
HLE restores the declared sample backing and can retain the supported independent
buffer/image domain. Unsupported MSAA pressure victims stay excluded. Eighteen
GPU API cases prepare exact quarter/negative-zero bits in every sample/layer,
restricted-view independence, identity and physical padding. No execution.
Core/checks pass (`command-20261012-220909-107940.log`, 7.732 s;
`command-20261012-220909-105948.log`, 5.057 s); all ten full-core fixtures link.
The 97-group audit passes (`command-20261012-221055-112500.log`, 21.724 s),
1,483 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Other formats, HLE/sample transitions, MSAA pressure/compact downloads
and full runtime acceptance remain open. Migration continues.
Sampled-view channel selectors no longer restrict supported physical RTV clears.
Twelve API cases prepare permuted/constant selectors for R32/RGBA32 FLOAT and
RGBA8 UNORM in linear/tiled layouts, all mips/layers/padding and unchanged
resource/descriptor. No execution. Core/checks pass
(`command-20261012-221559-59000.log`, 7.331 s;
`command-20261012-221559-75604.log`, 4.321 s); all ten full-core fixtures link.
The 98-group audit passes (`command-20261012-221629-81520.log`, 20.941 s),
zero semantic closures; diagnostics/whitespace clean. Other view/format/sample
semantics and full runtime acceptance remain open. Full migration continues.
Exact-expanded linear pressure retirement now retains/accounts both the native
texture and canonical packed GPU buffer through the copy fence, using the existing
nonblocking compact path. Six gated cache-destruction cases prepare R32 float,
signed packed10 and four expanded packed16 volumes, exact allocation accounting,
compact words/padding and worker retirement; no execution. Core/checks pass
(`command-20261012-221853-82188.log`, 7.802 s;
`command-20261012-221853-108464.log`, 5.044 s); all ten full-core fixtures link.
The 99-group audit passes (`command-20261012-221921-73640.log`, 21.183 s),
zero semantic closures; diagnostics/whitespace clean. Actual pressure budgets,
physical commit ordering/races and full runtime acceptance remain open.
Writable MSAA clears now use the existing typed UAV sample planes and full
multisample writeback. Clear descriptors/transfer resources are retained by the
pending entry before commands; authoritative sample trackers transition before
temporary clear views are copied. R32 uint/sint/float/raw-float HLE ownership is
accepted for 2/4/8 samples. Twenty-four API cases prepare exact union/negative-zero
bits in every sample/layer, complete padding, restricted views, unchanged native
identity and cache clear-lease retirement. No execution. Core/checks pass
(`command-20261012-222437-112736.log`, 7.469 s;
`command-20261012-222437-74056.log`, 4.405 s); all ten full-core fixtures link.
The 100-group audit passes (`command-20261012-222513-109000.log`, 21.023 s),
1,483 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Actual HLE/GPU contents, broader formats/ownership, MSAA pressure/source
domains and full runtime acceptance remain open. Full migration continues.
User-requested immutable preview ten is installed in the existing desktop
catalogue: `dx12-20261012-full-port-preview-830369606174`, display name
`DX12 — перенос №10 — 12.10.2026`, SHA256
`8303696061747848d147471924a49ec6e6699dec4c5a07a7c23f1671ac0542d8`.
It includes source through group 100/core 201. Its 76 manifested files, PE
imports, compiler/dependency identities and cumulative dirty patch passed static
verification. Twenty save files were copied/hash-checked from the still-selected
first preview's isolated profile; previous packages/settings/desktop remain
unchanged. Packaging passed (`command-20261012-222636-99876.log`, 34.576 s).
Instructions/provenance are in task `outputs/DX12-preview-10-20261012`.
No tests or game launches occurred. Full migration continues after this snapshot.
Ordinary VS-exported PointSize now reaches an auxiliary geometry shader through
a private varying. It expands the completed vertex once into a viewport-sized
strip, forwards parameters/primitive ID/native clip-cull arrays, clips the center
and preserves point fill/culling/front-facing behavior. Runtime specialization
includes the expansion flag; stage records are v5/pipeline metadata identity v7,
and auxiliary GS serialization is supported. Ninety-six actual GPU GCN cases
prepare sizes, viewport flips, center clipping, culling/stencil and full padding;
no execution. Core/checks pass (`command-20261012-223840-70132.log`, 6.079 s;
`command-20261012-223840-106600.log`, 10.889 s), all ten full-core fixtures link.
The 101-group audit passes (`command-20261012-224248-44424.log`, 21.133 s),
1,484 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Point coverage/limits, side effects, routing, GS/tessellation and complete
runtime acceptance remain open. This source work postdates immutable preview ten.
Sized point geometry now forwards the original four-component emulated clip
vector at location zero and shifts ordinary parameter locations accordingly,
using the pinned negative-distance predicate before expansion. Native clip/cull
arrays stay supported. Two additional modes prepare 192 GCN cases with all four
planes initialized, positive/negative distances, sizes, viewport/culling/stencil,
cold/warm and full padding; no execution. Core/checks pass
(`command-20261012-224728-11864.log`, 3.913 s;
`command-20261012-224728-25516.log`, 1.606 s), all ten full-core fixtures link.
The 102-group audit passes (`command-20261012-224759-106240.log`, 21.139 s),
1,485 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Other masks/parameters/routing, GS/tessellation and full runtime acceptance
remain open. These changes also postdate immutable preview ten.
Sized point geometry now carries the completed vertex's integer render-target
layer through a private varying and emits it for every expanded corner. Unit
points without actual PointSize output keep their native layer output. Ninety-six
additional GCN cases prepare routing into a three-layer target, sizes/flips/cull,
clipped centers, cold/warm and unchanged other layers/depth/stencil/padding;
no execution. Core/checks pass (`command-20261012-225052-43580.log`, 4.052 s;
`command-20261012-225052-25460.log`, 3.207 s), all ten full-core fixtures link.
The 103-group audit passes (`command-20261012-225130-43744.log`, 21.188 s),
1,485 associations/13 mappings/zero semantic closures. Diagnostics/whitespace
clean. Combined routing, viewport arrays, GS/tessellation, coverage/limits and
full runtime acceptance remain open. Source work postdates immutable preview ten.
Sized points now forward their integer viewport index through a private varying,
select actual viewport extents from an aligned sixteen-entry runtime table and
emit native GS ViewportIndex. The original 36-byte Mesa prefix stays unchanged;
the complete rounded runtime CBV is bound. Strip orientation follows the selected
viewport's Y flip relative to viewport zero. Ninety-six more GCN cases prepare
different viewport dimensions, mixed flips, front-face/stencil, sizes/culling,
clipped centers, cold/warm and complete padding; no execution. An initial Sirit
Id-to-bool compile failure was corrected. Core/checks pass
(`command-20261012-225735-72184.log`, 3.190 s;
`command-20261012-225710-75404.log`, 10.567 s); all ten full-core fixtures link.
Only existing deprecated-destructor diagnostics remain. The 104-group audit
passes (`command-20261012-225810-54776.log`, 21.098 s), 1,485 associations/
13 mappings/zero semantic closures; whitespace clean. Combined routing, sparse
viewport indices, coverage/limits, GS/tessellation and full acceptance remain open.
These changes postdate immutable preview ten. Full migration continues.
Guest GS PointList output now expands at production EmitVertex using a shared
completed-vertex strip generator. Default/exported size, parameter snapshots,
layer/viewport/distance forwarding, depth/clip conversion, output restoration,
per-point strip termination, native output budget and point culling/fill rules
are integrated. Native geometry controls participate in runtime equality; stage
record v6/pipeline metadata identity v9 reject older records. Prepared coverage
adds 192 GPU cases with actual GCN VS/PS and an explicit SPIR-V GS through the
production emitter, plus triangle-input/point-output state coverage. No execution.
An initial integer-literal overload compile failure was corrected. Core/checks
pass (`command-20261012-230707-103992.log`, 12.033 s;
`command-20261012-230610-101464.log`, 15.105 s), all ten full-core fixtures link.
Only existing deprecated diagnostics remain; whitespace clean. The 105-group
audit passes (`command-20261012-230808-79200.log`, 21.361 s), 1,489 associations/
13 mappings/zero semantic closures. Actual guest GCN GS ring/copy programs,
side effects/invocations, streams/output-budget fallback, edge cases, tessellation
and full runtime acceptance remain open. Immutable preview ten is unchanged.
Another 192 prepared point cases now use actual guest GCN ES/GS/copy programs,
coherent ring stores, two GS emissions and separate one-increment ES/GS atomic
oracles even for clipped output. Token-directed shader metadata is kept beyond
all words. Targeted full-core winding compilation/linking passes
(`command-20261012-231234-57812.log`, 3.362 s;
`command-20261012-231237-89400.log`, 0.465 s), diagnostics clean; no execution.
This extends group 105's preparation, without claiming guest GS acceptance.
Geometry stage metadata now clears borrowed VS-copy spans in temporary runtime
and specialization copies, preserves the live stage/hash/reflection/compiled
code, and rejects nonempty borrowed spans on restore. Stage record v7/native
metadata identity v10 invalidate older records. Actual GCN GS metadata cases
prepare empty restored spans and unchanged live source; no acceptance execution.
Core/checks pass (`command-20261012-231507-66168.log`, 5.519 s;
`command-20261012-231507-68272.log`, 0.829 s), all ten full-core fixtures link;
diagnostics/whitespace clean. The 106-group audit passes
(`command-20261012-231506-95952.log`, 19.579 s), 1,491 associations/13 mappings/
zero semantic closures. New-process/malformed-record/mapping-lifetime acceptance
and broader transient serialized data remain open. Preview ten is unchanged.
Offline shader compilation was added as a build-only path before device creation
and metadata/content oracles. It does not execute GPU checks. It found a NIR
failure for direct VS viewport output and an auxiliary-GS SV_Position validation
failure. The latter came from private locations outside Mesa's varying mask;
those locations were replaced in the following implementation group. These
compiler failures are retained in task work logs, not hidden as acceptance.
Point expansion now transports size/layer/viewport bits together in one private
uint4 at a free generic location below 32. Emulated clip and guest parameter
locations are respected; exhausted interfaces fail explicitly. This fixes the
lost high-location varying and the resulting empty-GS SV_Position rejection.
Metadata identity v11 invalidates older words. Core/checks pass
(`command-20261012-232537-24932.log`, 7.378 s;
`command-20261012-232606-102408.log`, 4.309 s), all ten full-core fixtures link;
only existing deprecated diagnostics, whitespace clean. Nine point chains pass
offline frontend/linked NIR/DXIL/signing; the direct VS viewport baseline still
failed before the next compiler adaptation. The build-only path returns before
hardware creation and skips metadata/content oracles: no GPU acceptance or game
launch. The 107-group audit passes (`command-20261012-232952-99652.log`, 22.305 s),
1,491 associations/13 mappings/zero semantic closures. Generic-slot saturation,
runtime contents, viewport conversion and full equivalence remain open.

Pinned Mesa now accepts direct vertex/domain layer/viewport outputs and applies
per-viewport Y/Z conversion after final output stores: before every GS emission
and at vertex/domain entrypoint exit. Reversed homogeneous depth uses w-z.
The canonical vendor patch and manifest SHA256 were updated through the exact
pinned-source contract. Core/checks pass (`command-20261012-233308-73540.log`,
29.037 s; `command-20261012-233308-98828.log`, 29.150 s), followed by metadata-v12
core 214 (`command-20261012-233705-112656.log`, 5.245 s). All twelve shader
chains compile offline and sign, including two new late-viewport modes with
w=2 and reversed depth (`command-20261012-234155-54644.log`, 0.570 s;
`command-20261012-234155-58972.log`, 0.034 s). Latest winding fixture compiles
and links against core 214; no device/GPU acceptance. The 108-group audit passes
(`command-20261012-234147-22120.log`, 19.492 s), 1,491 associations/13 mappings/
zero semantic closures. Precision, sparse indices, domain-stage contents and
adapter capability gating remain open. Preview ten remains immutable.
Expanded point corners now retain the original viewport window transform while
using same-count 16384-wide native viewports. Center clipping precedes expansion;
the square can cross original viewport edges while original scissors still
apply. The shared VS/GS path applies original Y/depth orientation, viewport
origin, depth min/max and homogeneous w itself; later Mesa flips are disabled
only for expanded points. Runtime prefix ABI is preserved, point mask is at
36, extents/origins at 48 and depth array at 304; rounded CBV is 768 bytes.
Metadata identity v13 invalidates older generated stages. Core 215/checks 169
pass (`command-20261012-234810-45632.log`, 8.790 s;
`command-20261012-234810-5644.log`, 11.641 s). All ten full-core fixtures compile
and link; no new diagnostics (checks retain existing deprecated warnings),
whitespace clean. All thirteen production frontend/linked NIR/DXIL chains sign
offline, including the new 384-case edge preparation
(`command-20261012-235013-3440.log`, 0.034 s). Actual device/GPU contents,
side effects, clipping, depth and padding oracles remain unexecuted. The
109-group audit passes (`command-20261012-234843-101208.log`, 20.260 s),
1,492 associations/13 mappings/zero semantic closures. Subpixel ties, size
limits, MSAA, saturated interface/output budgets and polygon point fill remain
open; full implementation continues.
The requested current core is installed as immutable preview eleven,
`dx12-20261012-full-port-preview-686f45face70`, display name
`DX12 — перенос №11 — 12.10.2026`, including groups through 109/core 215.
Core SHA256 `686f45face70cc9722a4c3c24c23df06c5638831a9bafeb0fa36391c7f42bf81`.
Packaging passes (`command-20261012-235055-24012.log`, 34.989 s): matching
PDB/MAP, cumulative dirty/untracked patch, pinned Mesa/DXC/DXIL provenance,
licenses, static import/dependency validation, 76 manifested files verified,
20 isolated save files copied/hash-checked. Existing desktop/default/settings
and previous versions remain intact. No device/GPU test or game was run.
Full implementation boundary remains open; this is a requested test snapshot.
Ordinary point VS interfaces now use the already-translated fragment's actual
parameter loads and input/default mapping. One shared specialization mask
controls declarations/stores, private misc-slot allocation and auxiliary GS
forwarding. Unconsumed exports are removed without changing guest computation
or buffer/image side effects. Sparse live outputs retain their original
locations; emulated clip output no longer aliases output_params after pruning.
Stage version 8/metadata v14 invalidate older records. Core 216/checks 170 pass
(`command-20261013-000003-18540.log`, 22.902 s;
`command-20261013-000003-43380.log`, 15.275 s). All ten full-core fixtures compile
and link; no new diagnostics, existing deprecated checks warnings, whitespace
clean. Three 96-case GCN modes export all 32 parameters and prepare zero/live
consumer, sparse emulated clip and one VS atomic side effect. An incorrect
new-fixture VINTRP encoding initially caused fail-fast before shader emission;
the actual GCN encoding was corrected. Static inspection also found that
earlier winding compiler preparation did not bind a color target before
frontend translation, so color exports were absent from those shader inputs.
This preparation error is fixed for all modes; it did not change production
color bindings or execute any GPU acceptance. All sixteen chains now sign with
real MRT0 outputs, including sparse live parameters and clip forwarding
(`command-20261013-000558-38388.log`, 0.550 s;
`command-20261013-000559-50948.log`, 0.033 s). Static SPIR-V inspection confirms
the matching VS/GS/PS parameter locations, exactly one VS atomic instruction
and four GS emit instructions; this is compilation evidence, not runtime proof.
The 110-group audit passes (`command-20261013-000055-14612.log`, 20.200 s),
1,493 associations/13 mappings/zero semantic closures. Runtime masks/caches,
genuinely full live interfaces and other point/GS limits remain open. This
later code is excluded from immutable preview eleven; migration continues.
Actual native-expanded point GS now uses the same fragment liveness mapping in
its runtime specialization/equality. Declarations, parameter stores and per-emit
snapshots omit unconsumed copy outputs; budget admission counts live parameters
plus actual position/layer/viewport/clip/cull components. Guest GS execution and
side effects are retained. Stage v9/metadata v15 invalidate older layouts/code.
Two new 96-case real GCN ES/GS/copy preparations export 32 float4 parameters and
emit two points, with no reader or one sparse reader. COPY scalar offsets handle
values outside MUBUF's 12-bit immediate; metadata is at word 768 in 1024-word
arrays. Core 217/checks 171 pass (`command-20261013-001108-15428.log`, 22.829 s;
`command-20261013-001108-75064.log`, 15.254 s), all ten full-core fixtures compile
and link; no new diagnostics, existing deprecated checks warnings, whitespace
clean. All eighteen chains compile/sign offline with MRT0, including the 32-
parameter actual GS (`command-20261013-001255-54268.log`, 0.036 s). Static
SPIR-V confirms one ES and one GS atomic instruction, eight point-corner emits
and the required live GS-to-PS parameter location. This is not hardware proof.
The initial registry audit rejected an incorrect upstream pass path/name;
these references were corrected to the actual RingAccessElimination source.
The final 111-group audit passes (`command-20261013-001350-48096.log`, 21.883 s),
1,493 associations/13 mappings/zero semantic closures. Genuinely high native GS
budgets still need a capture fallback without replaying guest code. Streams,
MSAA, full live interfaces, caches and actual contents/side effects remain open.
Immutable current preview twelve is installed as
`dx12-20261013-full-port-preview-84ded17582dc`, display name
`DX12 — перенос №12 — 13.10.2026`, including groups through 111/core 217.
SHA256 `84ded17582dcc7364c0c1cdbe65c53ce7e0257e2f985cb254bc3b19b81ea64dc`.
Packaging passes (`command-20261013-001639-100744.log`, 35.908 s): matching
PDB/MAP, complete cumulative source patch, exact compiler provenance/licenses,
static dependencies/imports, all 76 manifested files and 20 copied isolated
saves verified. Installed desktop/default/settings and previous packages are
preserved. Runtime/GPU/game verification remains unrun; full migration continues.
Native-expanded point GS now bounds its output declaration by the sum of
EmitVertex instructions in structured syntax blocks when there is no Loop or
Repeat. This conservative acyclic bound is capped by the original declaration;
looping shaders retain the original maximum. Guest runtime/copy strides,
invocations, side effects and emission order remain unchanged. Zero emissions
use a legal positive declaration without adding vertices. Metadata v16
invalidates older compiled code; stage v9/pipeline v4 are unchanged. Core 218 /
checks 172 build (`command-20261013-002324-95452.log`, 5.304 s;
`command-20261013-002325-52920.log`, 0.797 s), all ten full-core fixtures compile
and link, no unexpected diagnostics, whitespace clean. Three additional
96-case preparations cover maxima 128/64 with two actual emits and a real
scalar-controlled loop with maximum 64. An incorrect new-fixture S_MOV_B32
encoding initially caused decoder fail-fast; CPU compiler logging identified
it and the original GCN opcode was corrected. All 21 MRT shader chains now
compile/sign offline. Static SPIR-V confirms eight declared/emitted corner
vertices for both acyclic modes and 256 declared vertices with one loop and
four lexical emits for the looping mode
(`command-20261013-002838-62800.log`, 0.554 s). Each actual ES/GS retains one
atomic instruction. This proves compilation structure, not GPU behavior.
The 112-group audit passes (`command-20261013-003218-70036.log`, 21.093 s),
1,494 associations/13 mappings/zero semantic closures. Genuine large emission
or live component budgets still require a capture fallback without replaying
guest shader side effects; all hardware acceptance remains deferred. Full
migration continues; preview twelve remains immutable and excludes group 112.
Immutable preview thirteen is installed as
`dx12-20261013-full-port-preview-9482de72edc6`, display name
`DX12 — перенос №13 — 13.10.2026`, including groups through 112/core 218.
SHA256 `9482de72edc6d18f6fe36e4137a98a7dd7a8f11b20d89064acc37aadd0d47934`.
Packaging passes (`command-20261013-003331-54032.log`, 35.618 s): all 76
manifested files, static PE dependencies/imports, matching PDB/MAP and exact
cumulative source/compiler provenance verified. Twenty saves were copied and
hash-checked in a separate profile. Installed desktop/default/settings and
previous packages are preserved; no runtime/GPU/game acceptance was performed.
Actual guest point GS without a reflected PointSize export now emits one
hardware unit point per original emission, rather than four strip corners.
Real viewport/depth ranges and Mesa Y/Z flip masks are retained. Native point
liveness and depth/clip-disable conversion remain; output snapshots are
restored after emission without replaying guest shader code. The common
point-output mode typo inherited from the original emitter (operandless
OutputVertices instead of OutputPoints) was found by static SPIR-V inspection
and corrected. The initial offline converter accepted that malformed mode;
final artifacts explicitly contain OutputPoints plus a separate vertex count.
Metadata v17 invalidates older code; stage v9/pipeline v4 are unchanged.
Core 219/checks 173 build (`command-20261013-003652-54628.log`, 7.457 s;
`command-20261013-003652-53492.log`, 4.432 s); core 220 includes the mode
correction (`command-20261013-003850-30764.log`, 4.028 s). All ten full-core
fixtures compile/link against final objects; 24 MRT chains sign offline.
Three additional 96-case actual GCN ES/GS/copy preparations cover unit
declarations 256, 128 with a live parameter and a loop declaration 256.
Static SPIR-V shows two emissions/bound two in the acyclic modes, one lexical
emission/one loop/bound 256 in the loop, matching live GS/PS locations and one
ES/GS atomic instruction. DXC disassembly confirms point output topology and
maximum 256 (`command-20261013-004036-45400.log`, 0.020 s). No GPU oracle ran.
Final audit 113 passes (`command-20261013-003955-71436.log`, 21.510 s),
1,495 associations/13 mappings/zero closures. No unexpected diagnostics,
17 existing deprecated checks warnings, whitespace clean. Hardware unit
footprints/front-face/clip/MSAA, cache/runtime recreation and variable-size
large-output capture remain open. Preview thirteen is immutable and excludes
these later changes; full migration continues.
Point fragment inputs and actual point GS outputs now retain only the prefix
through the highest consumed component, instead of always declaring float4.
GS stores, per-emission snapshots and output-budget checks use the same width;
multiple consumers take the maximum prefix and per-vertex array interpolation
conservatively retains four. Logical component positions, copy/ring strides,
guest execution and side effects remain unchanged. Fragment mode and GS widths
participate in specialization equality; stage v10/metadata v18 invalidate older
runtime layouts/code, pipeline v4 stays unchanged. Core 221/checks 174 build
(`command-20261013-004641-28560.log`, 22.980 s;
`command-20261013-004641-28944.log`, 15.411 s). All ten final full-core fixtures
compile/link; all 27 MRT chains compile/sign offline. Three new 96-case real
GCN loop preparations read x/z/w of distinct parameter components with maxima
48/36/32. Static SPIR-V shows matching GS/PS widths 1/3/4, declared corner
counts 192/144/128 and budgets 960/1008/1024, retaining one ES/GS atomic and
one loop/four lexical emits. DXC disassembly confirms actual PS load columns
0/2/3 and correct GS value stores; printed PS Used masks are not substituted
for instruction-level component evidence. This is compilation, not GPU proof.
Audit 114 passes (`command-20261013-004726-2716.log`, 19.913 s),
1,496 associations/13 mappings/zero closures. Core has no diagnostics; checks
have 294 existing deprecated warnings, no unexpected diagnostics; whitespace
clean. Sparse component packing, full live interfaces/genuine excessive output
capture and all runtime/GPU acceptance remain open. Preview thirteen remains
immutable and excludes these later changes; full migration continues.
Actual point GS-to-PS transport now packs sparse consumed components in their
original channel order, with matching masks/index maps on GS stores and PS
reads. Multiple consumers share the union mask; per-vertex interpolation
retains all four channels. Ordinary VS/auxiliary points retain prefix transport.
GS snapshots/budgets use the same popcount widths. Guest calculations, atomics,
emission order and copy/ring strides are unchanged. Mask/compaction equality
and stage v11/metadata v19 reject older layouts/code; pipeline v4 stays unchanged.
Core 222/checks 175 build (`command-20261013-005256-43624.log`, 22.904 s;
`command-20261013-005256-45564.log`, 15.035 s), all ten final fixtures compile
and link; 28 MRT chains compile/sign offline. The w-only real-loop declaration
is increased to 48; a new 96-case x+w loop declares 42 guest vertices and
prepares distinct color/counter/stencil/padding oracles. Static GS/PS widths are
1/1/1/2 for x/z/w/x+w, corner declarations 192/144/192/168 and budgets
960/720/960/1008. DXC instructions confirm original x/z/w values .125/.5/.75
are stored into the correct compact channels and PS x+w loads columns 0/1;
one ES/GS atomic, one loop and four lexical corner emits remain. This is static
compilation evidence, not runtime contents/side-effect proof.
Audit 115 passes (`command-20261013-005332-65220.log`, 20.012 s),
1,499 associations/13 mappings/zero semantic closures. No core diagnostics,
294 existing deprecated checks warnings, no unexpected fixture/compiler
diagnostics, whitespace clean. Actual aliased/interpolated consumers, runtime
cache recreation, fully live interfaces, genuinely excessive GS output capture,
streams/MSAA and hardware acceptance remain open. Full migration continues;
preview thirteen remains immutable and excludes groups 113–115.
Immutable preview fourteen is installed as
`dx12-20261013-full-port-preview-22c3d89cfb54`, display name
`DX12 — перенос №14 — 13.10.2026`, including groups through 115/core 222.
SHA256 `22c3d89cfb54185664ed1f969a5c50ec3afb15b705106ffe95b3edbf24760506`.
Packaging passes (`command-20261013-005548-25088.log`, 34.702 s): all 76
manifested files, static PE dependencies/imports, matching PDB/MAP and exact
cumulative source/compiler provenance verified. Twenty saves copied and
hash-checked in a separate profile; installed desktop/default/settings and
previous packages preserved. No runtime/GPU/game acceptance was performed;
full migration continues.
Actual point GS compact fragment inputs now share one declaration when
multiple consumed guest inputs have the same source parameter and identical
primary/auxiliary interpolation. Each logical input retains its component
index map; default/unconsumed inputs cannot supply a shared declaration.
The initial alias preparation signed offline but revealed two Location 0
variables lowering to TEXCOORD0/TEXCOORD1 while GS only produced TEXCOORD0.
Final artifacts have one matched producer/consumer semantic and correct x/w
loads from compact columns 0/1. Metadata v20 invalidates old code; stage v11 /
pipeline v4 are unchanged. Core 223/checks 176 build
(`command-20261013-005910-16244.log`, 5.587 s;
`command-20261013-005910-63220.log`, 3.266 s), all ten final fixtures compile
and link. Two new 96-case real GCN loop preparations cover aliased x/w and
a default input preceding a real w reader. The initial default preparation
used V_INTERP_MOV, whose original frontend path does not handle defaults,
causing fail-fast before shader artifacts. It now uses the original P2 default
path. All 30 chains sign offline, including the corrected default case
(`command-20261013-010127-72672.log`, 0.035 s). Static DXIL confirms matching
GS/PS scalar transport for the default case and preserved values/effects
instructions; GPU contents and effects oracles remain unrun.
Final audit 116 passes (`command-20261013-010348-68564.log`, 20.084 s),
1,499 associations/13 mappings/zero semantic closures; no unexpected
diagnostics, whitespace clean. Different interpolation qualifiers sharing one
source, full live interfaces/genuine excessive output capture, caches/streams/
MSAA and hardware acceptance remain open. Preview fourteen remains immutable
and excludes these later changes; full migration continues.
Actual point GS/PS now derives a shared qualified transport plan: equal
source/interpolation inputs share packed output masks, while different
qualifiers get separate proxy outputs in free locations reserved away from
other live source parameters. The first group keeps the original location.
Guest stores fan out only to matching proxy channels; computations, atomics,
copy/ring layout and emission order are retained. Output budgets count all
groups, including repeated channels. Fragment cache comparisons derive the
same plan before equality; saved maps/outputs use stage v12/metadata v21,
pipeline v4 unchanged. Missing source outputs and clip-emulation combinations
remain explicit open contracts. Core 224/checks 177 build
(`command-20261013-011139-66584.log`, 23.070 s;
`command-20261013-011139-43456.log`, 15.444 s), all ten full-core fixtures compile
and link; 31 MRT chains compile/sign offline. One additional 96-case GCN loop
prepares flat x and perspective w from parameter 0, preserving FrontFace around
barycentric input registers. Static SPIR-V has matched scalar locations 0/1,
Flat/default-smooth decorations and 168 corner vertices/budget 1008. DXIL
confirms matching TEXCOORD0/1 with nointerpolation/linear modes, intended
values and reads, one GS/ES atomic and four lexical corner emits; guest code
is not replayed. This remains compilation evidence, not GPU proof.
Audit 117 passes (`command-20261013-011237-64888.log`, 20.030 s),
1,499 associations/13 mappings/zero semantic closures. No core diagnostics,
294 existing deprecated checks warnings, no unexpected diagnostics, whitespace
clean. Host-slot reservation/order, all interpolation/clip/signature limits,
runtime/cache acceptance and genuine large-output capture remain open.
Preview fourteen remains immutable and excludes these changes; migration
continues.

Source group 118 adds original signed per-viewport transforms to immutable
graphics runtime constants for native clip-disabled conversion. Mesa's 36-byte
prefix and existing point fields are retained; 16 float4 records begin at byte
560, runtime size is 816/CBV 1024. Native VS exit and point GS emission select
the exported viewport index, including the VS private point misc output, and
retain original conversion arithmetic, W and depth. Original hardware viewport
replacement no longer loses the selected signed raw transform. Profile gating
preserves the other backend; stage v13/metadata v22 reject older records,
pipeline v4 unchanged. Core 225/checks 178 pass
(`command-20261013-012456-63468.log`, 14.382 s;
`command-20261013-012456-60660.log`, 14.110 s), all ten full-core fixtures
compile/link. Thirty-five chains compile/sign offline, including three new
96-case sized VS/sized GS/unit GS clip-disabled viewport preparations and
the reserved-slot mixed-interpolation case. Initial sized-GS DXIL revealed
that its fixture counter overwrote the viewport in v9; the counter now uses
v10, final DXIL reads the actual offset-20 raw input for index selection.
Static code confirms byte 560 plus 16 times min(index,15), original signed
transform arithmetic, GS/ES atomics unchanged and sized/unit topologies.
Reserved-slot SPIR-V/DXIL confirms matched locations/semantics 0/1/2 with
flat x from parameter 0, flat x from parameter 1 and smooth w from parameter
0, respective values .125/.5/.75 and budget 1008. This proves compilation
and this static interface case only. Audit 118 passes
(`command-20261013-012730-58108.log`, 19.678 s), 1,500 associations/13 mappings/
zero closures. Across 68 final commands, 323 existing deprecated checks
warnings, zero unexpected diagnostics and clean whitespace. Hardware/runtime/
cache, W/depth/window-offset boundaries, nonpoint GS/tessellation conversions,
genuine output capture and all full-port equivalence remain open. Preview
fourteen is immutable; the requested next snapshot is being packaged and
full migration continues without running tests or games.

User snapshot fifteen is installed in the existing PS4Desktop catalogue as
`dx12-20261013-full-port-preview-499eb21d6c50`, display
`DX12 — перенос №15 — 13.10.2026`, with all current changes through 118/core 225,
SHA256 `499eb21d6c50132c80368002c706d5987226a46fab792b29f8c46c3f2c5ecbea`.
Static hashes/imports/dependencies pass for all 76 manifested files; 20 save
files are copied/hash-checked in an isolated profile. Earlier versions and
the selected default are retained. Packaging passes
(`command-20261013-012912-43340.log`, 36.460 s). The initial attempt stopped
before staging on a summary-log JSON parse; summary logs are excluded from
the per-command glob. This snapshot remains experimental/incomplete, with
no tests or game launches and no semantic closures. Full migration continues.

Source group 119 keeps actual sized point GS clip/cull distances in initialized
private arrays. Original distance stores use private pointers and all loaded
values participate in center rejection before corner emission; no redundant
distance output/restoration is required. Unit-point clipping retains the
hardware builtin interface/budget. Four new 96-case GCN loop preparations
cover clip-only, cull-only, combined clip/cull and unit clip control. Sized
cases declare 64 guest vertices, compile with 256 corner vertices and a
position-only output budget of 1024; unit control declares/emits at most 64
hardware points. DXIL keeps all eight distinct combined distance comparisons,
their actual raw loads, one GS/ES atomic and one guest loop; sized output
signatures have only Position, while unit control retains ClipDistance.
Stage v14/metadata v23, pipeline v4. Core 227/checks 180 pass
(`command-20261013-013754-107192.log`, 3.311 s;
`command-20261013-013754-88648.log`, 0.795 s), all ten full-core fixtures
compile/link and 39 chains compile/sign offline. Initial checks 179 had 45
existing deprecated warnings, zero unexpected; 72 final commands have no
diagnostics and whitespace is clean. Audit 119 passes
(`command-20261013-013806-90244.log`, 19.795 s), 1,500 associations/13 mappings/
zero closures. An attempted auxiliary GS output removal was reverted;
static review then proved a pre-existing VS ClipDistance / GS CullDistance
input mismatch even with its old outputs. Linked Mesa leaves input_clip_size
unset; the next compiler correction must supply the previous stage's clip
length. Genuine large varying/capture limits, unit combined distances,
hardware/cache/runtime/clipping acceptance remain open. These changes are
excluded from immutable preview fifteen; full migration continues.

Source group 120 supplies each linked DXIL stage's actual producer clip array
length to nir_to_dxil.input_clip_size. Stage-identity lookup handles missing
intermediate stages and snapshots lengths before compilation mutations;
compute input length stays zero. The complete old Mesa Git diff/hash was
verified and generated source restored before updating the canonical source
contract: pinned b628375f, patch SHA256
`d4d16e7054beb94acdf45beeb7e68b5380ac391f2aa189f01970d2ebeaf18b2b`,
48,144 bytes; linked header/ABI/features unchanged. Stage v15/metadata v24,
pipeline v4. Core 228/checks 181 pass
(`command-20261013-014204-61148.log`, 31.469 s;
`command-20261013-014204-25872.log`, 26.778 s), all ten final full-core fixtures
compile/link, 40 MRT chains compile/sign offline. One new 96-case native cull
preparation complements native clip. Final DXIL proves exact matching VS
outputs/GS inputs: ClipDistance0/1 for the clip case, CullDistance0/1 for cull,
same float4 masks/types. Earlier emulated/generic, actual-GS private clip/cull
and unit clipping chains still sign. Across 73 final commands, no C++/link
errors; 20 reviewed Meson dependency configuration warnings concern CMake
compiler-state/generator-expression recognition and add_languages native
selection, with successful pinned MSVC compilation/linking. Two -Wno-error
capability probe lines are recorded separately. Full diagnostics retained,
whitespace clean. Audit 120 passes
(`command-20261013-014246-91620.log`, 19.841 s), 1,500 associations/13 mappings/
zero closures. Source-only linkage evidence is not runtime acceptance;
combined clipping limits, tessellation/array cases and full equivalence stay
open. Preview fifteen remains immutable; migration continues.

Source group 121 densely transports active sized-VS point clip/cull components
to its auxiliary GS. Original ascending component order and values are
preserved by matched store-index compaction and input array lengths; all
transported values still reject the original point center. Corrected producer
clip sizes allow the auxiliary GS to omit redundant native distance outputs
without input semantic changes. Emulated generic clip, unit/nonpoint
interfaces and actual-GS private arrays retain their existing paths. Two new
96-case preparations cover Clip4/Cull4 and sparse Clip0/2 plus Cull5/7.
Final DXIL has matched VS/GS Clip4/Cull4 or Clip2/Cull2 signatures, no GS
distance outputs, and original sparse loads at byte offsets 20/28/40/48
(.125/.375/.75/1 prepared values) feeding all four center comparisons.
Sparse Clip xy and Cull zw share one physical signature row; all eight full
values remain distinct. Stage v16/metadata v25, pipeline v4. Core 229/checks
182 pass (`command-20261013-014730-104884.log`, 7.646 s;
`command-20261013-014730-105932.log`, 4.736 s), all ten final full-core fixtures
compile/link, 42 MRT chains compile/sign offline. Seventy-five final commands
have 45 existing deprecated checks warnings, no unexpected diagnostics,
whitespace clean. Audit 121 passes
(`command-20261013-014800-68548.log`, 19.842 s), 1,500 associations/13 mappings/
zero closures. Hardware/runtime/cache/MSAA/NaN/boundaries, unit combined
distance limits and genuine large varying/capture remain open. Preview
fifteen remains immutable; the next user snapshot is being packaged and
full migration continues.

User snapshot sixteen is installed in the existing PS4Desktop catalogue as
`dx12-20261013-full-port-preview-b423844202bf`, display
`DX12 — перенос №16 — 13.10.2026`, with all changes through 121/core 229,
SHA256 `b423844202bf4b5fd77e0826f3282290763480dfb19d4342e0180e15d765ff15`.
Static hashes/imports/dependencies pass for all 76 manifested files; 20 save
files are copied/hash-checked in an isolated profile. Older versions and the
selected default are retained. Packaging passes
(`command-20261013-015112-75952.log`, 34.845 s). Experimental/incomplete,
no tests/game launches or semantic closures; full migration continues.

Source group 122 densely emits active actual unit-point GS clip/cull values,
retaining hardware clipping and point topology. Declarations, store indices,
snapshots/restoration and component budget all use matched actual counts;
inactive initialized-zero distances are omitted. Sized actual GS private
checks, sized VS transport and nonpoints retain their paths. Two new 96-case
GCN loop preparations cover Clip4/Cull4 and sparse Clip0/2,Cull5/7, 64 declared
points/budgets 768 or 512. Unit clip-only now uses four active distances and
budget 512. Static DXIL has native Clip4/Cull4 or Clip2/Cull2 semantics, sparse
xy/zw packed row, original byte-offset 20/28/40/48 values, unchanged one ES/GS
atomic, one guest loop and one lexical point emit. Stage v17/metadata v26,
pipeline v4. Core 230/checks 183 pass
(`command-20261013-015354-82884.log`, 5.697 s;
`command-20261013-015354-63192.log`, 3.344 s), all ten final full-core fixtures
compile/link, 44 MRT chains compile/sign offline. Seventy-seven final commands
have zero diagnostics, whitespace clean. Audit 122 passes
(`command-20261013-015436-109188.log`, 19.583 s), 1,500 associations/13 mappings/
zero closures. Hardware/runtime/cache/NaN/MSAA/clipping-boundary and full
generic varying/capture acceptance remain open. Immutable preview sixteen
excludes 122; full migration continues.

Source group 123 applies the shared qualified point-parameter transport to
ordinary VS points, both sized and native unit points. Runtime SwVS outputs,
VS declarations/stores, auxiliary GS forwarding and free misc-location
selection agree on actual host masks; aliased x/w become one vec2 while
flat-x/linear-w use separate scalars. Synthetic emulated clip inputs retain
original component indexing. Missing exports remain undefined and full
varying packing/capture acceptance remains open. Three new 96-case
preparations preserve one VS atomic, values .125/.75 and original PS
interpolation. Static VS/GS/PS signatures match; sized paths have four corner
emits, unit VS has no GS, and synthetic clipping retains columns 0/1/2/3.
Stage v18/metadata v27, pipeline v4. Core 231/checks 184 pass
(`command-20261013-020530-14912.log`, 22.982 s;
`command-20261013-020530-42008.log`, 15.471 s), all ten final full-core
fixtures compile/link, 47 MRT chains compile/sign offline. Eighty-nine final
commands retain 294 existing deprecated warnings and zero unexpected
diagnostics, whitespace clean. Audit 123 passes
(`command-20261013-021024-42248.log`, 21.395 s), 1,501 associations/13 mappings/
zero closures. Hardware/runtime/cache/precision/MSAA, all qualifiers and full
varying/capture acceptance remain open. Preview seventeen is being packaged;
full migration continues.

User snapshot seventeen is installed as
`dx12-20261013-full-port-preview-95d73764dd3d`, display
`DX12 — перенос №17 — 13.10.2026`, with all changes through 123/core 231,
SHA256 `95d73764dd3d3e277539a8a11376457f78a775a242584d386e2a68abe071da93`.
All 76 manifested files/hashes/imports/dependencies pass static checks; 20 save
files are copied/hash-checked in an isolated profile. Existing default and
older packages remain. Packaging passes
(`command-20261013-021204-56080.log`, 35.117 s). Experimental/incomplete, no
tests/game launches or semantic closures; full migration continues.

Source group 124 packs different live point sources with identical primary/
auxiliary interpolation into contiguous components of shared host float4
locations. Source/qualifier mask union precedes packing; producer fanout and
PS offset-plus-rank reads retain original values. VS/GS declarations, private
misc allocation, auxiliary forwarding and actual-GS snapshots derive matched
host widths. PerVertex retains full records and qualifiers never mix within
one variable. Initial sized 32-scalar and 31-scalar-plus-clip chains fail free
slot allocation; initial unit chain terminates offline with 0xc0000409.
Three new 96-case preparations now compile/sign, using distinct source/channel
values `(4*source+channel+1)/256`. Static proof verifies every 32/31 source
store and 32/35 PS reads, eight packed rows, four auxiliary passes per
component, clip columns, one VS atomic and prepared green values 64/62.
Existing actual GS reserved/mixed case packs two flat sources into one vec2,
keeps smooth separate and retains one GS atomic. Stage v19/metadata v28,
pipeline v4. Core 232/checks 185 pass
(`command-20261013-021620-101664.log`, 23.181 s;
`command-20261013-021620-82556.log`, 15.651 s), all ten full-core fixtures
compile/link, 50 MRT chains compile/sign offline. Ninety-four final commands
retain 294 existing deprecated warnings and zero unexpected diagnostics,
whitespace clean. Audit 124 passes (`command-20261013-021727-94836.log`,
21.632 s), 1,501 associations/13 mappings/zero closures. True full signature/
large-GS capture, all qualifiers/runtime/cache/MSAA/precision and game
acceptance remain open. Immutable preview seventeen excludes 124; migration
continues.

Supplemental 124 verification adds three 96-case 32-source preparations with
alternating flat / smooth center, centroid or sample inputs. All values and
32 PS reads still match eight packed rows and four auxiliary forwards per
component; one VS atomic remains. DXIL rows alternate nointerpolation with
linear/centroid/sample, and only sample enables SampleFrequency=1. Original
front face is preserved in v5 while original interpolation uses v0. All 53
chains compile/sign offline, latest winding fixture compiles/links
(`command-20261013-022038-70548.log`, 3.670 s;
`command-20261013-022042-62324.log`, .442 s). Six new packed preparations total
576 unexecuted GPU cases. Final combined evidence records 109 successful
commands, 294 existing deprecated warnings and zero unexpected diagnostics.
Preview eighteen is being packaged; full migration continues.

User snapshot eighteen is installed as
`dx12-20261013-full-port-preview-519d8356217d`, display
`DX12 — перенос №18 — 13.10.2026`, with all changes through 124/core 232,
SHA256 `519d8356217d04c9b8c12251bbd1efc014ac4f86d8325e08d3eca7614a180f87`.
All 76 manifested files/hashes/imports/dependencies pass static checks; 20 save
files are copied/hash-checked in an isolated profile. Existing default and
older packages remain. Fresh audit 124 passes
(`command-20261013-022241-110536.log`, 19.089 s), 1,501 associations/13 mappings/
zero closures. Packaging passes (`command-20261013-022324-38672.log`, 34.982 s).
Experimental/incomplete, no tests/game launches; full migration continues.

Source group 125 adds an opt-in ordered point-GS capture producer, still
disabled in production. Raster outputs become private and original emissions
write fixed primitive/invocation/emission slots in raw u0 space33. The shared
152-word/608-byte ABI retains raw position/size/layer/viewport, original eight
clip/cull indices and all 32 source float4 records. Ordinal saturation prevents
wraparound; no global append, shader replay or hardware vertex emission.
GS invocation input is also declared for original IR that reads it. CPU plan
checks dimensions/ranges and caps DXIL 32-bit BYTE addressing; bounding only
SPIR-V words was insufficient and is corrected. Mesa can merge valid markers
with payload, so consumers must wait for the whole producer and barrier.
Wide fixture ring stores now use literal scalar offsets beyond 4095 instead
of overflowing the 12-bit immediate. Five ES/GS producer chains compile/sign:
256-declared loop x/w, sized/unit clip+cull, all 32 copy sources, and two GS
invocations. Static SPIR-V/DXIL retains one ES atomic, one GS atomic, original
loops and primitive/invocation addressing, zero raster outputs/vertex emits;
full 32-source records contain all 128 values per emission. No producer
runtime/PS execution. Stage v20/metadata v29, pipeline v4. Core 233/checks 186
pass; private pointer corrections pass core 234/checks 187. Final core 235 /
checks 188 pass (`command-20261013-023717-99000.log`, 3.962 s;
`command-20261013-023717-23132.log`, 3.319 s). All ten full-core fixtures
compile/link; 53 ordinary and five producer chains sign. Latest winding
compile/link passes (`command-20261013-023909-46704.log`, 3.758 s;
`command-20261013-023913-93116.log`, .458 s). Final combined 104 commands have
zero diagnostics, whitespace clean. Audit 125 passes
(`command-20261013-023516-72604.log`, 21.650 s), 1,501 associations/13 mappings/
zero closures. Replay, bounds for instances/indirect/tessellation/streams,
allocation/clear/barriers/fences/pressure and all runtime/game equivalence
remain open; next implementation is replay with flat record id and PS raw
attribute fetch to avoid forwarding 128 values through hardware signatures.
Immutable preview eighteen excludes 125; full migration continues.

Emergency stop / source group126 — 2026-10-13:
captured FS receives one flat uint record ID and reads original parameter/clip
indices. Readonly StorageBuffer initially signed as raw UAV u0/space33 under
pinned Mesa; SRV static proof failed. Latest sampled uint Buffer/OpImageFetch
patch intends typed R32_UINT SRV t0/space33 with152-word addressing. Core237 /
checks190 compile/link pass (`command-20261013-025120-27688.log`,5.381s;
`command-20261013-025120-107444.log`,4.613s). Stage21/pipeline4/metadata30.
Fresh fixture signing/resource proof is pending: existing winding/artifacts are
core236 raw-reader versions. Before this final patch all ten fixtures compiled/
linked and54 ordinary +6 producer/fragment chains signed offline. Audit126
passed (`command-20261013-024807-53592.log`,21.585s),1,501 associations/13 mappings/
zero closures. Capture replay/runtime/allocation/clear/barriers/lifetimes and
qualifier/precision/coverage equivalence are not implemented or accepted.
No source completion/game/FPS claim. Preview18 excludes125/126. At the user's
emergency request development stopped; no new builds/tests/profile/game runs.
Source WIP c0aa3e4 and full residual patch/archive are preserved. Read
../SESSION_HANDOFF.md and external CHECKPOINT_FINAL.json before continuing;
do not repeat old audits or apply patches onto the already-dirty current tree.

2026-10-13 resumed development: groups127/128 register ordered point capture/replay
and placed-color MSAA transfer initialization. Source WIP 0a3571a4b76bf1a6683a47f1ee409fe2555614a3; Stage23/metadata-v32.
Six final offline chains,864 point GPU cases verified; full native suite174/227
before final MSAA init fix,53 remaining failures. Later targeted results in SESSION_HANDOFF.md.
No new overall audit/semantic closure; no current Bloodborne world/FPS acceptance.
