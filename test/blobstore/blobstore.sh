#!/usr/bin/env bash
#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2017 Intel Corporation
#  All rights reserved.
#
SYSTEM=$(uname -s)
if [ $SYSTEM = "FreeBSD" ]; then
	echo "blobstore.sh cannot run on FreeBSD currently."
	exit 0
fi

testdir=$(readlink -f $(dirname $0))
rootdir=$(readlink -f $testdir/../..)
source $rootdir/test/common/autotest_common.sh

# Nvme0 target configuration
"$rootdir/scripts/gen_nvme.sh" --json-with-subsystems -n 1 > "$testdir/blobcli.json"

# generate random data file for import/export diff
dd if=/dev/urandom of=$testdir/test.pattern bs=1M count=1

(cd $testdir \
	&& $SPDK_EXAMPLE_DIR/blobcli -j $testdir/blobcli.json -b Nvme0n1 -T $testdir/test.bs > $testdir/btest.out)

# the test script will import the test pattern generated by dd and then export
# it to a file so we can compare and confirm basic read and write
$rootdir/test/app/match/match -v $testdir/btest.out.match
diff $testdir/test.pattern $testdir/test.pattern.blob

rm -rf $testdir/btest.out
rm -rf $testdir/blobcli.json
rm -rf $testdir/*.blob
rm -rf $testdir/test.pattern
