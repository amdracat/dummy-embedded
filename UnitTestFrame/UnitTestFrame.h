#ifndef TEST_FRAME_H
#define TEST_FRAME_H

#include <stdio.h>

void UnitTestFrame_Init(void);
void UnitTestFrame_ReportResult(void);
void UnitTestFrame_AssertEqInt(int expected, int actual, const char *file, int line);

#define ASSERT_EQ(expected, actual) \
    UnitTestFrame_AssertEqInt((expected), (actual), __FILE__, __LINE__)

#endif /* TEST_FRAME_H */

