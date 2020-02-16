#define _GNU_SOURCE

#include <glib.h>

#include <core/frame.h>
#include <core/stacktrace.h>
#include <core/thread.h>
#include <core/unwind.h>
#include <err.h>
#include <errno.h>
#include <utils.h>
#include <internal_unwind.h>
#include <stacktrace.h>
#include <stdio.h>
#include <strbuf.h>
#include <sys/wait.h>
#include <unistd.h>

static char const *test_json =
    "{   \"signal\": 9\n"
    ",   \"stacktrace\":\n"
    "      [ {   \"frames\":\n"
    "              [ {   \"address\": 68719476722\n"
    "                ,   \"build_id\": \"aabbccddeeff2\"\n"
    "                ,   \"build_id_offset\": 2562\n"
    "                ,   \"function_name\": \"test2\"\n"
    "                ,   \"file_name\": \"executable2\"\n"
    "                ,   \"fingerprint\": \"ab\"\n"
    "                }\n"
    "              , {   \"address\": 68719476723\n"
    "                ,   \"build_id\": \"aabbccddeeff3\"\n"
    "                ,   \"build_id_offset\": 2563\n"
    "                ,   \"function_name\": \"test3\"\n"
    "                ,   \"file_name\": \"executable3\"\n"
    "                } ]\n"
    "        }\n"
    "      , {   \"crash_thread\": true\n"
    "        ,   \"frames\":\n"
    "              [ {   \"address\": 68719476721\n"
    "                ,   \"build_id\": \"aabbccddeeff1\"\n"
    "                ,   \"build_id_offset\": 2561\n"
    "                ,   \"function_name\": \"test1\"\n"
    "                ,   \"file_name\": \"executable1\"\n"
    "                ,   \"fingerprint\": \"ab\"\n"
    "                }\n"
    "              , {   \"address\": 68719476720\n"
    "                ,   \"build_id\": \"aabbccddeeff0\"\n"
    "                ,   \"build_id_offset\": 2560\n"
    "                ,   \"function_name\": \"test0\"\n"
    "                ,   \"file_name\": \"executable0\"\n"
    "                } ]\n"
    "        } ]\n"
    "}";
static char const *dump_core_program = "dump_core";

static void
test_core_stacktrace_to_json(void)
{
    struct sr_core_thread *threads[2];
    struct sr_core_frame *frames[4];
    struct sr_core_stacktrace *stacktrace;
    g_autofree char *json = NULL;
    g_autofree char *json2 = NULL;

    for (size_t i = 0; i < G_N_ELEMENTS(threads); i++)
    {
        threads[i] = sr_core_thread_new();
    }
    for (size_t i = 0; i < G_N_ELEMENTS(frames); i++)
    {
        frames[i] = sr_core_frame_new();
    }

    threads[0]->frames = frames[1];

    threads[1]->frames = frames[3];
    threads[1]->next = threads[0];

    frames[0]->address = 0xffffffff0;
    frames[0]->build_id = sr_strdup("aabbccddeeff0");
    frames[0]->build_id_offset = 2560;
    frames[0]->function_name = sr_strdup("test0");
    frames[0]->file_name = sr_strdup("executable0");

    frames[1]->address = 0xffffffff1;
    frames[1]->build_id = sr_strdup("aabbccddeeff1");
    frames[1]->build_id_offset = 2561;
    frames[1]->function_name = sr_strdup("test1");
    frames[1]->file_name = sr_strdup("executable1");
    frames[1]->fingerprint = sr_strdup("ab");
    frames[1]->next = frames[0];

    frames[2]->address = 0xffffffff3;
    frames[2]->build_id = sr_strdup("aabbccddeeff3");
    frames[2]->build_id_offset = 2563;
    frames[2]->function_name = sr_strdup("test3");
    frames[2]->file_name = sr_strdup("executable3");

    frames[3]->address = 0xffffffff2;
    frames[3]->build_id = sr_strdup("aabbccddeeff2");
    frames[3]->build_id_offset = 2562;
    frames[3]->function_name = sr_strdup("test2");
    frames[3]->file_name = sr_strdup("executable2");
    frames[3]->fingerprint = sr_strdup("ab");
    frames[3]->next = frames[2];

    stacktrace = sr_core_stacktrace_new();

    stacktrace->signal = SIGKILL;
    stacktrace->crash_thread = threads[0];
    stacktrace->threads = threads[1];

    {
        g_autofree char *reason = NULL;

        reason = sr_core_stacktrace_get_reason(stacktrace);

        g_assert_nonnull(reason);
    }
    {
        g_autofree char *reason = NULL;

        reason = sr_stacktrace_get_reason((struct sr_stacktrace *) stacktrace);

        g_assert_nonnull(reason);
    }

    json = sr_core_stacktrace_to_json(stacktrace);

    g_assert_cmpstr(json, ==, test_json);

    json2 = sr_stacktrace_to_json((struct sr_stacktrace *) stacktrace);

    g_assert_cmpstr(json, ==, json2);

    sr_core_stacktrace_free(stacktrace);
}

