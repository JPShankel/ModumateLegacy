import datetime
import os
import re
import shutil
import sys

log_pat = re.compile(r'.*LogAutomationController: .*Test Completed.*'
                     'Result={(?P<result>\w+)}.*Name={(?P<shortname>.+)}.*Path={(?P<longname>.+)}')
success_result = "Passed"

def check_test_logs(log_path, build_id):
    passed_tests = []
    failed_tests = []
    file_exists = False

    with open(log_path) as log_file:
        file_exists = True
        for log_line in log_file:
            m = log_pat.match(log_line)
            if m:
                test_name = m.group('longname')
                result = m.group('result')

                if result == success_result:
                    passed_tests.append(test_name)
                else:
                    failed_tests.append(test_name)

                print("Test result for %s: %s" % (m.group('longname'), m.group('result')))

    if file_exists:
        path_base, path_ext = os.path.splitext(log_path)
        time_str = datetime.datetime.now().strftime("%Y.%m.%d-%H.%M.%S")
        log_copy_path = "%s-build-%s-%s%s" % (path_base, build_id, time_str, path_ext)
        shutil.copy(log_path, log_copy_path)

    return passed_tests, failed_tests

if __name__ == '__main__':
    if len(sys.argv) > 2:
        passed_tests, failed_tests = check_test_logs(sys.argv[1], sys.argv[2])
        num_passed_tests = len(passed_tests)
        num_failed_tests = len(failed_tests)

        if num_failed_tests > 0:
            print("ERROR: %d test(s) failed!" % num_failed_tests)
            sys.exit(1)
        elif num_passed_tests == 0:
            print("ERROR: no tests found!")
            sys.exit(2)
        else:
            print("SUCCESS: all %d tests passed!" % num_passed_tests)
    else:
        sys.exit(-1)
