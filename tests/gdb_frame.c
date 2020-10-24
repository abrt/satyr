#include "gdb/frame.h"
#include "location.h"
#include <stdio.h>
#include <glib.h>

static void
test_gdb_frame_dup(void)
{
    struct sr_gdb_frame *frame1 = sr_gdb_frame_new();;
    frame1->function_name = g_strdup("test1");
    frame1->function_type = g_strdup("type1");
    frame1->number = 10;
    frame1->source_file = g_strdup("file1");
    frame1->source_line = 11;
    frame1->address = 12;

    struct sr_gdb_frame *frame0 = sr_gdb_frame_new();;
    frame0->function_name = g_strdup("test0");
    frame0->function_type = g_strdup("type0");
    frame0->number = 13;
    frame0->source_file = g_strdup("file0");
    frame0->source_line = 14;
    frame0->address = 15;
    frame0->next = frame1;

    /* Test the duplication without siblings. */
    struct sr_gdb_frame *frame = sr_gdb_frame_dup(frame0, false);
    g_assert_null(frame->next);
    g_assert_false(frame->function_name == frame0->function_name);
    g_assert_false(frame->function_type == frame0->function_type);
    g_assert_false(frame->source_file == frame0->source_file);
    g_assert_cmpint(sr_gdb_frame_cmp(frame, frame0, true), ==, 0);
    sr_gdb_frame_free(frame);

    /* Test the duplication with the siblings. */
    frame = sr_gdb_frame_dup(frame0, true);
    g_assert_false(frame->function_name == frame0->function_name);
    g_assert_false(frame->function_type == frame0->function_type);
    g_assert_false(frame->source_file == frame0->source_file);
    g_assert_cmpint(sr_gdb_frame_cmp(frame, frame0, true), ==, 0);
    g_assert_true(frame->next != frame1);
    g_assert_cmpint(sr_gdb_frame_cmp(frame->next, frame1, true), ==, 0);
    sr_gdb_frame_free(frame->next);
    sr_gdb_frame_free(frame);

    sr_gdb_frame_free(frame1);
    sr_gdb_frame_free(frame0);
}

static void
test_gdb_frame_parse_frame_start(void)
{
    /**
     * @param parsed_char_count
     * The expected number of characters parsed (taken) from input.
     * @param expected_frame_number
     * The expected parsed frame number.
     * @param input
     * The input text stream.
     */
    struct test_data
    {
        char *input;
        int parsed_char_count;
        unsigned expected_frame_number;
    };

    struct test_data t[] = {
        {"#10 "    , 4, 10},
        {"#0  "    , 4, 0},
        {"#99999  ", 8, 99999},
        {"ab "     , 0, 0},
        {"#ab "    , 0, 0},
        {"#-9999 " , 0, 9999},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        int number;
        char *old_input = t[i].input;
        g_assert_cmpuint(t[i].parsed_char_count, ==, sr_gdb_frame_parse_frame_start((const char **)&t[i].input, &number));
        if (0 < t[i].parsed_char_count)
        {
            g_assert_cmpint(number, ==, t[i].expected_frame_number);
            g_assert_cmpuint(*t[i].input, ==, '\0');
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
        }
    }
}

static void
test_gdb_frame_parseadd_operator(void)
{
    struct test_data
    {
        char *input;
        int parsed_length;
    };

    struct test_data t[] = {
        {"operator>", strlen("operator>")},
        {"operator->", strlen("operator->")},
        {"operator new", strlen("operator new")},
        {"operator new[]", strlen("operator new[]")},
        {"operator delete", strlen("operator delete")},
        {"operator del", 0},
        {"operator delete[] (test)", strlen("operator delete[]")},
        /* Red Hat Bugzilla bug #542445 */
        {"cairo_add_operator (test)", 0},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        printf("Testing '%s' -> %d\n", t[i].input, t[i].parsed_length);
        char *old_input = t[i].input;
        GString *strbuf = g_string_new(NULL);
        g_assert_cmpuint(t[i].parsed_length, ==, sr_gdb_frame_parseadd_operator((const char **)&t[i].input, strbuf));
        printf("  input = '%s', old_input = '%s'\n", t[i].input, old_input);

        /* Check that the input pointer was updated properly. */
        g_assert_true(*t[i].input == old_input[t[i].parsed_length]);

        /* Check that the strbuf has been modified accordingly to what was parsed. */
        g_assert_cmpint(strncmp(strbuf->str, old_input, t[i].parsed_length), ==, 0);
        g_assert_cmpuint(strbuf->len, ==, t[i].parsed_length);

        g_string_free(strbuf, TRUE);
    }

}