static void
test_core_stacktrace_from_json_check(struct sr_core_stacktrace *stacktrace)
{
    struct sr_core_frame *frame;

    g_assert_nonnull(stacktrace);
    g_assert_cmpint(stacktrace->signal, ==, SIGABRT);
    g_assert_nonnull(stacktrace->executable);
    g_assert_cmpstr(stacktrace->executable, ==, "/usr/bin/will_abort");
    g_assert_nonnull(stacktrace->threads);

    assert(stacktrace->threads->next);
    assert(stacktrace->crash_thread == stacktrace->threads->next);

    frame = stacktrace->threads->frames;

    assert(frame->address == 87654);
    assert(0 == strcmp(frame->build_id, "92ebaf825e4f492952c45189cb9ffc6541f8599b"));
    assert(frame->build_id_offset == 42);
    assert(0 == strcmp(frame->function_name, "fake_frame_for_the_sake_of_multiple_threads"));
    assert(0 == strcmp(frame->file_name, "/usr/bin/will_abort"));
    assert(frame->fingerprint == NULL);

    frame = frame->next;
    assert(frame->address == 98346);
    assert(0 == strcmp(frame->build_id, "92ebaf825e4f492952c45189cb9ffc6541f8599b"));
    assert(frame->build_id_offset == 10734);
    assert(0 == strcmp(frame->function_name, "fake_function"));
    assert(0 == strcmp(frame->file_name, "/usr/bin/will_abort"));
    assert(frame->fingerprint == NULL);

    frame = stacktrace->threads->next->frames;
    assert(frame->address == 237190207797);
    assert(0 == strcmp(frame->build_id, "cc10c72da62c93033e227ffbe2670f2c4fbbde1a"));
    assert(frame->build_id_offset == 219445);
    assert(0 == strcmp(frame->function_name, "raise"));
    assert(0 == strcmp(frame->file_name, "/usr/lib64/libc-2.15.so"));
    assert(0 == strcmp(frame->fingerprint, "f33186a4c862fb0751bca60701f553b829210477"));

    frame = frame->next->next->next;
    assert(frame->address == 237190125365);
    assert(0 == strcmp(frame->build_id, "cc10c72da62c93033e227ffbe2670f2c4fbbde1a"));
    assert(frame->build_id_offset == 137013);
    assert(0 == strcmp(frame->function_name, "__libc_start_main"));
    assert(0 == strcmp(frame->file_name, "/usr/lib64/libc-2.15.so"));
    assert(0 == strcmp(frame->fingerprint, "5ff4a3d3743fdde711c49033d1e01784477e57fb"));
}

static void
test_core_stacktrace_from_json(void)
{
    char *full_input;
    struct sr_core_stacktrace *core_stacktrace;
    struct sr_stacktrace *stacktrace;

    full_input = sr_file_to_string("json_files/core-01", NULL);
    core_stacktrace = sr_core_stacktrace_from_json_text(full_input, NULL);
    stacktrace = sr_stacktrace_parse(SR_REPORT_CORE, full_input, NULL);

    test_core_stacktrace_from_json_check(core_stacktrace);
    test_core_stacktrace_from_json_check((struct sr_core_stacktrace *)stacktrace);

    sr_stacktrace_free(stacktrace);
    sr_core_stacktrace_free(core_stacktrace);
}

