#ifndef __ORCHARD_TEST_H__
#define __ORCHARD_TEST_H__

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "orchard.h"

#define TEST_NAME_LENGTH 16

typedef enum _OrchardTestType {
  orchardTestPoweron = 0,      // test run at power-on to confirm the block is good
  orchardTestTrivial,          // test if we can simply talk to a hardware block
  orchardTestComprehensive,    // a deeper test, as required
  orchardTestInteractive,      // tests that require interaction with factory labor
} OrchardTestType;

typedef enum _OrchardTestResult {
  orchardResultPass = 0,
  orchardResultFail,
  orchardResultUnsure,
  orchardResultNoTest,  // for cases where there is no test for a function
} OrchardTestResult;

/*
  my_name is the name assigned to the test per build system macro. This is used
    to store info about the test in the test audit log.

  test_type is a number that represents the type of test to run. This is by convention, 
    and examples are "simple connectivity" test vs. "comprehensive test". Some blocks 
    may not required the comprehensive test, as they yield well enough, others may.
 */

typedef OrchardTestResult (*testroutine_t)(const char *my_name, OrchardTestType test_type);

typedef struct {
  const char         *test_name;     
  testroutine_t      test_function;  
} TestRoutine;

// test get sorted in memory alphabetically by test_name at link time

#define orchard_test_start() \
({ \
  static char start[0] __attribute__((unused,  \
    aligned(4), section(".chibi_list_test_1")));        \
  (const TestRoutine *)&start;            \
})

#define orchard_test(_name, _func) \
  const TestRoutine _orchard_test_list_##_func \
  __attribute__((unused, aligned(4), section(".chibi_list_test_2_" _name))) = \
     { _name, _func }

#define orchard_test_end() \
  const TestRoutine _orchard_test_list_##_func \
  __attribute__((unused, aligned(4), section(".chibi_list_test_3_end"))) = \
     { NULL, NULL }

void orchardTestInit(void);
const TestRoutine *orchardGetTestByName(const char *name);
void orchardTestRun(uint32_t test_type);
void orchardListTests(void);

#endif /* __ORCHARD_TEST_H__ */
