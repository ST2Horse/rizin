# A first route through the Rizin codebase

This guide is for contributors who can build Rizin but do not yet know where a
behavior lives. Follow one small command through the libraries before exploring
the whole call graph. You do not need to understand every field of `RzCore`, every
binary format, or every architecture plugin to make your first change.

Keep [the contributor guidelines](../CONTRIBUTING.md),
[the build instructions](../BUILDING.md), and
[the developer conventions](../DEVELOPERS.md) nearby. This guide explains how to
read the code; those documents cover building, contribution requirements, and
coding style.

## 1. Start with a two-byte input

Use a locally built and installed `rizin`, including its installed data files.
Run the following in a shell:

```sh
rizin -N -a x86 -b 64 -e scr.color=0 hex://90c3
```

`hex://90c3` gives the I/O layer two bytes without creating a file. `-N` skips
user settings and scripts, and the architecture and bitness are explicit. At the
Rizin prompt, enter:

```text
s
px 2
pxj 2
pd 2
pdq 2
aoj 2
afl
q
```

Look for these facts, rather than copying the spacing of the output:

- `s` reports offset `0x0`.
- `px 2` shows the bytes `90c3`; `pxj 2` prints `[144,195]`.
- `pd 2` and `pdq 2` decode `nop` at address 0 and `ret` at address 1.
- `aoj 2` returns two instruction records with size 1 and types `nop` and `ret`.
- `afl` is empty: decoding instructions has not created analyzed functions.

Here is a compact, noninteractive version with stable output:

```sh
rizin -N -q -a x86 -b 64 -e scr.color=0 -c 'pxj 2; pdq 2' hex://90c3
```

```text
[144,195]
0x00000000 nop
0x00000001 ret
```

For this first pass, skip automatic analysis, debugger startup, and visual mode.
The small input keeps the command path visible without needing to study
format-specific loader details or an analyzed function graph.

## 2. Find the application and the objects it coordinates

Read these entry points in order:

1. [binrz/rizin/rizin.c](../binrz/rizin/rizin.c): `MAIN_NAME` is the executable
   wrapper. Follow its ordinary `rizin` branch to `rz_main_rizin`; skip the
   separate `rzp` pipe wrapper initially.
2. [librz/main/rizin.c](../librz/main/rizin.c): `rz_main_rizin` handles startup,
   options, input opening, and the command session. Find its `rz_core_new` call;
   you can skip the many platform and option branches on your first reading.
3. [librz/include/rz_core.h](../librz/include/rz_core.h): find `struct rz_core_t`.
   Locate `io`, `bin`, `rasm`, `analysis`, `config`, `cons`, `rcmd`, `offset`, and
   `blocksize`. Treat this as a map of collaborators, not a struct to memorize.
4. [librz/core/core.c](../librz/core/core.c): compare `rz_core_new`,
   `rz_core_init`, `rz_core_fini`, and `rz_core_free`. Initialization constructs
   and connects the subsystems, including command registration; finalization
   shows their cleanup responsibilities.

`RzCore` coordinates objects it owns or references. Do not infer exclusive
ownership merely because a pointer is a core field: for example, `RzCons` uses
reference-counted singleton creation and release. Read the relevant constructor,
destructor, and API contract when retaining or freeing an object.

The following division of work will help you decide where to look:

| Object or library | Responsibility | First source to inspect |
| --- | --- | --- |
| RzCore | Session state, command handlers, and coordination of lower-level APIs | [core/core.c](../librz/core/core.c), [core/cmd/](../librz/core/cmd/) |
| RzIO | Open resources and read or write bytes through descriptors and address maps | [I/O concepts](../librz/io/README.md), [io/io.c](../librz/io/io.c) |
| RzBin | Interpret file formats: sections, symbols, imports, and entry points | [bin/bin.c](../librz/bin/bin.c), [rz_bin.h](../librz/include/rz_bin.h) |
| RzAsm | Assemble instructions and produce disassembly text | [arch/asm.c](../librz/arch/asm.c), [rz_asm.h](../librz/include/rz_asm.h) |
| RzAnalysis | Instruction semantics and analysis state: types, branches, functions, blocks, and references | [arch/op.c](../librz/arch/op.c), [arch/fcn.c](../librz/arch/fcn.c), [rz_analysis.h](../librz/include/rz_analysis.h) |
| RzCons / RzPrint | Console buffering and presentation / reusable byte-formatting routines | [cons/cons.c](../librz/cons/cons.c), [util/print.c](../librz/util/print.c) |
| RzConfig / RzFlag | Settings and named addresses used by commands and rendering | [config/config.c](../librz/config/config.c), [flag/flag.c](../librz/flag/flag.c) |
| RzUtil | Shared strings, containers, buffers, numbers, and portability helpers | [util/README.md](../librz/util/README.md), [rz_util/](../librz/include/rz_util/) |