static char *
generate_coredump(int start)
{
  if (start)
  {
      return generate_coredump(--start);
  }

  const char* coredump_pattern = "/tmp/satyr.core";

  char *mypid = NULL;
  asprintf(&mypid, "%d", getpid());
  if (mypid == NULL)
      err(1, "asprintf");

  pid_t pid = fork();
  if (pid < 0)
      err(1, "fork");
  if (pid == 0)
  {
      char *args[] = { "gcore", "-o", (char *)coredump_pattern, mypid, NULL };
      execv("/usr/bin/gcore", args);
  }

  int status;
  int r;
  while ((r = waitpid(pid, &status, 0)) < 0)
  {
      if (errno != EAGAIN)
          err(1, "waitpid");
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      errx(1, "gcore failed");

  return sr_asprintf("%s.%s", coredump_pattern, mypid);
}

GString *
run_and_get_stdout(char const **argv)
{
    int p[2];
    pid_t pid;
    g_autoptr (GString) output = NULL;
    char buf[256];
    ssize_t size;
    int status;

    if (pipe(p) == -1)
    {
        err(1, "pipe");
    }

    pid = fork();
    if (-1 == pid)
    {
        err(1, "fork");
    }
    if (0 == pid)
    {
        close(p[0]);

        dup2(p[1], STDOUT_FILENO);
        dup2(p[1], STDERR_FILENO);

        close(p[1]);

        execv(argv[0], (char *const *) argv);
    }

    close(p[1]);

    output = g_string_new(NULL);

    do
    {
        size = read(p[0], buf, sizeof(buf) - 1);
        if (-1 == size)
        {
            if (EINTR == errno)
            {
                continue;
            }
            break;
        }

        buf[size] = '\0';

        g_string_append(output, buf);
    } while (0 != size);

    printf("%s\n", output->str);

    if (waitpid(pid, &status, 0) == -1)
    {
        err(1, "waitpid");
    }

    if (!WIFEXITED(status))
    {
        errx(1, "!WIFEXITED");

        return NULL;
    }

    if (WEXITSTATUS(status) != 0)
    {
        errx(1, "WEXITSTATUS: %d", WEXITSTATUS(status));

        return NULL;
    }

    return g_steal_pointer (&output);
}

static void
test_core_stacktrace_from_gdb_limit(void)
{
    g_autoptr(GString) coredump_path = NULL;
    char const *gdb_path = "/usr/libexec/gdb";
    g_autofree char *file = NULL;
    g_autofree char *core_file = NULL;
    g_autoptr(GString) gdb_output = NULL;
    char *error_msg = NULL;
    struct sr_core_stacktrace *core_stacktrace;
    struct sr_core_frame *frame;
    size_t frames;

    coredump_path = run_and_get_stdout((char const *[]) {
        dump_core_program,
        "257",
        NULL,
    });

    g_assert_nonnull(coredump_path);
    g_assert_cmpuint(strlen(coredump_path->str), >, 0);

    if (access("/usr/bin/gdb", F_OK) != -1)
    {
        gdb_path = "/usr/bin/gdb";
    }

    file = g_strdup_printf("file %s", dump_core_program);
    core_file = g_strdup_printf("core-file %s", coredump_path->str);

    gdb_output = run_and_get_stdout((char const *[]) {
        gdb_path,
        "-batch",
        "-iex", "set debug-file-directory /",
        "-ex", file,
        "-ex", core_file,
        "-ex", "thread apply all -ascending backtrace full 1024",
        "-ex", "info sharedlib",
        "-ex", "print (char*)__abort_msg",
        "-ex", "print (char*)__glib_assert_msg",
        "-ex", "info all-registers",
        "-ex", "disassemble",
        NULL,
    });

    g_assert_nonnull(gdb_output);
    g_assert_cmpuint(strlen(gdb_output->str), >, 0);

    core_stacktrace = sr_core_stacktrace_from_gdb(gdb_output->str, coredump_path->str,
                                                  dump_core_program, &error_msg);
    g_assert_cmpstr (error_msg, ==, NULL);

    frame = core_stacktrace->threads->frames;
    frames = 0;

    while (NULL != frame)
    {
        frames++;

        frame = frame->next;
    }

    g_assert_cmpuint(frames, ==, CORE_STACKTRACE_FRAME_LIMIT);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/stacktrace/core/to-json", test_core_stacktrace_to_json);
    g_test_add_func("/stacktrace/core/from-json", test_core_stacktrace_from_json);
    g_test_add_func("/stacktrace/core/from-gdb-limit", test_core_stacktrace_from_gdb_limit);

    return g_test_run();
}
