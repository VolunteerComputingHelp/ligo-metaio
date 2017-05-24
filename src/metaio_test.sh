#!/bin/sh

OVERALL_PASS=1

check_pass () {
  cmd=$*
  r=`sh -c "$cmd"`
  result=$?
  if [ $result -eq 0 ]; then
    echo "PASS: $cmd"
  else
    echo "FAIL: $cmd: $r"
    OVERALL_PASS=0
  fi
  return
}

check_fail () {
  cmd=$*
  r=`sh -c "$cmd"`
  result=$?
  if [ $result -ne 0 ]; then
    echo "PASS: $cmd gave error"
  else
    echo "FAIL: $cmd did not give error"
    OVERALL_PASS=0
  fi
  return
}

# because lwtcut can't write XML to stdout
METAIO_TESTS_PIPE=/tmp/metaio_tests.pipe
mkfifo $METAIO_TESTS_PIPE

echo "-- Basic tests"
check_pass "./parse_test -q ${srcdir}/gdstrig10.xml"
check_pass "./parse_test -q ${srcdir}/gdstrig5000.xml"
check_pass "gunzip < ${srcdir}/glueligolw_sample.xml.gz | ./parse_test -q /dev/stdin"
check_pass "./parse_test_table_only -q ${srcdir}/gdstrig10.xml"
check_pass "./parse_test_table_only -q ${srcdir}/gdstrig5000.xml"
check_pass "./lwtcut ${srcdir}/gdstrig10.xml -o $METAIO_TESTS_PIPE & diff $METAIO_TESTS_PIPE ${srcdir}/gdstrig10.xml.lwtcut_output"
check_pass "./lwtcut ${srcdir}/dmt_sample.xml -o $METAIO_TESTS_PIPE & diff $METAIO_TESTS_PIPE ${srcdir}/dmt_sample.xml.lwtcut_output"
check_pass "gunzip < ${srcdir}/blobtest.xml.gz | ./lwtcut /dev/stdin -o $METAIO_TESTS_PIPE & diff $METAIO_TESTS_PIPE ${srcdir}/blobtest.xml.lwtcut_output"
check_pass "./lwtdiff ${srcdir}/gdstrig10.xml ${srcdir}/gdstrig10.xml"
check_pass "./lwtprint ${srcdir}/gdstrig10.xml | diff - ${srcdir}/gdstrig10.xml.lwtprint_output"
check_pass "./lwtscan ${srcdir}/gdstrig10.xml"
check_pass "./lwtscan ${srcdir}/gdstrig10.xml -t row"
check_pass "./lwtscan ${srcdir}/gdstrig10.xml -t row2"
check_pass "./lwtscan ${srcdir}/gdstrig10.xml -t row3"

if test x${HAVE_LIBZ} = "xyes" ; then
  echo "-- Zlib compression tests"
  check_pass "./parse_test -q ${srcdir}/gdstrig10.xml.gz"
  check_pass "./parse_test -q ${srcdir}/gdstrig5000.xml.gz"
  check_pass "./parse_test_table_only -q ${srcdir}/gdstrig10.xml"
  check_pass "./parse_test_table_only -q ${srcdir}/gdstrig5000.xml"
  check_pass "./lwtcut ${srcdir}/gdstrig10.xml.gz -o $METAIO_TESTS_PIPE & diff $METAIO_TESTS_PIPE ${srcdir}/gdstrig10.xml.lwtcut_output"
  check_pass "gzip < ${srcdir}/dmt_sample.xml | ./lwtcut /dev/stdin -o $METAIO_TESTS_PIPE & diff $METAIO_TESTS_PIPE ${srcdir}/dmt_sample.xml.lwtcut_output"
  check_pass "./lwtcut ${srcdir}/blobtest.xml.gz -o $METAIO_TESTS_PIPE & diff $METAIO_TESTS_PIPE ${srcdir}/blobtest.xml.lwtcut_output"
  check_pass "./lwtdiff ${srcdir}/gdstrig5000.xml.gz ${srcdir}/gdstrig5000.xml.gz"
  check_pass "./lwtdiff ${srcdir}/gdstrig5000.xml.gz ${srcdir}/gdstrig5000.xml"
  check_pass "./lwtprint ${srcdir}/gdstrig10.xml.gz | diff - ${srcdir}/gdstrig10.xml.lwtprint_output"
  check_pass "./lwtscan ${srcdir}/gdstrig10.xml.gz"
  check_pass "./lwtscan ${srcdir}/gdstrig10.xml.gz -t row"
  check_pass "./lwtscan ${srcdir}/gdstrig10.xml.gz -t row2"
  check_pass "./lwtscan ${srcdir}/gdstrig10.xml.gz -t row3"
fi

echo "-- Specific tests"
check_pass "./lwtcut ${srcdir}/gdstrig5000.xml -t row 'SIGNIFICANCE > 2' -r 3-"
check_pass "./lwtdiff ${srcdir}/gdstrig5000.xml ${srcdir}/gdstrig5000.xml -c START_TIME -t row"
check_pass "./lwtprint ${srcdir}/gdstrig5000.xml -r 8-12 -c IFO,START_TIME,FREQUENCY,SIZE -t row"
check_pass "./lwtscan ${srcdir}/gdstrig5000.xml"
check_pass "./lwtscan ${srcdir}/gdstrig5000.xml -t row"


echo "-- Failure tests"
check_fail "./lwtcut doesntexist.xml"
check_fail "./lwtdiff doesntexist.xml ${srcdir}/gdstrig10.xml"
check_fail "./lwtdiff ${srcdir}/gdstrig5000.xml ${srcdir}/gdstrig10.xml"
check_fail "./lwtprint doesntexist.xml"
check_fail "./lwtscan doesntexist.xml"
check_fail "./lwtscan ${srcdir}/gdstrig10.xml -t foo"

check_fail "./lwtcut ${srcdir}/gdstrig5000.xml -t row 'SIG > 2' -r 3-"
check_fail "./lwtdiff ${srcdir}/gdstrig10.xml ${srcdir}/gdstrig10.xml -c NO_COLUMN -t row"
check_fail "./lwtprint ${srcdir}/gdstrig10.xml -r foo-bar"


rm -f $METAIO_TESTS_PIPE

if [ $OVERALL_PASS -eq 1 ]; then
  echo "PASS: All tests passed"
  exit 0
else
  echo "FAIL: Some tests failed"
  exit 1
fi