static void
test_gdb_frame_parse_function_name(void)
{
    struct test_data
    {
        bool success;
        char *input;
    };

    struct test_data t[] = {
        {true, "??"},
        {true, "IA__g_bookmark_file_to_file"},
        {true, "pthread_cond_timedwait@@GLIBC_2.3.2"},
        {true, "_pixman_walk_composite_region"},
        {true, "CairoOutputDev::tilingPatternFill"},
        {true, "sdr::(anonymous namespace)::ViewContact::~ViewContact"},
        {true, "operator==<nsIAtom, nsICSSPseudoClass>"},
        {true, "wait_until<std::unique_lock<bmalloc::Mutex>, std::chrono::_V2::system_clock, std::chrono::duration<long int, std::ratio<1l, 1000000000l> >, bmalloc::AsyncTask<Object, Function>::entryPoint() [with Object = bmalloc::Heap; Function = void (bmalloc::Heap::*)()]::<lambda()> >"},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        /* Function name must be ended with a space. */
        char *input_with_space = g_malloc(strlen(t[i].input) + 2);
        strcpy(input_with_space, t[i].input);
        input_with_space[strlen(t[i].input)] = ' ';
        input_with_space[strlen(t[i].input) + 1] = '\0';

        char *function_name = NULL, *function_type = NULL;
        char *old_input_with_space = input_with_space;
        printf("Parsing '%s'\n", t[i].input);
        struct sr_location location;
        sr_location_init(&location);
        g_assert_true(t[i].success == sr_gdb_frame_parse_function_name((const char **)&input_with_space,
                                                                       &function_name,
                                                                       &function_type,
                                                                       &location));

        if (t[i].success)
        {
          g_assert_nonnull(function_name);
          printf("Function name '%s'\n", function_name);
          g_assert_cmpstr(function_name, ==, t[i].input);
          g_assert_null(function_type);
          free(function_name);
          g_assert_cmpuint(*input_with_space, ==, ' ');
        }
        else
        {
          /* Check that the pointer is not moved. */
          g_assert_true(old_input_with_space == input_with_space);
        }

        free(old_input_with_space);
    }
}

static void
test_gdb_frame_parse_function_name_template_args(void)
{
    struct test_data
    {
        const char *input;
        size_t len;
    };

    struct test_data t[] = {
        {" []", 0},
        {" [with ", 0},
        {" [with [", 0},
        {" [within ]", 0},

        {" [with [[[]]]", 0},
        {" [with [[[]]]", 0},
        {" [with [a][b][c][d][e]", 0},

        /* -1 == succeeds and read full input */
        {" [with ]", -1},
        {" [with ]]", strlen(" [with ]")},

        {" [with [a][b][c][d]]", -1},
        {" [with [f = [ d([g[e]])]]]", -1},
        {" [with j() = [h<[i]>]; k = [l, m, n~[o]]]", -1},

        {" [with Func = boost:graphs::bfs<tr1::distance([blah])>()]", -1},
        {" [with Object = std::map<std::string, std::string>; Func = boost:graphs::bfs<tr1::distance([blah])>(}, Param = int]", -1},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        if (t[i].len == -1)
            t[i].len = strlen(t[i].input);

        char *local_input = g_strdup(t[i].input);
        char *old_input = local_input;

        char *namechunk = (char *)0xDEADBEEF;
        size_t chars = sr_gdb_frame_parse_function_name_template_args((const char **)&local_input, &namechunk);

        g_assert_false(t[i].len == 0 && chars != 0);
        g_assert_false(t[i].len != 0  && chars == 0);
        g_assert_false(t[i].len != 0 && chars != t[i].len);
        g_assert_false(chars != 0 && namechunk == (char *)0xDEADBEEF);
        g_assert_false(chars == 0 && namechunk != (char *)0xDEADBEEF);
        g_assert_false(chars != 0 && strncmp(namechunk, t[i].input, t[i].len) != 0);

        if (namechunk != (char *)0xDEADBEEF)
        {
            free(namechunk);
        }

        free(old_input);
    }
}

