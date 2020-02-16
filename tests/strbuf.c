#include <strbuf.h>

#include <glib.h>

void
test_strbuf_append_char(void)
{
    struct sr_strbuf *strbuf;

    strbuf = sr_strbuf_new();

    for (int i = 0; i < 100; ++i)
    {
        g_assert_cmpuint(strbuf->len, ==, i);
        g_assert_cmpuint(strbuf->alloc, >, strbuf->len);
        g_assert_cmpint(strbuf->buf[i], ==, '\0');

        sr_strbuf_append_char(strbuf, 'a');

        g_assert_cmpint(strbuf->buf[i], ==, 'a');
        g_assert_cmpint(strbuf->buf[i + 1], ==, '\0');

        g_assert_cmpuint(strbuf->len, ==, i + 1);
        g_assert_cmpuint(strbuf->alloc, >, strbuf->len);
    }

    sr_strbuf_free(strbuf);
}

void
test_strbuf_append_str(void)
{
    for (int i = 0; i < 50; i++)
    {
        struct sr_strbuf *strbuf;
        char str[50] = { 0 };

        strbuf = sr_strbuf_new();
        for (int j = 0; j < i; j++)
        {
            str[j] = 'a';
        }

        for (int j = 0; j < 100; j++)
        {
            g_assert_cmpuint(strbuf->len, ==, j * i);
            g_assert_cmpuint(strbuf->alloc, >, strbuf->len);
            g_assert_cmpint(strbuf->buf[j * i], ==, '\0');

            sr_strbuf_append_str(strbuf, str);

            g_assert_cmpint(strbuf->buf[j * i], ==, str[0]);
            g_assert_cmpint(strbuf->buf[j * i + i], ==, '\0');
            g_assert_cmpuint(strbuf->len, ==, j * i + i);
            g_assert_cmpuint(strbuf->alloc, >, strbuf->len);
        }

        sr_strbuf_free(strbuf);
    }
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/strbuf/append-char", test_strbuf_append_char);
    g_test_add_func("/strbuf/append-str", test_strbuf_append_str);

    return g_test_run();
}
