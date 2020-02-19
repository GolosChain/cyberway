#!/bin/bash

dir=$(dirname $0)

source $dir/run_tests.sh

execute_test nodeos_sanity_test "nodeos_run_test.py -v --sanity-test --clean-run --dump-error-detail"
execute_test nodeos_sanity_bnet_test "nodeos_run_test.py -v --sanity-test --clean-run --p2p-plugin bnet --dump-error-detail"
execute_test nodeos_run_test "nodeos_run_test.py -v --clean-run --dump-error-detail"
execute_test nodeos_run_bnet_test "nodeos_run_test.py -v --clean-run --p2p-plugin bnet --dump-error-detail"
execute_test nodeos_run_test-mongodb "nodeos_run_test.py --mongodb -v --clean-run --dump-error-detail"
execute_test distributed-transactions-test "distributed-transactions-test.py -d 2 -p 4 -n 6 -v --clean-run --dump-error-detail"
execute_test distributed-transactions-bnet-test "distributed-transactions-test.py -d 2 -p 1 -n 4 --p2p-plugin bnet -v --clean-run --dump-error-detail"
execute_test restart-scenarios-test-resync "restart-scenarios-test.py -c resync -p4 -v --clean-run --dump-error-details"
#execute_test restart-scenarios-test-hard_replay "restart-scenarios-test.py -c hardReplay -p4 -v --clean-run --dump-error-details"
#execute_test restart-scenarios-test-none "restart-scenarios-test.py -c none --kill-sig term -p4 -v --clean-run --dump-error-details"
#execute_test consensus-validation-malicious-producers "consensus-validation-malicious-producers.py -w 80 --dump-error-details"
execute_test validate_dirty_db_test "validate-dirty-db.py -v --clean-run --dump-error-detail"
execute_test launcher_test "launcher_test.py -v --clean-run --dump-error-detail"

exit $status
