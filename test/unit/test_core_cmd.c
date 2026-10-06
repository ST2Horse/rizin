// SPDX-FileCopyrightText: 2021 ret2libc <sirmy15@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_core.h>
#include "minunit.h"

static const RzCmdDescArg string_args[] = {
	{ .name = "s", .type = RZ_CMD_ARG_TYPE_STRING },
	{ 0 },
};
static const RzCmdDescHelp string_help = {
	.summary = "string help",
	.args = string_args,
};
static const RzCmdDescArg cmd_args[] = {
	{ .name = "c", .type = RZ_CMD_ARG_TYPE_CMD },
	{ 0 },
};
static const RzCmdDescHelp cmd_help = {
	.summary = "cmd help",
	.args = cmd_args,
};
static const RzCmdDescArg cmd_last_args[] = {
	{ .name = "c", .type = RZ_CMD_ARG_TYPE_CMD, .flags = RZ_CMD_ARG_FLAG_LAST },
	{ 0 },
};
static const RzCmdDescHelp cmd_last_help = {
	.summary = "cmd_last help",
	.args = cmd_last_args,
};
static const RzCmdDescArg cmd_last_opt_args[] = {
	{ .name = "c1", .type = RZ_CMD_ARG_TYPE_CMD },
	{ .name = "c2", .type = RZ_CMD_ARG_TYPE_CMD, .flags = RZ_CMD_ARG_FLAG_LAST, .optional = true },
	{ 0 },
};
static const RzCmdDescHelp cmd_last_opt_help = {
	.summary = "cmd_last help",
	.args = cmd_last_opt_args,
};

static RzCmdStatus string_handler(RzCore *core, int argc, const char **argv) {
	return argc == 2 ? RZ_CMD_STATUS_OK : RZ_CMD_STATUS_ERROR;
}

static RzCmdStatus cmd_handler(RzCore *core, int argc, const char **argv) {
	return argc == 2 ? RZ_CMD_STATUS_OK : RZ_CMD_STATUS_ERROR;
}

static RzCmdStatus cmd_last_handler(RzCore *core, int argc, const char **argv) {
	return argc == 2 ? RZ_CMD_STATUS_OK : RZ_CMD_STATUS_ERROR;
}

static RzCmdStatus cmd_last_with_at_handler(RzCore *core, int argc, const char **argv) {
	mu_assert_eq(argc, 2, "just one arg");
	mu_assert_streq(argv[1], "string hello \\@ 0xdeadbeef", "the command should be passed as argument");
	return RZ_CMD_STATUS_OK;
}

static RzCmdStatus cmd_last_opt_handler(RzCore *core, int argc, const char **argv) {
	if (argc >= 2) {
		mu_assert_streq(argv[1], "string 'hello everybody'", "first cmd, non optional, should be string");
	}
	if (argc == 3) {
		mu_assert_streq(argv[2], "cmd string\\ hello", "second cmd, optional, should be cmd");
		RzCmdStatus s = rz_core_cmd0_rzshell(core, argv[2]);
		mu_assert_eq(s, RZ_CMD_STATUS_OK, "cmd_last second arg should be executed well");
	}
	return RZ_CMD_STATUS_OK;
}

static RzCmd *old_rcmd = NULL;

static RzCore *fake_core_new(void) {
	RzCore *core = rz_core_new();
	old_rcmd = core->rcmd;
	core->rcmd = rz_core_cmd_new(core, true);
	RzCmdDesc *root = rz_cmd_get_root(core->rcmd);
	rz_cmd_desc_argv_new(core->rcmd, root, "string", string_handler, &string_help);
	rz_cmd_desc_argv_new(core->rcmd, root, "cmd", cmd_handler, &cmd_help);
	rz_cmd_desc_argv_new(core->rcmd, root, "cmd_last", cmd_last_handler, &cmd_last_help);
	rz_cmd_desc_argv_new(core->rcmd, root, "cmd_last_with_at", cmd_last_with_at_handler, &cmd_last_help);
	rz_cmd_desc_argv_new(core->rcmd, root, "cmd_last_opt", cmd_last_opt_handler, &cmd_last_opt_help);
	return core;
}

