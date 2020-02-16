#include <glib.h>
#include <koops/frame.h>
#include <stdbool.h>

static void
test_koops_skip_timestamp(void)
{
    struct
    {
        bool result;
        const char *input;
    } test_data[] =
    {
        { true, "[  120.836230]" },
        { true, "[110417.280595]" },
        { false, "--2012-06-28 " },
        { false, "[<ffffffff81479782>]" },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        const char *input;
        bool success;

        input = test_data[i].input;
        success = sr_koops_skip_timestamp(&input);

        g_assert_true(success == test_data[i].result);

        if (success)
        {
            g_assert_cmpint(*input, ==, '\0');
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(input == test_data[i].input);
        }
    }
}

static void
test_koops_parse_address(void)
{
    struct
    {
        bool result;
        const char *input;
        uint64_t address;
    } test_data[] =
    {
        { true, "[<ffffffff81479782>]", 0xffffffff81479782, },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        const char *input;
        uint64_t address;
        bool success;

        input = test_data[i].input;
        success = sr_koops_parse_address(&input, &address);

        g_assert_true(success == test_data[i].result);

        if (success)
        {
            g_assert_cmpint(*input, ==, '\0');
            g_assert_cmpuint(address, ==, test_data[i].address);
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(input == test_data[i].input);
        }
    }
}

static void
test_koops_parse_module_name(void)
{
    struct
    {
        bool result;
        const char *input;
        const char *module_name;
    } test_data[] =
    {
        { true, "[ppp_generic]", "ppp_generic", },
        { false, "ppp_generic", NULL, },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        const char *input;
        g_autofree char *module_name = NULL;
        bool success;

        input = test_data[i].input;
        success = sr_koops_parse_module_name(&input, &module_name);

        g_assert_true(test_data[i].result == success);

        if (success)
        {
            g_assert_cmpstr(module_name, ==, test_data[i].module_name);
            g_assert_cmpint(*input, ==, '\0');
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(input == test_data[i].input);
        }
    }
}

static void
test_koops_parse_function(void)
{
    struct
    {
        bool result;
        const char *input;
        const char *function_name;
        uint64_t offset;
        uint64_t length;
        const char *module_name;
    } test_data[] =
    {
        {
            true,
            "warn_slowpath_common+0x7f/0xc0",
            "warn_slowpath_common", 0x7f, 0xc0, NULL,
        },
        {
            true,
            "(_raw_spin_lock_irqsave+0x48/0x5c)",
            "_raw_spin_lock_irqsave", 0x48, 0x5c, NULL,
        },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        const char *input;
        g_autofree char *function_name = NULL;
        g_autofree char *module_name = NULL;
        uint64_t offset;
        uint64_t length;
        bool success;

        input = test_data[i].input;
        success = sr_koops_parse_function(&input, &function_name,
                                          &offset, &length, &module_name);

        g_assert_true(success == test_data[i].result);

        if (success)
        {
            g_assert_cmpstr(function_name, ==, test_data[i].function_name);
            g_assert_cmpuint(offset, ==, test_data[i].offset);
            g_assert_cmpuint(length, ==, test_data[i].length);
            g_assert_cmpstr(module_name, ==, test_data[i].module_name);

            g_assert_cmpint(*input, ==, '\0');
        }
        else
        {
            /* Check that the pointer is not moved. */
            g_assert_true(input == test_data[i].input);
        }
    }
}

