import datetime
import os
import re
import shutil
import sys

log_pat = re.compile(r'.*LogAutomationController: .*Automation Test '
                     '(?P<result>\w+) \((?P<shortname>.+) - (?P<longname>.+)\)')
success_result = "Succeeded"

def check_test_logs(log_path, build_id):
    failed_tests = []
    file_exists = False

    with open(log_path) as log_file:
        file_exists = True
        for log_line in log_file:
            m = log_pat.match(log_line)
            if m:
                test_name = m.group('longname')
                result = m.group('result')

                if result != success_result:
                    failed_tests.append(test_name)

                print("Test result for %s: %s" % (m.group('longname'), m.group('result')))

    if file_exists:
        path_base, path_ext = os.path.splitext(log_path)
        time_str = datetime.datetime.now().strftime("%Y.%m.%d-%H.%M.%S")
        log_copy_path = "%s-build-%s-%s%s" % (path_base, build_id, time_str, path_ext)
        shutil.copy(log_path, log_copy_path)

    return failed_tests

if __name__ == '__main__':
    if len(sys.argv) > 2:
        failed_tests = check_test_logs(sys.argv[1], sys.argv[2])
        num_failed_tests = len(failed_tests)

        if num_failed_tests > 0:
            print("ERROR: %d tests failed!" % num_failed_tests)
            sys.exit(1)
        else:
            print("SUCCESS: all tests passed!")
    else:
        sys.exit(-1)
