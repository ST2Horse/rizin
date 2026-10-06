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

static RzCmdStatus raw_file_command(RzCore *core, const char *command,
	const char *start, const char *size, const char *filename, const char *offset) {
	char *cmd = rz_str_newf("%s %s %s \"%s\" %s", command, start, size,
		filename, offset ? offset : "");
	if (!cmd) {
		return RZ_CMD_STATUS_ERROR;
	}
	RzCmdStatus status = rz_core_cmd0_rzshell(core, cmd);
	free(cmd);
	return status;
}

static bool test_print_raw_file_errors(void) {
	int retval = MU_PASSED;
	RzCore *core = rz_core_new();
	char *filename = rz_file_temp("rz-prf-errors");
	char *missing = filename ? rz_str_newf("%s/missing.bin", filename) : NULL;
	char *contents = NULL;
	const ut8 original[] = { 0x41, 0x00, 0xff, 0x20 };
	const char *invalid[][3] = {
		{ "unknown_export_address", "1", NULL },
		{ "1junk", "1", NULL },
		{ "0", "1/0", NULL },
		{ "0", "-1", NULL },
		{ "0", "1.5", NULL },
		{ "0x10000000000000000", "1", NULL },
		{ "0xffffffffffffffff", "2", NULL },
		{ "0", "0x10000000000000000", NULL },
		{ "0", "1", "-1" },
		{ "0", "1", "0x8000000000000000" },
		{ "0", "2", "0x7fffffffffffffff" },
		{ "31", "2", NULL },
	};
	if (!core || !filename || !missing ||
		!rz_core_file_open(core, "malloc://32", RZ_PERM_RW, 0) ||
		!rz_file_dump(filename, original, sizeof(original), false)) {
		mu_cleanup_fail(cleanup, "raw file test setup");
	}
	core->io->va = false;
	for (size_t i = 0; i < RZ_ARRAY_SIZE(invalid); i++) {
		const char *command = invalid[i][2] ? "prfa" : "prf";
		RzCmdStatus status = raw_file_command(core, command, invalid[i][0], invalid[i][1],
			filename, invalid[i][2]);
		if (status != RZ_CMD_STATUS_ERROR) {
			eprintf("unexpected status %d for start=%s size=%s offset=%s\n", status,
				invalid[i][0], invalid[i][1], invalid[i][2] ? invalid[i][2] : "EOF");
			mu_cleanup_fail(cleanup, "invalid input or short read must fail");
		}
		size_t size = 0;
		contents = rz_file_slurp(filename, &size);
		if (!contents || size != sizeof(original) || memcmp(contents, original, size)) {
			mu_cleanup_fail(cleanup, "validation and initial read failures must preserve the file");
		}
		RZ_FREE(contents);
	}
	if (raw_file_command(core, "prf", "0", "1", missing, NULL) != RZ_CMD_STATUS_ERROR ||
		raw_file_command(core, "prfa", "0", "1", missing, "0") != RZ_CMD_STATUS_ERROR) {
		mu_cleanup_fail(cleanup, "file open failures must fail");
	}
	if (raw_file_command(core, "prf", "0", "1", filename, NULL) != RZ_CMD_STATUS_OK ||
		rz_file_size(filename) != 1) {
		mu_cleanup_fail(cleanup, "an invalid expression must not poison later valid exports");
	}

cleanup:
	free(contents);
	if (filename) {
		rz_file_rm(filename);
	}
	free(missing);
	free(filename);
	rz_core_free(core);
	mu_cleanup_end;
}

static bool test_print_raw_file_zero_size(void) {
	int retval = MU_PASSED;
	RzCore *core = rz_core_new();
	char *filename = rz_file_temp("rz-prf-zero");
	char *contents = NULL;
	const ut8 original[] = { 0, 0xff, 0x20 };
	size_t size = 0;
	if (!core || !filename || !rz_file_dump(filename, original, sizeof(original), false)) {
		mu_cleanup_fail(cleanup, "zero size test setup");
	}
	if (raw_file_command(core, "prfa", "0", "0", filename, NULL) != RZ_CMD_STATUS_OK ||
		raw_file_command(core, "prfa", "0", "0", filename, "8") != RZ_CMD_STATUS_OK) {
		mu_cleanup_fail(cleanup, "zero size append must succeed without reading memory");
	}
	contents = rz_file_slurp(filename, &size);
	if (!contents || size != sizeof(original) || memcmp(contents, original, size)) {
		mu_cleanup_fail(cleanup, "zero size append must preserve existing bytes and file size");
	}
	RZ_FREE(contents);
	if (raw_file_command(core, "prf", "0", "0", filename, NULL) != RZ_CMD_STATUS_OK ||
		rz_file_size(filename) != 0) {
		mu_cleanup_fail(cleanup, "zero size export must truncate the file");
	}
	rz_file_rm(filename);
	if (raw_file_command(core, "prfa", "0", "0", filename, NULL) != RZ_CMD_STATUS_OK ||
		!rz_file_exists(filename) || rz_file_size(filename) != 0) {
		mu_cleanup_fail(cleanup, "zero size append must create an empty file");
	}

cleanup:
	free(contents);
	if (filename) {
		rz_file_rm(filename);
	}
	free(filename);
	rz_core_free(core);
	mu_cleanup_end;
}

static bool test_print_raw_file_write_error(void) {
	int retval = MU_PASSED;
#if !__WINDOWS__
	RzCore *core = rz_core_new();
	char *filename = rz_file_temp("rz-prf-full");
	if (!core || !filename || !rz_core_file_open(core, "malloc://32", RZ_PERM_RW, 0)) {
		mu_cleanup_fail(cleanup, "write error test setup");
	}
	core->io->va = false;
	if (access("/dev/full", F_OK)) {
		printf("SKIP: /dev/full is unavailable\n");
		goto cleanup;
	}
	if (raw_file_command(core, "prf", "0", "1", filename, NULL) != RZ_CMD_STATUS_OK ||
		rz_file_size(filename) != 1) {
		mu_cleanup_fail(cleanup, "normal export must succeed before testing a write failure");
	}
	if (raw_file_command(core, "prf", "0", "1", "/dev/full", NULL) != RZ_CMD_STATUS_ERROR) {
		mu_cleanup_fail(cleanup, "buffered write or close failures must fail");
	}
cleanup:
	if (filename) {
		rz_file_rm(filename);
	}
	free(filename);
	rz_core_free(core);
#else
	printf("SKIP: /dev/full is unavailable on Windows\n");
#endif
	mu_cleanup_end;
}

int all_tests() {
	mu_run_test(test_arg_cmd);
	mu_run_test(test_arg_cmd_last);
	mu_run_test(test_arg_cmd_last_with_at);
	mu_run_test(test_arg_cmd_last_opt);
	mu_run_test(test_hist_push);
	mu_run_test(test_hist_push_seq);
	mu_run_test(test_hist_push_load);
	mu_run_test(test_print_raw_file_errors);
	mu_run_test(test_print_raw_file_zero_size);
	mu_run_test(test_print_raw_file_write_error);
	return tests_passed != tests_run;
}

mu_main(all_tests)