static void
test_gdb_frame_skip_function_args(void)
{
    struct test_data
    {
        bool success;
        char *input;
    };

    struct test_data t[] = {
        /* minimal */
        {true, "()"},
        /* newline */
        {true, "(\n"
               "page=0x7f186003e280, \n"
               "cairo=0x7f18600efd10, printing=0)"},
        /* value optimized out */
        {true, "(this=0x7f1860023400, DPI=<value optimized out>)"},
        /* string */
        {true, "(filename=0x18971b0 \"/home/jfclere/.recently-used.xbel\")"},
        /* problematic frame #33 from rhbz#803600 */
        {true, "(func=<function at remote 0x1b3aa28>, arg=(<vmmAsyncJob(show_progress=False, topwin=<gtk.Window at remote 0x1b43690>, cancel_args=[<...>], _gobject_handles=[], _signal_id_map={}, cancel_job=None, window=<gtk.glade.XML at remote 0x1b43aa0>, _gobject_timeouts=[], config=<vmmConfig(rhel6_defaults=False, support_threading=True, glade_dir='/usr/share/virt-manager', _spice_error=None, conf_dir='/apps/virt-manager', appname='virt-manager', libvirt_packages=['libvirt'], support_inspection=False, keyring=None, appversion='0.9.0', preferred_distros=['rhel', 'fedora'], hv_packages=['qemu-kvm'], conf=<gconf.Client at remote 0x17ea9b0>, _objects=@<:@'<vmmErrorDialog object at 0x1885460 (virtManager+error+vmmErrorDialog at 0xfe9080)>', '<vmmEngine object at 0x1acaaf0 (virtManager+engine+vmmEngine at 0xfe8920)>', '<vmmErrorDialog object at 0x1acacd0 (virtManager+error+vmmErrorDialog at 0xfe2000)>', '<vmmSystray object at 0x1acad20 (virtManager+systray+vmmSystray at 0xfe8e60)>', '<vmmErrorDialog object at 0x1acad70 (virtManager+error+vmmErrorDialog at 0xd9b120)>', '<vm...(truncated), kw={})"},
        /* TODO: parentesis balance */
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        struct sr_location location;
        sr_location_init(&location);
        g_assert_true(t[i].success == sr_gdb_frame_skip_function_args((const char **)&t[i].input, &location));
        if (t[i].success)
        {
            g_assert_cmpuint(*t[i].input, ==, '\0');
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
        }
    }
}

