// SPDX-FileCopyrightText: 2021 Anton Kochkov <anton.kochkov@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_core.h>
#include <rz_analysis.h>
#include <rz_project.h>

#include "../unit/minunit.h"

bool test_open_analyse_save() {
	// 1. Open the file
	RzCore *core = rz_core_new();
	mu_assert_notnull(core, "new RzCore instance");
	const char *fpath = "bins/elf/dectest32";
	RzCoreFile *file = rz_core_file_open(core, fpath, RZ_PERM_R, 0);
	mu_assert_notnull(file, "open file");
	rz_core_bin_load(core, fpath, UT64_MAX);

	ut64 loadaddr = rz_config_get_i(core->config, "bin.baddr");
	mu_assert_eq(loadaddr, 0x08048000, "base address");

	// 2. Analyse the file
	rz_core_analysis_all(core);
	rz_core_analysis_everything(core, false, "esil");

	RzList *functionsold = rz_analysis_function_list(core->analysis);
	mu_assert_notnull(functionsold, "export functions list");

	// 3. Remove the function
	const char *fcnname = "sym.Aeropause";
	RzAnalysisFunction *fcn = rz_analysis_get_function_byname(core->analysis, fcnname);
	mu_assert_notnull(fcn, "find function");
	rz_analysis_function_delete(fcn);

	// 3. Export the function list
	RzList *functions = rz_analysis_function_list(core->analysis);
	mu_assert_notnull(functions, "export functions list");
	size_t functions_count_expect = rz_list_length(functions);

	// 4. Save into the project
	char *tmpdir = rz_file_tmpdir();
	char *project_file = rz_file_path_join(tmpdir, "test_open_analyse.rzdb");
	RzProjectErr err = rz_project_save_file(core, project_file, true);
	mu_assert_eq(err, RZ_PROJECT_ERR_SUCCESS, "project save err");
	free(project_file);

	// 5. Close the file
	rz_core_file_close(file);
	rz_core_free(core);

	// 6. Create a new core
	core = rz_core_new();
	mu_assert_notnull(core, "new RzCore instance");

	// 7. Load the previously saved project
	RzSerializeResultInfo *res = rz_serialize_result_info_new();
	mu_assert_notnull(res, "result info new");
	project_file = rz_file_path_join(tmpdir, "test_open_analyse.rzdb");
	err = rz_project_load_file(core, project_file, true, res);
	mu_assert_eq(err, RZ_PROJECT_ERR_SUCCESS, "project load err");
	free(project_file);

	// 8. Export the function list
	RzList *functions_load = rz_analysis_function_list(core->analysis);
	mu_assert_notnull(functions_load, "export functions list");

	// 9. Compare with the previously saved one
	mu_assert_eq(rz_list_length(functions_load), functions_count_expect, "compare functions list");

	// 10. Exit
	free(tmpdir);
	rz_serialize_result_info_free(res);
	rz_core_free(core);
	mu_end;
}

/** Check raw-map restoration separately from saved metadata and current IO state. */
static bool check_project_raw_map(ut64 address, bool remove_saved_address, bool load_bin_io) {
	int retval = MU_PASSED;
	const char *filename = "bins/other/0x3";
	const ut8 expected[] = { 0x6a, 0x69, 0x0a };
	RzCore *core = rz_core_new();
	RzProject *project = sdb_new0();
	RzSerializeResultInfo *res = rz_serialize_result_info_new();
	RzIOMap *original_map = NULL;
	ut64 expected_address = remove_saved_address ? 0 : address;
	if (!core || !project || !res ||
		!rz_core_file_open(core, filename, RZ_PERM_RX, address) ||
		!rz_core_bin_load(core, filename, UT64_MAX)) {
		mu_cleanup_fail(cleanup, "open source raw file");
	}
	rz_config_set_b(core->config, "io.va", true);
	if (!rz_flag_set(core->flags, "raw_marker", address + 2, 1) ||
		!rz_analysis_create_function(core->analysis, "raw_function", address, RZ_ANALYSIS_FCN_TYPE_FCN) ||
		!rz_core_seek(core, address + 1, true) ||
		rz_project_save(core, project, NULL) != RZ_PROJECT_ERR_SUCCESS) {
		mu_cleanup_fail(cleanup, "save raw project and metadata");
	}
	if (remove_saved_address) {
		Sdb *config = sdb_ns(sdb_ns(project, "core", false), "config", false);
		if (!config) {
			mu_cleanup_fail(cleanup, "saved config");
		}
		sdb_unset(config, "bin.laddr");
	}
	rz_core_free(core);
	core = rz_core_new();
	if (!core) {
		mu_cleanup_fail(cleanup, "new destination core");
	}
	// A stale destination setting must not replace a saved zero or a missing-key default.
	rz_config_set_i(core->config, "bin.laddr", 0x4800);
	if (!load_bin_io) {
		expected_address = 0x4800;
		if (!rz_core_file_open(core, filename, RZ_PERM_RX, expected_address) ||
			!rz_core_bin_load(core, filename, UT64_MAX)) {
			mu_cleanup_fail(cleanup, "open destination raw file");
		}
		original_map = rz_io_map_get(core->io, expected_address);
		if (!original_map) {
			mu_cleanup_fail(cleanup, "destination raw map");
		}
	}
	if (rz_project_load(core, project, load_bin_io, NULL, res) != RZ_PROJECT_ERR_SUCCESS) {
		mu_cleanup_fail(cleanup, "load raw project");
	}
	RzIOMap *map = rz_io_map_get(core->io, expected_address);
	if (!map || map->itv.addr != expected_address || map->itv.size != sizeof(expected) ||
		map->delta != 0 || (!load_bin_io && map != original_map)) {
		mu_cleanup_fail(cleanup, "actual raw mapping must be restored or preserved");
	}
	ut8 actual[sizeof(expected)];
	if (rz_io_nread_at(core->io, expected_address, actual, sizeof(actual)) != sizeof(actual) ||
		memcmp(actual, expected, sizeof(expected))) {
		mu_cleanup_fail(cleanup, "actual mapped raw bytes");
	}
	RzFlagItem *flag = rz_flag_get(core->flags, "raw_marker");
	RzAnalysisFunction *function = rz_analysis_get_function_byname(core->analysis, "raw_function");
	if (core->offset != address + 1 || !flag || flag->offset != address + 2 ||
		!function || function->addr != address) {
		mu_cleanup_fail(cleanup, "saved seek, flag and analysis addresses");
	}

cleanup:
	rz_serialize_result_info_free(res);
	rz_project_free(project);
	rz_core_free(core);
	mu_cleanup_end;
}

bool test_project_raw_nonzero_map() {
	return check_project_raw_map(0x2400, false, true);
}

bool test_project_raw_zero_map() {
	return check_project_raw_map(0, false, true);
}

bool test_project_raw_missing_map_address() {
	return check_project_raw_map(0x2400, true, true);
}

bool test_project_raw_keep_current_io() {
	return check_project_raw_map(0x2400, false, false);
}

int all_tests() {
	mu_run_test(test_open_analyse_save);
	mu_run_test(test_project_raw_nonzero_map);
	mu_run_test(test_project_raw_zero_map);
	mu_run_test(test_project_raw_missing_map_address);
	mu_run_test(test_project_raw_keep_current_io);
	return tests_passed != tests_run;
}

mu_main(all_tests)
