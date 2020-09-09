import json
import os
import shutil
import sys

def check_test_logs(results_path, build_id):
    passed_tests = []
    failed_tests = []
    timestamp = None

    try:
        with open(results_path, encoding='utf-8-sig') as results_file:
            results_obj = json.load(results_file)

            timestamp = results_obj['reportCreatedOn']
            for test in results_obj['tests']:
                test_name = test['fullTestPath']
                test_result = test['state']
                num_errors = test['errors']
                test_success = (test_result == 'Success') and (num_errors == 0)

                if test_success:
                    passed_tests.append(test_name)
                else:
                    failed_tests.append(test_name)

                print("Test result for %s: %s" % (test_name, "success" if test_success else "failure"))
    except e:
        pass

    if timestamp is not None:
        results_dir, results_filename = os.path.split(results_path)
        results_filebase, results_fileext = os.path.splitext(results_filename)
        results_copy_filename = "test-results_build-%s_%s%s" % (build_id, timestamp, results_fileext)
        results_copy_path = os.path.join(results_dir, results_copy_filename)
        shutil.copy(results_path, results_copy_path)

    num_passed_tests = len(passed_tests)
    num_failed_tests = len(failed_tests)
    if num_failed_tests > 0:
        print("ERROR: %d test(s) failed!" % num_failed_tests)
        return 1
    elif num_passed_tests == 0:
        print("ERROR: no tests found!")
        return 2
    else:
        print("SUCCESS: all %d tests passed!" % num_passed_tests)
        return 0

if __name__ == '__main__':
    if len(sys.argv) > 2:
        sys.exit(check_test_logs(sys.argv[1], sys.argv[2]))
    else:
        print("ERROR: must supply arguments for the path to the test results and the build ID!")
        sys.exit(-1)