static void fake_core_free(RzCore *core) {
	rz_cmd_free(core->rcmd);
	core->rcmd = old_rcmd;
	rz_core_free(core);
}

static bool test_arg_cmd(void) {
	RzCore *core = fake_core_new();
	RzCmdStatus s = rz_core_cmd0_rzshell(core, "cmd \"string hello\"");
	mu_assert_eq(s, RZ_CMD_STATUS_OK, "argument cmd is passed");
	fake_core_free(core);
	mu_end;
}

static bool test_arg_cmd_last(void) {
	RzCore *core = fake_core_new();
	RzCmdStatus s = rz_core_cmd0_rzshell(core, "cmd_last string hello");
	mu_assert_eq(s, RZ_CMD_STATUS_OK, "argument cmd is passed as a single arg");
	fake_core_free(core);
	mu_end;
}

static bool test_arg_cmd_last_with_at(void) {
	RzCore *core = fake_core_new();
	RzCmdStatus s = rz_core_cmd0_rzshell(core, "cmd_last_with_at string hello \\@ 0xdeadbeef");
	mu_assert_eq(s, RZ_CMD_STATUS_OK, "argument cmd is passed as a single arg");
	fake_core_free(core);
	mu_end;
}

static bool test_arg_cmd_last_opt(void) {
	RzCore *core = fake_core_new();
	RzCmdStatus s = rz_core_cmd0_rzshell(core, "cmd_last_opt \"string 'hello everybody'\" cmd 'string hello'");
	mu_assert_eq(s, RZ_CMD_STATUS_OK, "argument cmd is passed as a single arg");
	s = rz_core_cmd0_rzshell(core, "cmd_last_opt \"string 'hello everybody'\"");
	mu_assert_eq(s, RZ_CMD_STATUS_OK, "argument cmd is passed as a single arg");
	fake_core_free(core);
	mu_end;
}

static bool test_hist_push(void) {
	RzCore *core = rz_core_new();
	RzLine *line = core->cons->line;
	const char *mark = "i5824";
	const char *cmd = "echo i5824";

	rz_line_hist_add(line, cmd);
	int history_top = line->history.top;
	rz_cons_readflush();

	free(core->cmdqueue);
	core->cmdqueue = rz_str_newf("< \\x12%s\\n", mark);
	mu_assert_notnull(core->cmdqueue, "cmdqueue should be allocated");

	mu_assert_eq(rz_core_prompt_exec(core), 0, "push_escaped prompt command should execute");
	mu_assert_eq(line->history.top, history_top, "push_escaped should not be added to history");
	mu_assert_streq(rz_line_hist_get(line, history_top), cmd, "last history entry should remain the preexisting matching command");

	const char *res = rz_line_readline(line);
	mu_assert_notnull(res, "reverse-search result should not be null");
	mu_assert_streq(res, cmd, "reverse-search should resolve to prior history, not the injector command");

	rz_cons_readflush();
	rz_core_free(core);
	mu_end;
}

static bool test_hist_push_seq(void) {
	RzCore *core = rz_core_new();
	RzLine *line = core->cons->line;
	const char *mark = "i5824s";
	const char *cmd = "echo i5824s";

	rz_line_hist_add(line, cmd);
	int history_top = line->history.top;
	rz_cons_readflush();

	free(core->cmdqueue);
	core->cmdqueue = rz_str_newf("echo %s; < \\x12%s\\n", mark, mark);
	mu_assert_notnull(core->cmdqueue, "cmdqueue should be allocated");

	mu_assert_eq(rz_core_prompt_exec(core), 0, "push_escaped prompt command should execute");
	mu_assert_eq(line->history.top, history_top, "command lines containing push_escaped should not be added to history");
	mu_assert_streq(rz_line_hist_get(line, history_top), cmd, "last history entry should remain the preexisting matching command");

	const char *res = rz_line_readline(line);
	mu_assert_notnull(res, "reverse-search result should not be null");
	mu_assert_streq(res, cmd, "reverse-search should resolve to prior history, not the injector command");

	rz_cons_readflush();
	rz_core_free(core);
	mu_end;
}

