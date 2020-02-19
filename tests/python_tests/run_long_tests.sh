#!/bin/bash

if [ -z "$RUN_LONG_TESTS" ]; then
    echo "Variable RUN_LONG_TESTS is unset. Skipping."
    exit 0
fi

dir=$(dirname $0)

source $dir/run_tests.sh

execute_test nodeos_sanity_lr_test "nodeos_run_test.py -v --sanity-test --clean-run --dump-error-detail"
execute_test nodeos_sanity_bnet_lr_test "nodeos_run_test.py -v --sanity-test --p2p-plugin bnet --clean-run --dump-error-detail"
execute_test nodeos_run_check_lr_test "nodeos_run_test.py -v --clean-run --dump-error-detail"
execute_test nodeos_remote_lr_test "nodeos_run_remote_test.py -v --clean-run --dump-error-detail"
execute_test distributed_transactions_lr_test "distributed-transactions-test.py -d 2 -p 21 -n 21 -v --clean-run --dump-error-detail"
execute_test nodeos_forked_chain_lr_test "nodeos_forked_chain_test.py -v --wallet-port 9901 --clean-run --dump-error-detail"
execute_test nodeos_voting_lr_test "nodeos_voting_test.py -v --wallet-port 9902 --clean-run --dump-error-detail"
execute_test nodeos_voting_bnet_lr_test "nodeos_voting_test.py -v --wallet-port 9903 --p2p-plugin bnet --clean-run --dump-error-detail"
execute_test nodeos_under_min_avail_ram_lr_test "nodeos_under_min_avail_ram.py -v --wallet-port 9904 --clean-run --dump-error-detail"

exit $?
