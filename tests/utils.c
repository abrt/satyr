#include "utils.h"
#include <stdio.h>
#include <glib.h>

static void
test_strchr_location(void)
{
    /* The character is on the first line.*/
    int line, column;
    char *result = sr_strchr_location("test string", 'r', &line, &column);
    g_assert_cmpstr(result, ==, "ring");
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(7, ==, column);
    
    /* The character is on the third line. */
    result = sr_strchr_location("\ntest\n string", 'r', &line, &column);
    g_assert_cmpstr(result, ==, "ring");
    g_assert_cmpint(3, ==, line);
    g_assert_cmpint(3, ==, column);
    
    /* The character is not found. */
    result = sr_strchr_location("\ntest\n string", 'z', &line, &column);
    g_assert_false(result);
    
    /* The character _is_ a newline. */
    result = sr_strchr_location("abcd\nefgh", '\n', &line, &column);
    g_assert_cmpstr(result, ==, "\nefgh");
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(4, ==, column);
}

static void
test_strstr_location(void)
{
    /* The substring is on the first line.*/
    int line, column;
    char *result = sr_strstr_location("test string", "ri", &line, &column);
    g_assert_cmpstr(result, ==, "ring");
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(7, ==, column);
    
    /* The substring is on the third line. */
    result = sr_strstr_location("\ntest\n string", "ri", &line, &column);
    g_assert_cmpstr(result, ==, "ring");
    g_assert_cmpint(3, ==, line);
    g_assert_cmpint(3, ==, column);
    
    /* The substring is not found, but the first char is. */
    result = sr_strstr_location("\ntest\n string", "rz", &line, &column);
    g_assert_false(result);
    
    /* The substring is not found. */
    result = sr_strstr_location("\ntest\n string", "zr", &line, &column);
    g_assert_false(result);
    
    /* The substring _is_ a newline. */
    result = sr_strstr_location("abcd\nefgh", "\n", &line, &column);
    g_assert_cmpstr(result, ==, "\nefgh");
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(4, ==, column);
}

static void
test_strspn_location(void)
{
    /* No newline in the input.*/
    int line, column;
    size_t count = sr_strspn_location("test string",
                                       "tes ",
                                       &line,
                                       &column);
    g_assert_cmpuint(7, ==, count);
    g_assert_cmpuint(1, ==, line);
    g_assert_cmpuint(7, ==, column);
    
    /* With some newlines. */
    count = sr_strspn_location("te\nst \nstring",
                                "tes \n",
                                &line,
                                &column);
    g_assert_cmpuint(9, ==, count);
    g_assert_cmpuint(3, ==, line);
    g_assert_cmpuint(2, ==, column);
}

static void
test_skip_char(void)
{
    char *input = "abc";
    g_assert_true(sr_skip_char((const char **)&input, 'a'));
    g_assert_false(sr_skip_char((const char **)&input, 'c'));
}

static void
test_skip_char_limited(void)
{
    char *input = "abc";
    g_assert_true(sr_skip_char_limited((const char **)&input, "ab"));
    g_assert_true(sr_skip_char_limited((const char **)&input, "ab"));
    g_assert_false(sr_skip_char_limited((const char **)&input, "ab"));
}

static void
test_parse_char_limited(void)
{
    char *input = "abc";
    char result;
    
    /* First char in allowed is used. */
    g_assert_true(sr_parse_char_limited((const char **)&input, "ab", &result));
    g_assert_true(*input == 'b' && result == 'a');
    
    /* No char in allowed is used. */
    g_assert_false(sr_parse_char_limited((const char **)&input, "cd", &result));
    g_assert_true(*input == 'b' && result == 'a');
    
    /* Second char in allowed is used. */
    g_assert_true(sr_parse_char_limited((const char **)&input, "ab", &result));
    g_assert_true(*input == 'c' && result == 'b');
    
    /* Empty set of allowed chars. */
    g_assert_false(sr_parse_char_limited((const char **)&input, "", &result));
    g_assert_true(*input == 'c' && result == 'b');
    
    /* Char is multiple times in allowed. */
    g_assert_true(sr_parse_char_limited((const char **)&input, "cdc", &result));
    g_assert_true(*input == '\0' && result == 'c');
}