static void
test_gdb_frame_parse_function_call(void)
{
    struct test_data
    {
        bool success;
        char *input;
        char *expected_function_name;
        char *expected_function_type;
    };

    #define TYPE "void"
    #define FUNCTION "boost::throw_exception<"                          \
                     "boost::filesystem::basic_filesystem_error<"       \
                     "boost::filesystem::basic_path<"                   \
                     "std::basic_string<"                               \
                     "char, std::char_traits<char>, "                   \
                     "std::allocator<char> >, "                         \
                     "boost::filesystem::path_traits> > >"              \
                     "(boost::filesystem::basic_filesystem_error<"      \
                     "boost::filesystem::basic_path<"                   \
                     "std::basic_string<char, std::char_traits<char>, " \
                     "std::allocator<char> >, "                         \
                     "boost::filesystem::path_traits> > const&)"
    #define ARGS "()"
    #define FUNCALL TYPE " " FUNCTION " " ARGS

    struct test_data t[] = {
        /* minimal */
        {true, "?? ()", "??", NULL},
        {true, "fsync ()", "fsync", NULL},
        /* newlines */
        {true,
         "write_to_temp_file (\n"
            "filename=0x18971b0 \"/home/jfclere/.recently-used.xbel\", \n"
            "contents=<value optimized out>, length=29917, error=0x7fff3cbe4110)",
         "write_to_temp_file",
         NULL},
        /* C++ */
        {true,
         "osgText::Font::GlyphTexture::apply(osg::State&) const ()",
         "osgText::Font::GlyphTexture::apply(osg::State&) const",
         NULL},
        {true,
         "osgUtil::RenderStage::drawInner(osg::RenderInfo&, osgUtil::RenderLeaf*&, bool&) ()",
         "osgUtil::RenderStage::drawInner(osg::RenderInfo&, osgUtil::RenderLeaf*&, bool&)",
         NULL},
        {true,
         "nsRegion::RgnRect::operator new ()",
         "nsRegion::RgnRect::operator new",
         NULL},
        {true,
         "sigc::internal::slot_call0<sigc::bound_mem_functor0<void, Driver>, void>::call_it (this=0x6c)",
         "sigc::internal::slot_call0<sigc::bound_mem_functor0<void, Driver>, void>::call_it",
         NULL},
        {true,
         "sigc::internal::slot_call0<sigc::bound_mem_functor0<void, GameWindow>, void>::call_it(sigc::internal::slot_rep*) ()",
         "sigc::internal::slot_call0<sigc::bound_mem_functor0<void, GameWindow>, void>::call_it(sigc::internal::slot_rep*)",
         NULL},
        /* C++ operator< and templates */
        {true,
         "operator< <char, std::char_traits<char>, std::allocator<char> > (__s1=<value optimized out>)",
         "operator< <char, std::char_traits<char>, std::allocator<char> >",
         NULL},
        /* C++ plain operator-> */
        {true, "operator-> ()", "operator->", NULL},
        /* Not an operator, but includes the keyword 'operator' (Red Hat Bugzilla bug #542445) */
        {true,
         "cairo_set_operator (cr=0x0, op=CAIRO_OPERATOR_OVER)",
         "cairo_set_operator",
         NULL},
        /* type included */
        {true, FUNCALL, FUNCTION, TYPE},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        char *function_name, *function_type;
        struct sr_location location;
        sr_location_init(&location);
        g_assert_true(t[i].success == sr_gdb_frame_parse_function_call((const char **)&t[i].input,
                                                                  &function_name,
                                                                  &function_type,
                                                                  &location));
        if (t[i].success)
        {
            printf("Expected: '%s', got '%s'\n", t[i].expected_function_name, function_name);
            g_assert_cmpstr(t[i].expected_function_name, ==, function_name);
            g_assert_true((!t[i].expected_function_type && !function_type) ||
                          0 == g_strcmp0(t[i].expected_function_type, function_type));
            g_assert_cmpuint(*t[i].input, ==, '\0');
            free(function_name);
            free(function_type);
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
        }
    }
}

static void
test_gdb_frame_parse_address_in_function(void)
{
    struct test_data
    {
        bool success;
        char *input;
        uint64_t expected_address;
        char *expected_function;
    };

    struct test_data t[] = {
        /* minimal */
        {true, "raise (sig=6)", -1, "raise"},
        /* still simple */
        {true, "0x00ad0a91 in raise (sig=6)", 0xad0a91, "raise"},
        /* longnum */
        {true, "0xf00000322221730e in IA__g_bookmark_file_to_file (\n"
             "filename=0x18971b0 \"/home/jfclere/.recently-used.xbel\", \n"
             "error=0x7fff3cbe4160)",
         0xf00000322221730eULL,
         "IA__g_bookmark_file_to_file"},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        char *function;
        char *type;
        uint64_t address;
        struct sr_location location;
        sr_location_init(&location);
        g_assert_true(t[i].success == sr_gdb_frame_parse_address_in_function((const char **)&t[i].input,
                                                                             &address,
                                                                             &function,
                                                                             &type,
                                                                             &location));
        if (t[i].success)
        {
            g_assert_cmpstr(function, ==, t[i].expected_function);
            g_assert_cmpuint(address, ==, t[i].expected_address);
            g_assert_cmpuint(*t[i].input, ==, '\0');
            free(function);
            free(type);
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
        }
    }
}