RzBin's interpretation of an executable and RzIO's address maps are different
things. [core/cfile.c](../librz/core/cfile.c), especially `rz_core_file_open` and
`rz_core_bin_load`, and [core/cbin.c](../librz/core/cbin.c) connect them. A raw
byte input does not need ELF or PE metadata to be read or decoded.

RzAsm and RzAnalysis are also different interfaces even though their
implementations now share [librz/arch/](../librz/arch/). Architecture-specific
implementations live under [arch/p/](../librz/arch/p/) and
[arch/isa/](../librz/arch/isa/). Skip a backend's decoder internals until you have
established whether the error is in its text, its instruction semantics, or a
core command that uses them.

## 3. Follow a command from its description to its handler

Start with `px 2`. Command descriptions are declarative, while command behavior
is implemented in C:

1. Find `px` in [cmd_print.yaml](../librz/core/cmd_descs/cmd_print.yaml).
   Its executable entry has `cname: print_hexdump`, an optional length, and
   standard and JSON output modes. Compare this contract with `px?` in Rizin.
2. Find `"px"` in [cmd_descs.c](../librz/core/cmd_descs/cmd_descs.c).
   `rzshell_cmddescs_init` registers it with `rz_cmd_desc_group_state_new` and
   `rz_print_hexdump_handler`. This connects the help and argument contract to
   the executable callback.
3. In [cmd/cmd.c](../librz/core/cmd/cmd.c), `rz_core_cmd_init` calls that
   registration function. For execution, follow `rz_core_cmd0` through
   `rz_core_cmd` to `core_cmd_tsrzcmd`. The Tree-sitter parser handles the shell
   syntax; `handle_ts_arged_stmt` constructs parsed arguments and calls
   `rz_cmd_call_parsed_args`. To locate this macro-generated handler's definition,
   search for `DEFINE_HANDLE_TS_FCN_AND_SYMBOL(arged_stmt)`. Initially skip pipes,
   redirects, loops, and the other statement handlers.
4. In [cmd/cmd_api.c](../librz/core/cmd/cmd_api.c), follow
   `rz_cmd_call_parsed_args`, `call_cd`, and `argv_call_cb`. They resolve the
   command descriptor, check the argument count, select an output mode, and
   invoke its callback. For `px`, the callback receives a `RzCmdStateOutput`.
5. Read `rz_print_hexdump_handler` in
   [cmd/cmd_print.c](../librz/core/cmd/cmd_print.c). It resolves the length and
   passes the current offset and output mode to the core printing helper.

The parser grammar is in
[subprojects/rizin-shell-parser/](../subprojects/rizin-shell-parser/). You do not
need to change it to adjust an ordinary command's argument contract or handler.

[cmd_descs_generate.py](../librz/core/cmd_descs/cmd_descs_generate.py) generates
the descriptor C and header files from YAML. Read
[cmd_descs/meson.build](../librz/core/cmd_descs/meson.build) to see the inputs and
the `regenerate_cmds` setting. Edit YAML for help or argument changes and
regenerate the descriptors using the build's configured workflow; do not treat
generated registration code as the primary place to change a command.

## 4. Follow the bytes and compare two consumers

For the standard `px 2` path with the default `diff.from=diff.to=0`:

```text
rz_print_hexdump_handler                    core/cmd/cmd_print.c
  -> rz_core_print_hexdump_or_hexdiff        core/cprint.c
     -> rz_core_print_hexdump_or_hexdiff_str
        -> rz_io_read_at_mapped             io/io.c
        -> rz_print_hexdump_str             util/print.c
     -> rz_cons_print (macro)               include/rz_cons.h
        -> rz_cons_strcat                   cons/cons.c
```

The string helper reads bytes from the mapped address space and formats them;
its wrapper sends the result to the console and frees the string. The JSON
branch uses `rz_print_jsondump_str` instead. Begin with these short functions in
[core/cprint.c](../librz/core/cprint.c), then read the called functions only as
needed. `px` does not need to decide what instruction those bytes represent.

Now repeat the descriptor and dispatch lookup for `pd 2`. Its YAML `cname` is
`cmd_disassembly_n_instructions`, and its callback is
`rz_cmd_disassembly_n_instructions_handler`. In the same `cmd_print.c`, follow
that callback into `core_disassembly`. It reads bytes with
`rz_io_read_at_mapped`, then chooses the renderer for the requested output mode.
The standard `pd` path calls `rz_core_print_disasm` in
[core/disasm.c](../librz/core/disasm.c); JSON and quiet modes take separate paths.

