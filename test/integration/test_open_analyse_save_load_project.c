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

static bool check_project_base_address(ut64 base, bool remove_saved_base, bool load_bin_io) {
	const char *fpath = "bins/elf/dectest32";
	RzCore *core = rz_core_new();
	mu_assert_notnull(core, "new source core");
	mu_assert_notnull(rz_core_file_open(core, fpath, RZ_PERM_R, 0), "open source ELF");
	mu_assert_true(rz_core_bin_load(core, fpath, base), "load source ELF");
	ut64 expected_base = rz_bin_get_baddr(core->bin);
	RzFlagItem *entry = rz_flag_get(core->flags, "entry0");
	mu_assert_notnull(entry, "source entry flag");
	ut64 entry_addr = entry->offset;
	ut64 saved_entry_addr = entry_addr;
	ut8 expected[8];
	mu_assert_true(rz_io_read_at_mapped(core->io, entry_addr, expected, sizeof(expected)), "read source entry");
	mu_assert_notnull(rz_analysis_create_function(core->analysis, "saved_entry", entry_addr, RZ_ANALYSIS_FCN_TYPE_FCN), "create saved function");
	RzProject *project = sdb_new0();
	mu_assert_notnull(project, "new project");
	mu_assert_eq(rz_project_save(core, project, NULL), RZ_PROJECT_ERR_SUCCESS, "save project");
	if (remove_saved_base) {
		Sdb *config = sdb_ns(sdb_ns(project, "core", false), "config", false);
		mu_assert_notnull(config, "saved config");
		sdb_unset(config, "bin.baddr");
	}
	rz_core_free(core);

	core = rz_core_new();
	mu_assert_notnull(core, "new destination core");
	RzIOMap *original_map = NULL;
	if (!load_bin_io) {
		mu_assert_notnull(rz_core_file_open(core, fpath, RZ_PERM_R, 0), "open current ELF");
		mu_assert_true(rz_core_bin_load(core, fpath, 0x20000000), "load current ELF");
		expected_base = rz_bin_get_baddr(core->bin);
		entry_addr = rz_flag_get(core->flags, "entry0")->offset;
		original_map = rz_io_map_get(core->io, entry_addr);
		mu_assert_notnull(original_map, "current entry map");
	}
	RzSerializeResultInfo *res = rz_serialize_result_info_new();
	mu_assert_notnull(res, "new result info");
	mu_assert_eq(rz_project_load(core, project, load_bin_io, NULL, res), RZ_PROJECT_ERR_SUCCESS, "load project");
	mu_assert_eq(rz_bin_get_baddr(core->bin), expected_base, "actual binary base after load");
	RzIOMap *loaded_map = rz_io_map_get(core->io, entry_addr);
	mu_assert_notnull(loaded_map, "map at the expected entry after load");
	if (!load_bin_io) {
		mu_assert_ptreq(loaded_map, original_map, "keep current IO map");
	}
	ut8 actual[sizeof(expected)];
	mu_assert_true(rz_io_read_at_mapped(core->io, entry_addr, actual, sizeof(actual)), "read mapped entry after load");
	mu_assert_memeq(actual, expected, sizeof(actual), "preserve actual entry bytes");
	RzAnalysisFunction *fcn = rz_analysis_get_function_byname(core->analysis, "saved_entry");
	mu_assert_notnull(fcn, "saved analysis function");
	mu_assert_eq(fcn->addr, saved_entry_addr, "saved analysis address");
	rz_serialize_result_info_free(res);
	rz_project_free(project);
	rz_core_free(core);
	mu_end;
}

bool test_project_non_default_base() {
	return check_project_base_address(0x10000000, false, true);
}

bool test_project_zero_base() {
	return check_project_base_address(0, false, true);
}

bool test_project_without_saved_base() {
	return check_project_base_address(UT64_MAX, true, true);
}

bool test_project_keep_current_io() {
	return check_project_base_address(0x10000000, false, false);
}

int all_tests() {
	mu_run_test(test_open_analyse_save);
	mu_run_test(test_project_non_default_base);
	mu_run_test(test_project_zero_base);
	mu_run_test(test_project_without_saved_base);
	mu_run_test(test_project_keep_current_io);
	return tests_passed != tests_run;
}

mu_main(all_tests)
