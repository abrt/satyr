#include "lib/java_stacktrace.h"
#include "lib/java_thread.h"
#include "lib/java_frame.h"
#include "lib/utils.h"

#define XYZ_JTC_UT_STACKTRACE "java.lang.RuntimeException: java.lang.NullPointerException: null\n"\
"\tat SimpleTest.throwNullPointerException(SimpleTest.java:36)\n"\
"\tat SimpleTest.throwAndDontCatchException(SimpleTest.java:70)\n"\
"\tat SimpleTest.main(SimpleTest.java:82)\n"\
"Caused by: java.lang.NullPointerException: java.lang.InvalidRangeException: undefined index\n"\
"\tat SimpleTest.execute(Test.java:7)\n"\
"\tat SimpleTest.intercept(Test.java:2)\n"\
"\t... 3 more\n"\
"Caused by: java.lang.InvalidRangeException: undefined index\n"\
"\tat MyVector.at(Containers.java:77)\n"\
"\t... 5 more\n";

struct btp_java_frame
*create_real_stacktrace_top()
{
  struct btp_java_frame frame0, frame1, frame2;
  btp_java_frame_init(&frame0);
  frame0.name = "SimpleTest.throwNullPointerException";
  frame0.file_name = "SimpleTest.java";
  frame0.file_line = 36;
  frame0.next = &frame1;

  btp_java_frame_init(&frame1);
  frame1.name = "SimpleTest.throwAndDontCatchException";
  frame1.file_name = "SimpleTest.java";
  frame1.file_line = 70;
  frame1.next = &frame2;

  btp_java_frame_init(&frame2);
  frame2.name = "SimpleTest.main";
  frame2.file_name = "SimpleTest.java";
  frame2.file_line = 82;

  return btp_java_frame_dup(&frame0, true);
}

struct btp_java_frame
*create_real_stacktrace_middle()
{
  struct btp_java_frame frame3, frame4;
  btp_java_frame_init(&frame3);
  frame3.name = "SimpleTest.execute";
  frame3.file_name = "Test.java";
  frame3.file_line = 7;
  frame3.next = &frame4;

  btp_java_frame_init(&frame4);
  frame4.name = "SimpleTest.intercept";
  frame4.file_name = "Test.java";
  frame4.file_line = 2;

  return btp_java_frame_dup(&frame3, true);
}

struct btp_java_frame
*create_real_stacktrace_bottom()
{
  struct btp_java_frame frame5;
  btp_java_frame_init(&frame5);
  frame5.name = "MyVector.at";
  frame5.file_name = "Containers.java";
  frame5.file_line = 77;

  return btp_java_frame_dup(&frame5, true);
}

struct btp_java_frame
*create_real_stacktrace_objects()
{
  struct btp_java_frame *exception0, *exception1, *exception2;

  exception0 = btp_java_frame_new_exception();
  exception0->name = btp_strdup("java.lang.RuntimeException");
  exception0->message = btp_strdup("java.lang.InvalidRangeException: undefined index");
  exception0->next = create_real_stacktrace_top();

  exception1 = btp_java_frame_new_exception();
  exception1->name = btp_strdup("java.lang.NullPointerException");
  exception1->message = btp_strdup("null");
  exception1->next = create_real_stacktrace_middle();
  btp_java_frame_get_last(exception1->next)->next = exception0;

  exception2 = btp_java_frame_new_exception();
  exception2->name = btp_strdup("java.lang.InvalidRangeException");
  exception2->message = btp_strdup("null");
  exception2->next = create_real_stacktrace_bottom();
  btp_java_frame_get_last(exception2->next)->next = exception1;

  return exception2;
}

struct btp_java_thread
*create_real_main_thread_objects()
{
    struct btp_java_thread *thread;
    thread = btp_java_thread_new();
    thread->name = btp_strdup("main");
    thread->frames = create_real_stacktrace_objects();

    return thread;
}

const char *get_real_stacktrace_str()
{
    return XYZ_JTC_UT_STACKTRACE;
}

const char *get_real_thread_stacktrace()
{
    return "Exception in thread \"main\" " XYZ_JTC_UT_STACKTRACE;
}