static void
test_gdb_frame_parse_file_location(void)
{
    struct test_data
    {
        bool success;
        char *input;
        char *expected_file;
        unsigned expected_line;
    };

    struct test_data t[] = {
        /* Test with a newline and without a line number. */
        {true, "\n at gtkrecentmanager.c", "gtkrecentmanager.c", -1},

        /* Test with a newline and with a line number.  */
        {true, "\n at gtkrecentmanager.c:1377", "gtkrecentmanager.c", 1377},

        /* Test without a newline and a file name with a dash and an upper letter. */
        {true,
         " at ../sysdeps/unix/syscall-template.S:82",
         "../sysdeps/unix/syscall-template.S",
         82},

        /* A file name starting with an underscore: Red Hat Bugzilla bug #530678. */
        {true,
         " at _polkitauthenticationagent.c:885",
         "_polkitauthenticationagent.c",
         885},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        char *file;
        unsigned line;
        struct sr_location location;
        sr_location_init(&location);
        g_assert_true(t[i].success == sr_gdb_frame_parse_file_location((const char **)&t[i].input,
                                                                       &file,
                                                                       &line,
                                                                       &location));
        if (t[i].success)
        {
            g_assert_cmpstr(file, ==, t[i].expected_file);
            g_assert_cmpuint(line, ==, t[i].expected_line);
            g_assert_cmpuint(*t[i].input, ==, '\0');
            free(file);
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
        }
    }
}

static void
test_gdb_frame_parse_header(void)
{
    struct test_data
    {
        char *input;
        struct sr_gdb_frame *expected_frame;
    };

    /* no function address - rhbz#752811 */
    struct sr_gdb_frame frame1;
    sr_gdb_frame_init(&frame1);
    frame1.function_name = "slot_tp_getattr_hook";
    frame1.number = 0;
    frame1.source_file = "/usr/src/debug/Python-2.7.1/Objects/typeobject.c";
    frame1.source_line = 5436;
    frame1.address = -1;

    /* basic */
    struct sr_gdb_frame frame2;
    sr_gdb_frame_init(&frame2);
    frame2.function_name = "fsync";
    frame2.number = 1;
    frame2.source_file = "../sysdeps/unix/syscall-template.S";
    frame2.source_line = 82;
    frame2.address = 0x322160e7fdULL;

    /* C++ */
    struct sr_gdb_frame frame3;
    sr_gdb_frame_init(&frame3);
    frame3.function_name = "nsRegion::RgnRect::operator new";
    frame3.number = 4;
    frame3.source_file = "nsRegion.cpp";
    frame3.source_line = 214;
    frame3.address = 0x3f96d71056ULL;

    /* Templates and no filename. */
    struct sr_gdb_frame frame4;
    sr_gdb_frame_init(&frame4);
    frame4.function_name = "sigc::internal::slot_call0<sigc::bound_mem_functor0<void, GameWindow>, void>::call_it(sigc::internal::slot_rep*)";
    frame4.number = 15;
    frame4.address = 0x08201bdfULL;

    /* No address, just the function call. Red Hat Bugzilla bug #530678 */
    struct sr_gdb_frame frame5;
    sr_gdb_frame_init(&frame5);
    frame5.function_name = "handle_message";
    frame5.number = 30;
    frame5.source_file = "_polkitauthenticationagent.c";
    frame5.source_line = 885;

    struct test_data t[] = {
        {"#0  slot_tp_getattr_hook (self=<YumAvailableP...>)"
            " at /usr/src/debug/Python-2.7.1/Objects/typeobject.c:5436", &frame1},
        {"#1  0x000000322160e7fd in fsync () at ../sysdeps/unix/syscall-template.S:82", &frame2},
        {"#4  0x0000003f96d71056 in nsRegion::RgnRect::operator new ()\n"
            "    at nsRegion.cpp:214", &frame3},
        {"#15 0x08201bdf in sigc::internal::slot_call0<sigc::bound_mem_functor0<void, GameWindow>,"
            " void>::call_it(sigc::internal::slot_rep*) ()", &frame4},
        {"#30 handle_message (message=<value optimized out>,\n"
            "interface=<value optimized out>) at _polkitauthenticationagent.c:885", &frame5},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        printf("=================================================\n"
               "Testing %s\n",
               t[i].input);

        char *old_input = t[i].input;
        struct sr_location location;
        sr_location_init(&location);
        struct sr_gdb_frame *frame = sr_gdb_frame_parse_header((const char **)&t[i].input, &location);
        if (frame)
        {
            g_assert_cmpuint(*t[i].input, ==, '\0');
            g_assert_true(sr_gdb_frame_cmp(frame, t[i].expected_frame, true) == 0);
            sr_gdb_frame_free(frame);
        }
        else
        {
            printf(" - parsing failed: %d:%d %s\n", location.line, location.column, location.message);

            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
            g_assert_null(t[i].expected_frame);
        }
    }
}

