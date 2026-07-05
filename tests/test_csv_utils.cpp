#include "framework.h"
#include "timetable.h"

using utils::parseUint16;
using utils::splitCsvLine;
using utils::splitLines;

void test_csv_utils()
{
    SECTION("splitCsvLine");
    {
        auto f = splitCsvLine("a,b,c");
        CHECK((int)f.size() == 3);
        CHECK(f[0] == "a" && f[1] == "b" && f[2] == "c");

        // empty fields: leading, middle, trailing
        auto e = splitCsvLine("a,,c,");
        CHECK((int)e.size() == 4);
        CHECK(e[0] == "a" && e[1].empty() && e[2] == "c" && e[3].empty());

        // empty input => single empty field
        auto one = splitCsvLine("");
        CHECK((int)one.size() == 1 && one[0].empty());

        // quoted field containing the delimiter
        auto q = splitCsvLine("a,\"b,c\",d");
        CHECK((int)q.size() == 3);
        CHECK(q[1] == "b,c");

        // "" escape => literal quote inside a quoted field
        auto esc = splitCsvLine("\"a\"\"b\",c");
        CHECK((int)esc.size() == 2);
        CHECK(esc[0] == "a\"b");

        // custom delimiter (used for ;-separated sub-lists)
        auto semi = splitCsvLine("70;71;74", ';');
        CHECK((int)semi.size() == 3);
        CHECK(semi[2] == "74");
    }

    SECTION("splitLines");
    {
        // LF, CRLF and a trailing line without a newline
        auto a = splitLines("a\r\nb\nc");
        CHECK((int)a.size() == 3);
        CHECK(a[0] == "a" && a[1] == "b" && a[2] == "c");

        // trailing newline does not produce a trailing empty line
        auto b = splitLines("a\nb\n");
        CHECK((int)b.size() == 2);

        // intermediate blank lines ARE preserved (record parsers skip them)
        auto c = splitLines("a\n\nb");
        CHECK((int)c.size() == 3);
        CHECK(c[1].empty());

        // empty content => no lines
        CHECK((int)splitLines("").size() == 0);
    }

    SECTION("parseUint16");
    {
        CHECK(parseUint16("0") == 0);
        CHECK(parseUint16("05") == 5);       // leading zero, decimal (not octal)
        CHECK(parseUint16("65535") == 65535);
        CHECK(!parseUint16("65536"));        // overflow
        CHECK(!parseUint16("70000"));        // overflow
        CHECK(!parseUint16(""));             // empty
        CHECK(!parseUint16("12x"));          // trailing junk
        CHECK(!parseUint16("x12"));          // leading junk
        CHECK(!parseUint16(" 12"));          // leading space not consumed
        CHECK(!parseUint16("-5"));           // no sign for unsigned
        CHECK(!parseUint16("+5"));
    }
}