static void
test_koops_frame_parse(void)
{
    struct
    {
        bool result;
        const char *input;
        struct sr_koops_frame frame;
    } test_data[] =
    {
        {
            true,
            "[<ffffffff8151f860>] ? ip_forward_options+0x1f0/0x1f0",
            {
                .type = SR_REPORT_KERNELOOPS,
                .address = 0xffffffff8151f860,
                .reliable = false,
                .function_name = "ip_forward_options",
                .function_offset = 0x1f0,
                .function_length = 0x1f0,
            },
        },
        {
            true,
            "[<c02f671c>] (_raw_spin_lock_irqsave+0x48/0x5c) from [<bf054120>] (mxs_mmc_enable_sdio_irq+0x18/0xdc [mxs_mmc])",
            {
                .type = SR_REPORT_KERNELOOPS,
                .address = 0xc02f671c,
                .reliable = true,
                .function_name = "_raw_spin_lock_irqsave",
                .function_offset = 0x48,
                .function_length = 0x5c,
                .from_address = 0xbf054120,
                .from_function_name = "mxs_mmc_enable_sdio_irq",
                .from_function_offset = 0x18,
                .from_function_length = 0xdc,
                .from_module_name = "mxs_mmc",
            },
        },
        {
            true,
            "sys_delete_module+0x11f/0x280",
            {
                .type = SR_REPORT_KERNELOOPS,
                .reliable = true,
                .function_name = "sys_delete_module",
                .function_offset = 0x11f,
                .function_length = 0x280,
            },
        },
        {
            true,
            "[<c0014318>] (dump_backtrace_log_lvl) from [<c001457c>] (show_stack+0x24/0x2c)",
            {
                .address = 0xc0014318,
                .reliable = true,
                .function_name = "dump_backtrace_log_lvl",
                .from_address = 0xc001457c,
                .from_function_name = "show_stack",
                .from_function_offset = 0x24,
                .from_function_length = 0x2c,
            },
        },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(test_data); i++)
    {
        const char *input;
        struct sr_koops_frame *frame;

        input = test_data[i].input;
        frame = sr_koops_frame_parse(&input);

        if (test_data[i].result)
        {
            g_assert_nonnull(frame);
            g_assert_cmpint(sr_koops_frame_cmp(frame, &test_data[i].frame), ==, 0);
            g_assert_cmpint(*input, ==, '\0');

            sr_koops_frame_free(frame);
        }
        else
        {
            g_assert_null(frame);
            /* Check that the pointer is not moved. */
            g_assert_true(input == test_data[i].input);
        }
    }
}

static void
test_koops_to_json(void)
{
    struct sr_koops_frame frame;
    g_autofree char *json = NULL;
    const char *test_json = "{   \"address\": 3224332060\n"
                            ",   \"reliable\": true\n"
                            ",   \"function_name\": \"_raw_spin_lock_irqsave\"\n"
                            ",   \"function_offset\": 72\n"
                            ",   \"function_length\": 92\n"
                            ",   \"from_address\": 3204792608\n"
                            ",   \"from_function_name\": \"mxs_mmc_enable_sdio_irq\"\n"
                            ",   \"from_function_offset\": 24\n"
                            ",   \"from_function_length\": 220\n"
                            ",   \"from_module_name\": \"mxs_mmc\"\n"
                            "}";

    sr_koops_frame_init(&frame);

    frame.address = 0xc02f671c;
    frame.reliable = true;
    frame.function_name = "_raw_spin_lock_irqsave";
    frame.function_offset = 0x48;
    frame.function_length = 0x5c;
    frame.from_address = 0xbf054120;
    frame.from_function_name = "mxs_mmc_enable_sdio_irq";
    frame.from_function_offset = 0x18;
    frame.from_function_length = 0xdc;
    frame.from_module_name = "mxs_mmc";

    json = sr_koops_frame_to_json(&frame);

    g_assert_cmpstr(json, ==, test_json);

}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/frame/koops/skip-timestamp", test_koops_skip_timestamp);
    g_test_add_func("/frame/koops/parse-address", test_koops_parse_address);
    g_test_add_func("/frame/koops/parse-module-name", test_koops_parse_module_name);
    g_test_add_func("/frame/koops/parse-function", test_koops_parse_function);
    g_test_add_func("/frame/koops/parse", test_koops_frame_parse);
    g_test_add_func("/frame/koops/to-json", test_koops_to_json);

    return g_test_run();
}