static bool test_hist_push_load(void) {
	int retval = MU_PASSED;
	const char *cmd = "echo i5824l";
	const char *data = "< \\x12i5824l\\n\necho i5824l\n";
	char *old = rz_sys_getenv(RZ_SYS_HOME);
	char *home = rz_file_temp("rz5824");
	char *hist = NULL;
	char *dir = NULL;
	char *cache = NULL;
	RzCore *core = NULL;

	if (!home) {
		mu_cleanup_fail(beach, "home path should be available");
	}
	if (!rz_sys_mkdirp(home)) {
		mu_cleanup_fail(beach, "home dir should be created");
	}
	rz_sys_setenv(RZ_SYS_HOME, home);
	hist = rz_path_home_history();
	if (!hist) {
		mu_cleanup_fail(beach, "history path should be available");
	}
	dir = rz_file_dirname(hist);
	if (!dir) {
		mu_cleanup_fail(beach, "history dir should be available");
	}
	cache = rz_file_dirname(dir);
	if (!cache) {
		mu_cleanup_fail(beach, "cache dir should be available");
	}
	if (!rz_sys_mkdirp(dir)) {
		mu_cleanup_fail(beach, "history dir should be created");
	}
	if (!rz_file_dump(hist, (const ut8 *)data, (int)strlen(data), false)) {
		mu_cleanup_fail(beach, "history file should be written");
	}
	core = rz_core_new();
	if (!core) {
		mu_cleanup_fail(beach, "core should be created");
	}
	mu_assert_streq(rz_line_hist_get(core->cons->line, 1), cmd, "loaded history should keep only normal commands");
	mu_assert_null(rz_line_hist_get(core->cons->line, 2), "loaded push_escaped entry should be removed");

beach:
	rz_core_free(core);
	rz_sys_setenv(RZ_SYS_HOME, old);
	if (hist) {
		rz_file_rm(hist);
	}
	if (dir) {
		rz_file_rm(dir);
	}
	if (cache) {
		rz_file_rm(cache);
	}
	if (home) {
		rz_file_rm(home);
	}
	free(old);
	free(cache);
	free(dir);
	free(hist);
	free(home);
	mu_cleanup_end;
}

static bool check_pdj_small_block(const char *command, ut32 blocksize, bool backwards,
	bool defaults) {
	int retval = MU_PASSED;
	RzCore *core = rz_core_new();
	char *output = NULL;
	RzJson *json = NULL;
	const ut64 offsets[] = { 0, 5, 6, 16, 17, 32 };
	const char *bytes[] = { "b801000000", "90", "48b80102030405060708", "90",
		"6666666666662e0f1f840000000000", "90" };
	const char *opcodes[] = { "mov   eax, 0x01", "nop", "mov   rax, 0x807060504030201",
		"nop", "nop   word [rax+rax*1], ax", "nop" };
	if (!core) {
		mu_cleanup_fail(cleanup, "core should be created");
	}
	core->io->va = false;
	if (!rz_core_file_open(core, "malloc://2048", RZ_PERM_RW, 0)) {
		mu_cleanup_fail(cleanup, "memory file should open");
	}
	if (rz_core_cmd0_rzshell(core, "e asm.arch=x86; e asm.bits=64; e scr.color=0; "
		    "e asm.bytes=true; "
		    "e asm.nbytes=20; e asm.lines.bb=0; e asm.flags=false; "
		    "e asm.functions=false; e asm.comments=false; "
		    "wx b8010000009048b8010203040506070890"
		    "6666666666662e0f1f84000000000090c3") != RZ_CMD_STATUS_OK) {
		mu_cleanup_fail(cleanup, "test instructions should be written");
	}
	if (backwards && rz_core_cmd0_rzshell(core, "af") != RZ_CMD_STATUS_OK) {
		mu_cleanup_fail(cleanup, "backwards instructions should be analysed");
	}
	if (defaults && rz_core_cmd0_rzshell(core, "wx 9090909090") != RZ_CMD_STATUS_OK) {
		mu_cleanup_fail(cleanup, "default test instructions should be written");
	}
	if (!rz_core_block_size(core, blocksize)) {
		mu_cleanup_fail(cleanup, "small block should be selected");
	}
	ut64 offset = backwards ? 33 : 0;
	rz_core_seek(core, offset, true);
	ut8 *block = core->block;
	output = rz_core_cmd_str(core, command);
	if (!output || !(json = rz_json_parse(output)) || json->type != RZ_JSON_ARRAY) {
		mu_cleanup_fail(cleanup, "pdJ should return a JSON array");
	}
	size_t count = defaults ? 2 : RZ_ARRAY_SIZE(offsets);
	if (json->children.count != count) {
		mu_cleanup_fail(cleanup, "pdJ should return all requested instructions");
	}
	for (size_t i = 0; i < count; i++) {
		const RzJson *item = rz_json_item(json, i);
		const RzJson *address = rz_json_get(item, "offset");
		const RzJson *text = rz_json_get(item, "text");
		if (!address || address->num.u_value != (defaults ? i : offsets[i]) ||
			!text || text->type != RZ_JSON_STRING ||
			!strstr(text->str_value, defaults ? "90" : bytes[i]) ||
			!strstr(text->str_value, defaults ? "nop" : opcodes[i])) {
			fprintf(stderr, "pdJ row %zu: %s\n", i, text ? text->str_value : "missing text");
			mu_cleanup_fail(cleanup, "pdJ should preserve addresses, bytes and instructions");
		}
	}
	if (core->offset != offset || core->blocksize != blocksize || core->block != block) {
		mu_cleanup_fail(cleanup, "pdJ should preserve seek and the original block");
	}
cleanup:
	rz_json_free(json);
	free(output);
	rz_core_free(core);
	mu_cleanup_end;
}