static void
test_skip_char_sequence(void)
{
    char *input = "aaaaabc";
    g_assert_cmpint(5, ==, sr_skip_char_sequence((const char **)&input, 'a'));
    g_assert_cmpint(1, ==, sr_skip_char_sequence((const char **)&input, 'b'));
    g_assert_cmpint(0, ==, sr_skip_char_sequence((const char **)&input, 'b'));
}

static void
test_skip_char_span(void)
{
    char *input = "aabaabc";
    g_assert_cmpint(6, ==, sr_skip_char_span((const char **)&input, "ba"));
    g_assert_cmpint(0, ==, sr_skip_char_span((const char **)&input, "b"));
    g_assert_cmpint(1, ==, sr_skip_char_span((const char **)&input, "bc"));

    /* Test that it can parse empty string. */
    g_assert_cmpint(0, ==, sr_skip_char_span((const char **)&input, "abc"));
}

static void
test_skip_char_span_location(void)
{
    char *input = "aab\naabc";
    int line, column;
    
    g_assert_cmpint(7, ==, sr_skip_char_span_location((const char **)&input, "ba\n", &line, &column));
    g_assert_cmpint(2, ==, line);
    g_assert_cmpint(3, ==, column);
    
    g_assert_cmpint(0, ==, sr_skip_char_span_location((const char **)&input, "b", &line, &column));
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(0, ==, column);
    
    g_assert_cmpint(1, ==, sr_skip_char_span_location((const char **)&input, "bc", &line, &column));
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(1, ==, column);
    
    /* Test that it can parse empty string. */
    g_assert_cmpint(0, ==, sr_skip_char_span_location((const char **)&input, "abc", &line, &column));
    g_assert_cmpint(1, ==, line);
    g_assert_cmpint(0, ==, column);
}

static void
test_parse_char_span(void)
{
    char *input = "abcd";
    char *result;
    g_assert_cmpint(2, ==, sr_parse_char_span((const char **)&input, "ba", &result));
    g_assert_true(*input == 'c' && strcmp(result, "ab") == 0);
    g_assert_cmpint(0, ==, sr_parse_char_span((const char **)&input, "ba", &result));
    g_assert_true(*input == 'c' && strcmp(result, "ab") == 0);
    free(result);
    g_assert_cmpint(2, ==, sr_parse_char_span((const char **)&input, "ecfd", &result));
    g_assert_true(*input == '\0' && strcmp(result, "cd") == 0);
    free(result);
}

static void
test_parse_char_cspan(void)
{
    char *input = "abcd";
    char *result;
    g_assert_true(sr_parse_char_cspan((const char **)&input, "c", &result));
    g_assert_true(*input == 'c' && strcmp(result, "ab") == 0);
    g_assert_false(sr_parse_char_cspan((const char **)&input, "c", &result));
    g_assert_true(*input == 'c' && strcmp(result, "ab") == 0);
    free(result);
    g_assert_true(sr_parse_char_cspan((const char **)&input, "e", &result));
    g_assert_true(*input == '\0' && strcmp(result, "cd") == 0);
    free(result);
}

static void
test_skip_string(void)
{
    char *input = "abcd";
    g_assert_cmpint(2, ==, sr_skip_string((const char **)&input, "ab"));
    g_assert_cmpint(*input, ==, 'c');
    g_assert_cmpint(0, ==, sr_skip_string((const char **)&input, "cde"));
    g_assert_cmpint(2, ==, sr_skip_string((const char **)&input, "cd"));
    g_assert_cmpint(0, ==, sr_skip_string((const char **)&input, "cd"));
}

static void
test_parse_string(void)
{
    char *input = "abcd";
    char *result;
    g_assert_true(sr_parse_string((const char **)&input, "ab", &result));
    g_assert_cmpstr(result, ==, "ab");
    g_assert_cmpint(*input, ==, 'c');
    free(result);
}

static void
test_skip_uint(void)
{
    char *input = "10";
    g_assert_cmpint(2, ==, sr_skip_uint((const char **)&input));
    g_assert_cmpint(*input, ==, '\0');
}

static void
test_parse_uint32(void)
{
    char *input = "10";
    uint32_t result;
    g_assert_cmpint(2, ==, sr_parse_uint32((const char **)&input, &result));
    g_assert_cmpint('\0', ==, *input);
    g_assert_cmpuint(10, ==, result);
}

