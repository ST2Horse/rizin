// SPDX-FileCopyrightText: 2021 ret2libc <sirmy15@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_core.h>
#include <rz_analysis.h>
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

static const char *fpu_profile =
	"gpr r0 .32 0 0\n"
	"fpu control .16 0 0\n"
	"fpu f32 .32 2 0\n"
	"fpu f64 .64 6 0\n"
	"fpu f80 .80 14 0\n"
	"fpu f128 .128 24 0\n"
	"xmm@fpu vector .128 40 0\n"
	"fpu byte .8 56 0\n";

static bool test_fpu_regs_list(void) {
	RzCore *core = rz_core_new();
	mu_assert("set floating-point register profile",
		rz_reg_set_profile_string(rz_analysis_get_reg(core->analysis), fpu_profile));
	rz_config_set_i(core->config, "scr.color", 0);
	rz_core_cmd0_rzshell(core, "ar control=0x1234");
	char *output = rz_core_cmd_str(core, "arF");
	rz_core_free(core);
	mu_assert_streq(output,
		"control = 0x1234\n"
		"f32 = 0x00000000\n"
		"f64 = 0x0000000000000000\n"
		"f80 = 0x00000000000000000000\n"
		"f128 = 0x00000000000000000000000000000000\n"
		"vector = 0x00000000000000000000000000000000\n"
		"byte = 0x00\n",
		"list raw profile values, including controls and vectors");
	free(output);
	mu_end;
}

static bool test_fpu_regs_values(void) {
	int retval = MU_PASSED;
	RzCore *core = rz_core_new();
	char *command = NULL;
	char *output = NULL;
	if (!rz_reg_set_profile_string(rz_analysis_get_reg(core->analysis), fpu_profile)) {
		mu_cleanup_fail(beach, "set floating-point register profile");
	}
	const char *names[] = { "f32", "f64", "f80", "f128" };
	for (size_t i = 0; i < RZ_ARRAY_SIZE(names); i++) {
		command = rz_str_newf("arF %s = 1.5", names[i]);
		if (rz_core_cmd0_rzshell(core, command) != RZ_CMD_STATUS_OK) {
			mu_cleanup_fail(beach, "floating-point assignment returns success");
		}
		free(command);
		command = rz_str_newf("arF %s", names[i]);
		if (rz_core_cmd0_rzshell(core, command) != RZ_CMD_STATUS_OK) {
			mu_cleanup_fail(beach, "floating-point read returns success");
		}
		output = rz_core_cmd_str(core, command);
		if (!output || strcmp(output, "1.5\n")) {
			mu_cleanup_fail(beach, "floating-point value is read back");
		}
		free(output);
		output = NULL;
		free(command);
		command = NULL;
	}
	if (rz_core_cmd0_rzshell(core, "arF missing") != RZ_CMD_STATUS_ERROR) {
		mu_cleanup_fail(beach, "unknown register is rejected");
	}
	if (rz_core_cmd0_rzshell(core, "arF r0 = 2.5") != RZ_CMD_STATUS_OK) {
		mu_cleanup_fail(beach, "named general-purpose registers keep their floating-point reinterpretation");
	}
	output = rz_core_cmd_str(core, "arF r0");
	if (!output || strcmp(output, "2.5\n")) {
		mu_cleanup_fail(beach, "named values preserve existing reinterpretation");
	}
	if (rz_core_cmd0_rzshell(core, "arF byte") != RZ_CMD_STATUS_ERROR) {
		mu_cleanup_fail(beach, "unsupported floating-point width is rejected");
	}

beach:
	free(output);
	free(command);
	rz_core_free(core);
	mu_cleanup_end;
}

typedef struct {
	int read_type;
	int reads;
} FpuSyncContext;

static int fpu_reg_read(RzDebug *dbg, int type, ut8 *buf, int size) {
	FpuSyncContext *context = dbg->plugin_data;
	context->read_type = type;
	context->reads++;
	if (type != RZ_REG_TYPE_FPU || size < 4) {
		return 0;
	}
	rz_write_le32(buf, 0x3fc00000);
	return 4;
}

static bool fpu_sync_fail(RzDebug *dbg, RzReg *reg, bool write) {
	return false;
}

static bool test_fpu_regs_sync(void) {
	int retval = MU_PASSED;
	RzCore *core = rz_core_new();
	char *output = NULL;
	RzDebug *dbg = core->dbg;
	RzDebugPlugin *old_plugin = dbg->cur;
	void *old_data = dbg->plugin_data;
	int old_pid = dbg->pid;
	RzDebugReason old_reason = dbg->reason;
	FpuSyncContext context = { 0 };
	RzDebugPlugin plugin = { .name = "fpu-test", .reg_read = fpu_reg_read };
	dbg->cur = &plugin;
	dbg->plugin_data = &context;
	dbg->pid = 1;
	dbg->reason.type = RZ_DEBUG_REASON_NONE;
	rz_config_set_i(core->config, "scr.color", 0);
	if (!rz_reg_set_profile_string(dbg->reg, "fpu f0 .32 0 0\n")) {
		mu_cleanup_fail(beach, "set debugger floating-point register profile");
	}
	output = rz_core_cmd_str(core, "drF");
	if (context.read_type != RZ_REG_TYPE_FPU || context.reads != 1) {
		mu_cleanup_fail(beach, "synchronize a positive FPU type once");
	}
	if (!output || strcmp(output, "f0 = 0x3fc00000\n")) {
		mu_cleanup_fail(beach, "print synchronized register bits");
	}
	plugin.sync_registers = fpu_sync_fail;
	if (rz_core_cmd0_rzshell(core, "drF") != RZ_CMD_STATUS_ERROR) {
		mu_cleanup_fail(beach, "synchronization failure is reported");
	}

beach:
	free(output);
	dbg->cur = old_plugin;
	dbg->plugin_data = old_data;
	dbg->pid = old_pid;
	dbg->reason = old_reason;
	rz_core_free(core);
	mu_cleanup_end;
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

int all_tests() {
	mu_run_test(test_arg_cmd);
	mu_run_test(test_arg_cmd_last);
	mu_run_test(test_arg_cmd_last_with_at);
	mu_run_test(test_arg_cmd_last_opt);
	mu_run_test(test_fpu_regs_list);
	mu_run_test(test_fpu_regs_values);
	mu_run_test(test_fpu_regs_sync);
	mu_run_test(test_hist_push);
	mu_run_test(test_hist_push_seq);
	mu_run_test(test_hist_push_load);
	return tests_passed != tests_run;
}

mu_main(all_tests)