static void
test_gdb_frame_parse(void)
{
    struct test_data
    {
        char *input;
        struct sr_gdb_frame *expected_frame;
        char *expected_input;
        struct sr_location *expected_location;
    };

    struct sr_gdb_frame frame;
    sr_gdb_frame_init(&frame);
    frame.function_name = "fsync";
    frame.number = 1;
    frame.source_file = "../sysdeps/unix/syscall-template.S";
    frame.source_line = 82;
    frame.address = 0x322160e7fdULL;

    struct sr_location location1;
    sr_location_init(&location1);
    location1.line = 2;
    location1.column = 10;

    struct sr_location location2;
    sr_location_init(&location2);
    location2.line = 3;
    location2.column = 0;

    char *c1 = "#1  0x000000322160e7fd in fsync () at ../sysdeps/unix/syscall-template.S:82\n"
               "No locals.";

    char *c2 = "#1  0x000000322160e7fd in fsync () at ../sysdeps/unix/syscall-template.S:82\n"
               "No locals.\n"
               "#2  0x003f4f3f in IA__g_main_loop_run (loop=0x90e2c50) at gmain.c:2799\n"
               "        self = 0x8b80038\n"
               "  __PRETTY_FUNCTION__ = \"IA__g_main_loop_run\"\n";

    struct test_data t[] = {
        {c1, &frame, c1 + strlen(c1), &location1},
        {c2, &frame, strstr(c2, "#2"), &location2},
    };

    for (int i = 0; i < sizeof (t) / sizeof (*t); i++)
    {
        char *old_input = t[i].input;
        struct sr_location location;
        sr_location_init(&location);
        struct sr_gdb_frame *frame = sr_gdb_frame_parse((const char **)&t[i].input, &location);
        g_assert_true(t[i].input == t[i].expected_input);
        if (frame)
        {
            g_assert_cmpint(sr_gdb_frame_cmp(frame, t[i].expected_frame, true), ==, 0);
            g_autofree char *loc_str = sr_location_to_string(&location);
            puts(loc_str);
            g_autofree char *exp_loc_str = sr_location_to_string(t[i].expected_location);
            puts(exp_loc_str);
            g_assert_cmpint(sr_location_cmp(&location, t[i].expected_location, true), ==, 0);
            sr_gdb_frame_free(frame);
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(old_input == t[i].input);
            g_assert_cmpint(sr_location_cmp(&location, t[i].expected_location, true), ==, 0);
            g_assert_null(t[i].expected_frame);
        }
    }
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/gdb/dup", test_gdb_frame_dup);
    g_test_add_func("/frame/gdb/parse-frame-start", test_gdb_frame_parse_frame_start);
    g_test_add_func("/frame/gdb/parseadd-operator", test_gdb_frame_parseadd_operator);
    g_test_add_func("/frame/gdb/parse-function-name", test_gdb_frame_parse_function_name);
    g_test_add_func("/frame/gdb/parse-function-name-template-args", test_gdb_frame_parse_function_name_template_args);
    g_test_add_func("/frame/gdb/skip-function-args", test_gdb_frame_skip_function_args);
    g_test_add_func("/frame/gdb/parse-function-call", test_gdb_frame_parse_function_call);
    g_test_add_func("/frame/gdb/parse-address-in-function", test_gdb_frame_parse_address_in_function);
    g_test_add_func("/frame/gdb/parse-file-location", test_gdb_frame_parse_file_location);
    g_test_add_func("/frame/gdb/parse-header", test_gdb_frame_parse_header);
    g_test_add_func("/frame/gdb/parse", test_gdb_frame_parse);

    return g_test_run();
}