The standard renderer uses both `rz_asm_disassemble` for instruction text and
`rz_analysis_op` for instruction metadata and analysis-aware presentation.
Follow their interfaces to [arch/asm.c](../librz/arch/asm.c) and
[arch/op.c](../librz/arch/op.c): they dispatch to the selected architecture
callbacks. This explains why `pd` can combine mnemonics with branch information,
references, and existing function annotations.

`core/disasm.c` is a useful larger file to explore after the short `px` path.
Read the renderer's setup and the instruction loop first. Skip color themes,
source lines, jump-line drawing, and special data-format rendering until a
specific change needs them. Use `aoj 2` from the first exercise to inspect
instruction metadata, and `afl` to distinguish that from function analysis.

## 5. Read API contracts and build boundaries before changing code

Public declarations are under [librz/include/](../librz/include/). Find a
function's declaration there, its definition in the module, and one existing
caller. A search such as this keeps the reading bounded:

```sh
rg -n 'rz_print_hexdump_handler|rz_core_print_hexdump_or_hexdiff' librz/core
```

[librz/meson.build](../librz/meson.build) lists the libraries and generates their
pkg-config metadata. Each module's `meson.build` records its sources and
dependencies; for example, [core/meson.build](../librz/core/meson.build) links the
libraries coordinated by RzCore. A build dependency is not proof that every
runtime request uses that library: the `px` path above needs I/O and formatting,
whereas `pd` also needs instruction decoding.

Keep these conventions in mind while reading signatures:

| Marker | What to check |
| --- | --- |
| `RZ_API` | Public library API, declared in a public header |
| `RZ_IPI` | Internal API convention; prefer an existing public API when calling across library boundaries |
| `static` | Implementation local to that C file |
| `RZ_OWN` | Ownership transfers to the caller for a return value, or to the callee for an argument |
| `RZ_BORROW` | The receiver may use the object without taking ownership |
| `RZ_NONNULL` / `RZ_NULLABLE` | Whether the contract permits a null pointer |

See [rz_types.h](../librz/include/rz_types.h) and the pointer-modifier section of
[DEVELOPERS.md](../DEVELOPERS.md) for the definitions. Pair constructors and
destructors, and check cleanup on error exits. For example, an allocated
`RzAnalysisOp` is released with `rz_analysis_op_free`; an operation initialized
in caller storage uses `rz_analysis_op_fini` to release its contents. A returned
`RZ_OWN char *` is not a borrowed string to retain indefinitely.

## 6. Pick a test at the boundary you changed

Read [test/README.md](../test/README.md), then find a nearby existing test rather
than starting with the entire suite:

| Change | Existing tests to read first |
| --- | --- |
| Command parsing, descriptors, or dispatch | [test_cmd.c](../test/unit/test_cmd.c), [test_core_cmd.c](../test/unit/test_core_cmd.c) |
| `px` or `pd` output and arguments | [db/cmd/cmd_px](../test/db/cmd/cmd_px), [db/cmd/cmd_pd](../test/db/cmd/cmd_pd) |
| Instruction analysis metadata | [test_analysis_op.c](../test/unit/test_analysis_op.c), [db/cmd/cmd_ao](../test/db/cmd/cmd_ao), the matching [db/analysis/](../test/db/analysis/) backend |
| Assembly or disassembly of an instruction | The matching architecture file in [db/asm/](../test/db/asm/) |
| I/O addresses, maps, or byte reads | [test_io.c](../test/unit/test_io.c) |
| Behavior spanning file loading, analysis, and project persistence | [test_open_analyse_save_load_project.c](../test/integration/test_open_analyse_save_load_project.c), [db/cmd/project](../test/db/cmd/project) |

For a build directory named `build`, examples of focused test commands are:

```sh
meson test -C build --print-errorlogs core_cmd
```

With the matching installation's tools first in `PATH`, run command regressions
from the `test` directory:

```sh
cd test
rz-test db/cmd/cmd_px db/cmd/cmd_pd
```

Command tests verify observable shell behavior; unit tests isolate an API;
integration tests exercise collaboration between libraries. Choose according
to the change, including failure behavior where it matters. Do not replace
expected output automatically before understanding why it changed. Some tests
need fixtures under `test/bins`, which is a separate test-binary repository.

You now have a small route from executable startup to a command contract,
handler, module API, output, and test. For the next feature or bug, begin at the
observable command, identify the boundary responsible for it, and expand this
route only as far as the change requires. Use the
[library index](../librz/README.md) and the generated API documentation for
reference while following that route.