static bool test_pdj_small_block_forward(void) {
	mu_assert("one-byte block", check_pdj_small_block("pdJ 6", 1, false, false));
	mu_assert("two-byte block", check_pdj_small_block("pdJ 6", 2, false, false));
	mu_assert("crossing block boundary", check_pdj_small_block("pdJ 6", 16, false, false));
	mu_end;
}

static bool test_pdj_small_block_backwards(void) {
	mu_assert("backwards with one-byte block", check_pdj_small_block("pdJ -6", 1, true, false));
	mu_assert("backwards with two-byte block", check_pdj_small_block("pdJ -6", 2, true, false));
	mu_end;
}

static bool test_pdj_default_block_limit(void) {
	mu_assert("no argument", check_pdj_small_block("pdJ", 2, false, true));
	mu_assert("explicit zero", check_pdj_small_block("pdJ 0", 2, false, true));
	mu_end;
}

static bool test_pdj_count_range(void) {
	RzCore *core = rz_core_new();
	mu_assert_notnull(core, "core should be created");
	rz_core_block_size(core, 2);
	rz_core_seek(core, 10, false);
	mu_assert_eq(rz_core_cmd0_rzshell(core, "pdJ 32768"),
		RZ_CMD_STATUS_ERROR, "positive range limit");
	mu_assert_eq(rz_core_cmd0_rzshell(core, "pdJ -32769"),
		RZ_CMD_STATUS_ERROR, "negative range limit");
	mu_assert_eq(core->offset, 10, "invalid count should preserve seek");
	mu_assert_eq(core->blocksize, 2, "invalid count should preserve blocksize");
	rz_core_free(core);
	mu_end;
}

int all_tests() {
	mu_run_test(test_pdj_small_block_forward);
	mu_run_test(test_pdj_small_block_backwards);
	mu_run_test(test_pdj_default_block_limit);
	mu_run_test(test_pdj_count_range);
	mu_run_test(test_arg_cmd);
	mu_run_test(test_arg_cmd_last);
	mu_run_test(test_arg_cmd_last_with_at);
	mu_run_test(test_arg_cmd_last_opt);
	mu_run_test(test_hist_push);
	mu_run_test(test_hist_push_seq);
	mu_run_test(test_hist_push_load);
	return tests_passed != tests_run;
}

mu_main(all_tests)