static void
test_skip_hexadecimal_0xuint(void)
{
    char *input = "0xffddffddff";
    g_assert_cmpint(12, ==, sr_skip_hexadecimal_0xuint((const char **)&input));
    g_assert_cmpint(*input, ==, '\0');
}

static void
test_parse_hexadecimal_0xuint64(void)
{
    char *input = "0x0fafaffff 0x2badf00dbaadf00d";
    uint64_t num;
    g_assert_cmpint(11, ==, sr_parse_hexadecimal_0xuint64((const char **)&input, &num));
    g_assert_cmpint(*input, ==, ' ');
    g_assert_cmpuint(num, ==, 0xfafaffff);
    *input++;
    g_assert_cmpint(18, ==, sr_parse_hexadecimal_0xuint64((const char **)&input, &num));
    g_assert_cmpint(*input, ==, '\0');
    g_assert_cmpuint(num, ==, 0x2badf00dbaadf00d);
}

static void
test_indent(void)
{
    g_assert_cmpstr(sr_indent("text", 2), ==, "  text");
    g_assert_cmpstr(sr_indent("text\n", 2), ==, "  text\n");
    g_assert_cmpstr(sr_indent("text\n\n", 2), ==, "  text\n  \n");
}

void do_check(char **in, size_t inlen, char **out, size_t outlen)
{
    printf("%d %d\n", inlen, outlen);
    sr_struniq(in, &inlen);
    g_assert_true(inlen == outlen);
    
    int i;
    for (i = 0; i < outlen; i++)
    {
        g_assert_cmpstr(in[i], ==, out[i]);
    }
}

#define check(x,y) do_check(x, sizeof(x)/sizeof(x[0]), y, sizeof(y)/sizeof(y[0]))

static void
test_struniq(void)
{
    size_t size = 0;
    sr_struniq(NULL, &size);
    g_assert_cmpuint(size, ==, 0);
    
    char *a1[] = {"a"}, *b1[] = {"a"};
    check(a1, b1);
    
    char *a2[] = {"a", "a"}, *b2[] = {"a"};
    check(a2, b2);
    
    char *a3[] = {"a", "b"}, *b3[] = {"a", "b"};
    check(a3, b3);
}

static void
test_demangle_symbol(void)
{
    sr_debug_parser = true;

    char *result = sr_demangle_symbol("_ZN3Job8WaitDoneEv");
    g_assert_cmpstr(result, ==, "Job::WaitDone()");

    result = sr_demangle_symbol("not_mangled");
    g_assert_null(result);

    result = sr_demangle_symbol("f");
    g_assert_null(result);
}

int
main(int    argc,
     char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/utils/strchr_location", test_strchr_location);
    g_test_add_func("/utils/strstr_location", test_strstr_location);
    g_test_add_func("/utils/strspn_location", test_strspn_location);
    g_test_add_func("/utils/skip_char", test_skip_char);
    g_test_add_func("/utils/skip_char_limited", test_skip_char_limited);
    g_test_add_func("/utils/parse_char_limited", test_parse_char_limited);
    g_test_add_func("/utils/skip_char_sequence", test_skip_char_sequence);
    g_test_add_func("/utils/skip_char_span", test_skip_char_span);
    g_test_add_func("/utils/skip_char_span_location", test_skip_char_span_location);
    g_test_add_func("/utils/parse_char_span", test_parse_char_span);
    g_test_add_func("/utils/parse_char_cspan", test_parse_char_cspan);
    g_test_add_func("/utils/skip_string", test_skip_string);
    g_test_add_func("/utils/parse_string", test_parse_string);
    g_test_add_func("/utils/skip_uint", test_skip_uint);
    g_test_add_func("/utils/parse_uint32", test_parse_uint32);
    g_test_add_func("/utils/skip_hexadecimal_0xuint", test_skip_hexadecimal_0xuint);
    g_test_add_func("/utils/parse_hexadecimal_0xuint64", test_parse_hexadecimal_0xuint64);
    g_test_add_func("/utils/indent", test_indent);
    g_test_add_func("/utils/struniq", test_struniq);
    g_test_add_func("/utils/demangle_symbol", test_demangle_symbol);

    return g_test_run();
}
