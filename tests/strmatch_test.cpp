/**
 * strmatch.hpp — Unit Tests
 *
 * 665 findings in ase-codegen point at this header, and 340 patterns will be rewritten
 * against it. A matcher without a case per feature would be the most dangerous file in the
 * tree, so every feature the measurement found is pinned here, and so is every feature that
 * was deliberately left out.
 *
 * THE STATIC_ASSERTS ARE THE POINT, not decoration. str_match_search is constexpr, so the
 * compiler evaluates these cases while translating — a broken matcher fails the build
 * instead of failing a test run. The doctest cases below repeat the same ground at runtime
 * and add the ones that read more clearly as prose.
 *
 * The doctest main is switched on from tests/CMakeLists.txt rather than from a define here,
 * so this file carries no macro definition of its own.
 */

#include <doctest/doctest.h>

#include <ase/utils/strmatch.hpp>
#include <ase/utils/strops.hpp>  // str_split_next — the split needs no matcher, see below

using namespace ase::utils;

namespace {

constexpr uint32_t GROUPS = StrMatchMaxGroups;

/** Length of a literal, so the cases below read without a magic number beside each string. */
constexpr uint32_t len_of(const char* s) {
    uint32_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

/** True when `pattern` matches anywhere in `text`. */
constexpr bool hits(const char* pattern, const char* text) {
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    return str_match_search(pattern, text, len_of(text), gb, ge, GROUPS);
}

/** Start offset of the whole match, or StrMatchNoGroup when there is none. */
constexpr int32_t hit_begin(const char* pattern, const char* text) {
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    if (!str_match_search(pattern, text, len_of(text), gb, ge, GROUPS)) return StrMatchNoGroup;
    return gb[0];
}

/** End offset of the whole match, one past the last character. */
constexpr int32_t hit_end(const char* pattern, const char* text) {
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    if (!str_match_search(pattern, text, len_of(text), gb, ge, GROUPS)) return StrMatchNoGroup;
    return ge[0];
}

/** Length of capture group `n`, or StrMatchNoGroup when it did not participate. */
constexpr int32_t group_len(const char* pattern, const char* text, uint32_t n) {
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    if (!str_match_search(pattern, text, len_of(text), gb, ge, GROUPS)) return StrMatchNoGroup;
    if (gb[n] == StrMatchNoGroup) return StrMatchNoGroup;
    return ge[n] - gb[n];
}

/** Start offset of capture group `n`. */
constexpr int32_t group_begin(const char* pattern, const char* text, uint32_t n) {
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    if (!str_match_search(pattern, text, len_of(text), gb, ge, GROUPS)) return StrMatchNoGroup;
    return gb[n];
}

// ---------------------------------------------------------------------------------------
// LITERALS AND POSITION
// ---------------------------------------------------------------------------------------
static_assert(hits("abc", "abc"), "a literal matches itself");
static_assert(hits("abc", "xxabcyy"), "a literal is found inside a longer subject");
static_assert(!hits("abc", "abd"), "a literal that differs in one place does not match");
static_assert(hit_begin("abc", "xxabc") == 2, "the match starts where the literal starts");
static_assert(hit_end("abc", "xxabc") == 5, "the end is one past the last character");
static_assert(hits("", "anything"), "the empty pattern matches at position 0");

// ---------------------------------------------------------------------------------------
// QUANTIFIERS — greedy, and greedy means greedy
// ---------------------------------------------------------------------------------------
static_assert(hit_end("a*", "aaa") == 3, "star takes all three");
static_assert(hit_end("a*", "bbb") == 0, "star matches empty and stays put");
static_assert(hits("a+", "aaa"), "plus needs at least one and finds three");
static_assert(!hits("a+", "bbb"), "plus fails when there is none");
static_assert(hit_end("a+", "aaab") == 3, "plus stops where the run stops");
static_assert(hit_end("a?", "aaa") == 1, "question mark takes at most one");
static_assert(hit_end("a?", "bbb") == 0, "question mark is happy with none");
static_assert(hit_end("ab*c", "abbbc") == 5, "a quantifier in the middle backtracks correctly");
static_assert(hits("ab*c", "ac"), "the starred atom may be absent entirely");

// ---------------------------------------------------------------------------------------
// LAZY QUANTIFIERS — 18 patterns in the tree depend on this, and the difference is the point
// ---------------------------------------------------------------------------------------
static_assert(hit_end("a+?", "aaa") == 1, "lazy plus takes the fewest it can");
static_assert(hit_end("a*?b", "aaab") == 4, "lazy star still has to reach the b");
// The tree's own comment matcher: /\*[\s\S]*?\*/ must stop at the FIRST close.
static_assert(hit_end("/\\*[\\s\\S]*?\\*/", "/*one*/ /*two*/") == 7,
              "the comment matcher stops at the first close, not the last");
static_assert(hit_end("/\\*[\\s\\S]*\\*/", "/*one*/ /*two*/") == 15,
              "and the greedy form runs to the last one — this is the difference");

// ---------------------------------------------------------------------------------------
// THE GROUP BOUNDARY — the rest of a pattern does not reach into a group
//
// These three pin a LIMIT, not a feature, and they are written so that a later rewrite to
// continuation passing FAILS them instead of quietly changing behaviour. Whoever does that
// rewrite: line 2 flips to a hit with group length 17, and that is the signal to delete this
// block and the paragraph in strmatch.hpp that describes it.
//
// The third assert is the one that makes this a finding about GROUPS. Without it the second
// line reads as "lazy quantifiers are broken", which is false and would send the next reader
// into the wrong file.
// ---------------------------------------------------------------------------------------
static_assert(group_len("view<([^>]+)>", "registry.view<PositionComponent>", 1) == 17,
              "POSITIVE CONTROL: a group that ends itself is matched, and captured correctly");
static_assert(!hits("view<([\\s\\S]+?)>", "registry.view<PositionComponent>"),
              "a group that needs the FOLLOWING character as its stop signal does not match");
static_assert(hits("view<[\\s\\S]+?>", "registry.view<PositionComponent>"),
              "same quantifier without the parentheses matches — so it is the group, not lazy");
// The second failing shape. NOTE THE CORRECTION, it cost a wrong assert on 2026-08-20: give-back
// INSIDE a group works normally — \w* and the alternation after it live in the same inner
// sequence, and that sequence backtracks like any other. What cannot happen is give-back across
// the CLOSING PAREN, where the give-back is demanded by the pattern that follows the group.
static_assert(hits("\\b([A-Z]\\w*(?:Config|Data))\\s+x\\b", "PlayerConfigData x"),
              "inside the group \\w* gives back and the alternation lands — this is NOT the limit");
static_assert(hits("([A-Z][a-z]*)Data", "PlayerData"),
              "POSITIVE CONTROL: [a-z]* cannot eat the capital D, so the group ends itself");
static_assert(!hits("([A-Z]\\w*)Data", "PlayerConfigData"),
              "but \\w* swallows Data and nothing gives it back — the outer literal never lands");

// ---------------------------------------------------------------------------------------
// =(?!=) — "an equals sign that is not ==". The omission list calls this a one-character test
// rather than a lookahead; these pin that the claim HOLDS for the two call sites that used it,
// which is a different statement from "lookahead is unsupported".
//
// It holds because what follows the '=' in both patterns is a class that cannot match '=':
// \w in one, [a-zA-Z_] in the other. Drop the lookahead and '==' still cannot be matched.
// ---------------------------------------------------------------------------------------
static_assert(hits("(\\w+\\.\\w*_entity)\\s*=\\s*([a-zA-Z_]\\w*)\\s*;", "ref.sun_entity = sun;"),
              "POSITIVE CONTROL: the assignment the pattern is for still matches");
static_assert(!hits("(\\w+\\.\\w*_entity)\\s*=\\s*([a-zA-Z_]\\w*)\\s*;", "ref.sun_entity == sun;"),
              "and a comparison does NOT — [a-zA-Z_] cannot match the second '=' ");
// ---------------------------------------------------------------------------------------
// ALTERNATION IS FIRST-MATCH-WINS — a prefix ahead of a longer name shadows it
//
// match_alt returns the first alternative whose inner pattern matches; when the rest of the
// pattern then fails, no other alternative is tried. So "int" standing before "int8_t" makes
// the pattern fail on every int8_t, and it fails SILENTLY - a missing match, not an error.
//
// THIS IS NOT HYPOTHETICAL. Four patterns in ase-codegen carried "int" first and one carried
// "get" before "get_or_emplace"; under std::regex they worked, because std::regex backtracks.
// Rewriting them to str_match narrowed them without a word until these asserts were written.
// ---------------------------------------------------------------------------------------
static_assert(hits("\\bconstexpr\\s+(?:float|int|int8_t)\\s+([A-Z][A-Z0-9_]+)\\s*=",
                   "constexpr int MAX_LEVEL = 4;"),
              "POSITIVE CONTROL: the shadowing order still matches the SHORT name");
static_assert(!hits("\\bconstexpr\\s+(?:float|int|int8_t)\\s+([A-Z][A-Z0-9_]+)\\s*=",
                    "constexpr int8_t MAX_LEVEL = 4;"),
              "but the long name is lost when the prefix comes first");
static_assert(hits("\\bconstexpr\\s+(?:float|int8_t|int)\\s+([A-Z][A-Z0-9_]+)\\s*=",
                   "constexpr int8_t MAX_LEVEL = 4;"),
              "longest-first recovers it");
static_assert(hits("\\bconstexpr\\s+(?:float|int8_t|int)\\s+([A-Z][A-Z0-9_]+)\\s*=",
                   "constexpr int MAX_LEVEL = 4;"),
              "and does not cost the short name");
// The same shape on the registry verbs, which is where it was actually found.
static_assert(!hits("registry\\.(?:get|get_or_emplace)<", "registry.get_or_emplace<Foo>("),
              "get before get_or_emplace loses every get_or_emplace call");
static_assert(hits("registry\\.(?:get_or_emplace|get)<", "registry.get_or_emplace<Foo>("),
              "and the corrected order finds it");
static_assert(hits("registry\\.(?:get_or_emplace|get)<", "registry.get<Foo>("),
              "POSITIVE CONTROL: plain get still matches under the corrected order");

static_assert(hits("(?:const|let)\\s+\\w+\\s*=\\s*\\w+\\.\\w*_entity\\s*;",
                   "const sun_entity = ref.sun_entity;"),
              "POSITIVE CONTROL: the read form still matches");
static_assert(!hits("(?:const|let)\\s+\\w+\\s*=\\s*\\w+\\.\\w*_entity\\s*;",
                    "const sun_entity == ref.sun_entity;"),
              "and its comparison twin does not — \\w cannot match '='");

// ---------------------------------------------------------------------------------------
// CHARACTER CLASSES
// ---------------------------------------------------------------------------------------
static_assert(hits("[abc]", "b"), "a class matches any of its members");
static_assert(!hits("[abc]", "d"), "and nothing else");
static_assert(hits("[a-z]", "q"), "ranges work");
static_assert(hits("[0-9a-f]", "e"), "two ranges in one class work");
static_assert(!hits("[^0-9]", "5"), "a negated class rejects its members");
static_assert(hits("[^0-9]", "x"), "and accepts everything else");
static_assert(hits("[.]", "."), "a dot inside a class is literal");
static_assert(!hits("[.]", "x"), "so it does not match any character");
static_assert(hit_end("[0-9]+", "1234x") == 4, "a quantified class takes the whole run");

// ---------------------------------------------------------------------------------------
// ESCAPE CLASSES
// ---------------------------------------------------------------------------------------
static_assert(hits("\\w", "a") && hits("\\w", "Z") && hits("\\w", "_") && hits("\\w", "7"),
              "\\w covers letters, digits and underscore");
static_assert(!hits("\\w", "-"), "and not punctuation");
static_assert(hits("\\d", "5") && !hits("\\d", "x"), "\\d is digits only");
static_assert(hits("\\s", " ") && hits("\\s", "\t") && !hits("\\s", "x"), "\\s is whitespace");
static_assert(hits("\\W", "-") && !hits("\\W", "a"), "\\W is the complement of \\w");
static_assert(hits("\\D", "x") && !hits("\\D", "5"), "\\D is the complement of \\d");
static_assert(hits("\\S", "x") && !hits("\\S", " "), "\\S is the complement of \\s");
static_assert(hits("\\.", ".") && !hits("\\.", "x"), "an escaped dot is a literal dot");
static_assert(hits("\\(", "("), "an escaped paren is a literal paren");

// ---------------------------------------------------------------------------------------
// DOT
// ---------------------------------------------------------------------------------------
static_assert(hits(".", "x") && hits(".", "5") && hits(".", "-"), "dot matches any character");
static_assert(hit_end("a.c", "abc") == 3, "dot in the middle consumes exactly one");

// ---------------------------------------------------------------------------------------
// ANCHORS
// ---------------------------------------------------------------------------------------
static_assert(hits("^abc", "abcdef"), "caret matches at the start");
static_assert(!hits("^bcd", "abcdef"), "and only at the start");
static_assert(hits("def$", "abcdef"), "dollar matches at the end");
static_assert(!hits("abc$", "abcdef"), "and only at the end");
static_assert(hits("^abc$", "abc"), "both together anchor the whole subject");
static_assert(!hits("^abc$", "abcd"), "and reject anything longer");

// ---------------------------------------------------------------------------------------
// WORD BOUNDARIES — 140 patterns use \b, more than use bracket classes
// ---------------------------------------------------------------------------------------
static_assert(hits("\\bcat\\b", "the cat sat"), "\\b finds a standalone word");
static_assert(!hits("\\bcat\\b", "concatenate"), "and refuses one embedded in another");
static_assert(hits("\\bcat", "cat"), "\\b holds at the very start of the subject");
static_assert(hits("cat\\b", "cat"), "and at the very end");
static_assert(hits("\\Bcat", "concat"), "\\B is the complement: only inside a word");
// The redundant-lookahead finding, pinned as a test: \bPI\b alone already excludes PITCH.
static_assert(!hits("\\bPI\\b", "PITCH"),
              "the lookahead in \\bPI\\b(?!TCH) was redundant — \\b already excludes PITCH");
static_assert(hits("\\bPI\\b", "2 * PI"), "while the standalone constant still matches");

// ---------------------------------------------------------------------------------------
// ALTERNATION
// ---------------------------------------------------------------------------------------
static_assert(hits("cat|dog", "dog"), "the second alternative matches");
static_assert(hits("cat|dog", "cat"), "and so does the first");
static_assert(!hits("cat|dog", "bird"), "and neither matches something else");
static_assert(hits("(?:ab|cd)ef", "cdef"), "alternation inside a group binds to the group");
static_assert(!hits("(?:ab|cd)ef", "abcd"), "and not beyond it");

// ---------------------------------------------------------------------------------------
// CAPTURE GROUPS
// ---------------------------------------------------------------------------------------
static_assert(group_len("(\\w+)", "hello", 1) == 5, "group 1 captures the whole word");
static_assert(group_begin("x(\\w+)", "xabc", 1) == 1, "and starts after the literal");
static_assert(group_len("(\\w+)=(\\d+)", "count=42", 1) == 5, "first of two groups");
static_assert(group_len("(\\w+)=(\\d+)", "count=42", 2) == 2, "second of two groups");
static_assert(group_len("(?:\\w+)=(\\d+)", "count=42", 1) == 2,
              "a non-capturing group does not consume a group number");
static_assert(group_len("(a)(b)(c)(d)(e)(f)", "abcdef", 6) == 1,
              "all six capture groups are addressable — the measured maximum");
static_assert(group_len("(a)|(b)", "b", 2) == 1, "the taken alternative captures");
static_assert(group_len("(a)|(b)", "b", 1) == StrMatchNoGroup,
              "and the untaken one reports no group rather than an empty one");

// ---------------------------------------------------------------------------------------
// NESTING — 19 patterns in the tree nest groups two deep
// ---------------------------------------------------------------------------------------
static_assert(group_len("((\\w+))", "abc", 1) == 3, "outer group of a nested pair");
static_assert(group_len("((\\w+))", "abc", 2) == 3, "inner group of a nested pair");
static_assert(hits("(?:(?:a|b)c)+", "acbc"), "nested non-capturing groups under a quantifier");

// ---------------------------------------------------------------------------------------
// THE THREE REWRITTEN {n,m} PATTERNS — they must mean exactly what they meant before
// ---------------------------------------------------------------------------------------
// [lL]{0,2} became (?:[lL][lL]?)?  — zero, one or two, and never three.
static_assert(hit_end("^[uU](?:[lL][lL]?)?$", "u") == 1, "u alone is a valid suffix");
static_assert(hit_end("^[uU](?:[lL][lL]?)?$", "ul") == 2, "ul is valid");
static_assert(hit_end("^[uU](?:[lL][lL]?)?$", "ull") == 3, "ull is valid");
static_assert(!hits("^[uU](?:[lL][lL]?)?$", "ulll"),
              "ulll is NOT — this is what [lL]* would have wrongly accepted");
// [A-Z0-9_]{2,} became [A-Z0-9_][A-Z0-9_]+ — three characters minimum in total.
static_assert(hits("^[A-Z][A-Z0-9_][A-Z0-9_]+$", "MAX"), "a three-character constant matches");
static_assert(!hits("^[A-Z][A-Z0-9_][A-Z0-9_]+$", "PI"),
              "a two-character one does not — this is what + alone would have accepted");

// ---------------------------------------------------------------------------------------
// REAL PATTERNS FROM ase-codegen — the ones this header exists for
// ---------------------------------------------------------------------------------------
static_assert(group_len("(\\w+)\\s*=\\s*(\\d+)", "count = 42", 1) == 5,
              "assignment pattern, name side");
static_assert(group_len("(\\w+)\\s*=\\s*(\\d+)", "count = 42", 2) == 2,
              "assignment pattern, value side");
static_assert(hits("registry\\.(?:emplace_or_replace|get_or_emplace|emplace)<([\\w:]+)>\\s*\\(",
                   "registry.emplace<ase::Foo>("),
              "the emplace matcher, shortest alternative");
static_assert(group_len("registry\\.(?:emplace_or_replace|get_or_emplace|emplace)<([\\w:]+)>\\s*\\(",
                        "registry.emplace_or_replace<Bar>(", 1) == 3,
              "and it captures the component type from the longest alternative");
static_assert(hits("\\b(0[xX][0-9a-fA-F]+)(?:[uU](?:[lL][lL]?)?|[lL][lL]?[uU]?)\\b", "0xFFu"),
              "the hex-literal matcher with the rewritten suffix");
static_assert(group_len("(\\d+\\.\\d+)[fF]\\b", "3.5f", 1) == 3, "float literal with f suffix");

// ---------------------------------------------------------------------------------------
// str_match_full
// ---------------------------------------------------------------------------------------
static_assert(str_match_full("\\w+", "abc", 3), "full match over the whole subject");
static_assert(!str_match_full("\\w+", "abc def", 7), "and not over part of it");

// ---------------------------------------------------------------------------------------
// GUARDS — what a caller may hand in without breaking anything
// ---------------------------------------------------------------------------------------
static_assert(!hits("a", ""), "an empty subject cannot match a literal");
static_assert(hits("a*", ""), "but a star can match nothing at all");

// ---------------------------------------------------------------------------------------
// REPLACEMENT — 270 call sites, all of them global and most of them with $1
// ---------------------------------------------------------------------------------------

/** True when replacing `pattern` by `rep` in `txt` yields exactly `want`. */
inline bool repl_is(const char* pattern, const char* txt, const char* rep, const char* want) {
    return str_match_replace(pattern, std::string(txt), rep) == std::string(want);
}

/** How many matches the pattern finds — the same occurrences a replacement would rewrite. */
constexpr uint32_t repl_count(const char* pattern, const char* txt) {
    return str_match_count(pattern, txt, len_of(txt));
}

// COUNTING is constexpr and therefore settled while translating.
static_assert(repl_count("a", "aaa") == 3, "every occurrence is found, not just the first");
static_assert(repl_count("x", "abc") == 0, "and a pattern that misses counts nothing");
// Zero-width matches must not spin: a* matches empty at every position plus the end.
static_assert(repl_count("a*", "bb") == 3,
              "an empty match at each position plus the end, and the loop terminates");
static_assert(repl_count("\\d+", "a1b22c") == 2, "two digit runs, not three digits");

// REPLACEMENT is checked in the doctest cases below instead: its return type is std::string,
// which is not a literal type on this toolchain, so a constexpr version does not compile.

// ---------------------------------------------------------------------------------------
// str_match_contains — the "does it occur" question, 26 measured callers
// ---------------------------------------------------------------------------------------
static_assert(str_match_contains("\\bcat\\b", "the cat sat", 11), "finds a standalone word");
static_assert(!str_match_contains("\\bcat\\b", "concatenate", 11), "and refuses an embedded one");
static_assert(str_match_contains("a+", "bbaa", 4), "a quantified pattern is found mid-subject");
static_assert(!str_match_contains("a+", "bbbb", 4), "and reports absence as false, not as a hit");
static_assert(!str_match_contains(nullptr, "abc", 3), "a null pattern is refused, not dereferenced");

// ---------------------------------------------------------------------------------------
// str_match_next — the iteration primitive, and its zero-width guard
// ---------------------------------------------------------------------------------------

/** How many matches str_match_next yields before it stops — and whether it stops at all. */
constexpr uint32_t next_count(const char* pattern, const char* txt) {
    const uint32_t len = len_of(txt);
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    uint32_t pos = 0;
    uint32_t n = 0;
    // The bound is the safety net for THIS test, not for the function: if the advance ever
    // fails, a constexpr evaluation would otherwise run into the compiler's step limit and
    // report something unrelated to the actual defect.
    while (n < 100 && str_match_next(pattern, txt, len, pos, gb, ge, GROUPS)) {
        ++n;
    }
    return n;
}

/** Start offset of the n-th match, counting from 1 — proves the offsets are ABSOLUTE. */
constexpr int32_t next_begin_of(const char* pattern, const char* txt, uint32_t nth = 1) {
    const uint32_t len = len_of(txt);
    int32_t gb[GROUPS + 1] = {};
    int32_t ge[GROUPS + 1] = {};
    uint32_t pos = 0;
    uint32_t n = 0;
    while (str_match_next(pattern, txt, len, pos, gb, ge, GROUPS)) {
        ++n;
        if (n == nth) return gb[0];
    }
    return StrMatchNoGroup;
}

static_assert(next_count("a", "aaa") == 3, "three separate matches, three iterations");
static_assert(next_count("\\d+", "a1b22c333") == 3, "runs are one match each, not one per digit");
static_assert(next_count("x", "abc") == 0, "no match, no iteration");
// THE CASE THIS FUNCTION EXISTS FOR: an empty match must still advance, or the loop never ends.
static_assert(next_count("a*", "bb") == 3,
              "empty match at each position plus the end — and it TERMINATES");
static_assert(next_count("", "abcd") == 5, "the empty pattern likewise terminates");

// Absolute offsets: the second "cat" sits at index 8, not at index 0 of the remaining tail.
static_assert(next_begin_of("cat", "cat, cat") == 0, "first match, absolute offset");
static_assert(next_begin_of("cat", "cat, cat", 2) == 5,
              "SECOND match reports its offset in the whole subject, not in the tail");
static_assert(next_begin_of("(\\w+)=", "a=1 bb=2", 2) == 4, "and so do capture groups");

// ---------------------------------------------------------------------------------------
// str_split_next — no matcher involved, same advance discipline
// ---------------------------------------------------------------------------------------

/** How many fields a split yields — and whether it terminates. */
constexpr uint32_t split_count(const char* txt, char sep) {
    const uint32_t len = len_of(txt);
    uint32_t pos = 0;
    const char* field = nullptr;
    uint32_t field_len = 0;
    uint32_t n = 0;
    while (n < 100 && str_split_next(txt, len, sep, pos, &field, &field_len)) {
        ++n;
    }
    return n;
}

/** Length of the n-th field, counting from 1. */
constexpr uint32_t split_len_of(const char* txt, char sep, uint32_t nth) {
    const uint32_t len = len_of(txt);
    uint32_t pos = 0;
    const char* field = nullptr;
    uint32_t field_len = 0;
    uint32_t n = 0;
    while (str_split_next(txt, len, sep, pos, &field, &field_len)) {
        ++n;
        if (n == nth) return field_len;
    }
    return 0;
}

static_assert(split_count("a,b,c", ',') == 3, "three fields");
static_assert(split_count("abc", ',') == 1, "no separator is one field, not zero");
static_assert(split_count("", ',') == 1, "the empty subject is one empty field");
// The zero-width equivalent for a split: consecutive separators, and a trailing one.
static_assert(split_count("a,,b", ',') == 3, "an empty field between two separators is a field");
static_assert(split_count("a,b,", ',') == 3, "a trailing separator yields a final empty field");
static_assert(split_count(",,,", ',') == 4, "and only separators still TERMINATES");

static_assert(split_len_of("aa,bbb,c", ',', 1) == 2, "first field length");
static_assert(split_len_of("aa,bbb,c", ',', 2) == 3, "second field length");
static_assert(split_len_of("aa,bbb,c", ',', 3) == 1, "third field length");
static_assert(split_len_of("a,,b", ',', 2) == 0, "the empty field has length zero");
// The shape the seven ase-codegen call sites need: whitespace stays, they trim it themselves.
static_assert(split_len_of("a, b", ',', 2) == 2, "leading whitespace is NOT trimmed away here");

}  // namespace

TEST_CASE("strmatch: literals and position") {
    CHECK(hits("abc", "abc"));
    CHECK(hits("abc", "xxabcyy"));
    CHECK_FALSE(hits("abc", "abd"));
    CHECK(hit_begin("abc", "xxabc") == 2);
    CHECK(hit_end("abc", "xxabc") == 5);
}

TEST_CASE("strmatch: greedy and lazy differ where it matters") {
    CHECK(hit_end("a*", "aaa") == 3);
    CHECK(hit_end("a+?", "aaa") == 1);
    // The tree's comment matcher — the case that makes lazy quantifiers non-optional.
    CHECK(hit_end("/\\*[\\s\\S]*?\\*/", "/*one*/ /*two*/") == 7);
    CHECK(hit_end("/\\*[\\s\\S]*\\*/", "/*one*/ /*two*/") == 15);
}

TEST_CASE("strmatch: character and escape classes") {
    CHECK(hits("[a-z]", "q"));
    CHECK_FALSE(hits("[^0-9]", "5"));
    CHECK(hits("\\w", "_"));
    CHECK_FALSE(hits("\\w", "-"));
    CHECK(hits("\\s", "\t"));
    CHECK(hit_end("[0-9]+", "1234x") == 4);
}

TEST_CASE("strmatch: anchors and word boundaries") {
    CHECK(hits("^abc$", "abc"));
    CHECK_FALSE(hits("^abc$", "abcd"));
    CHECK(hits("\\bcat\\b", "the cat sat"));
    CHECK_FALSE(hits("\\bcat\\b", "concatenate"));
    // The redundant lookahead, measured 2026-08-20 across all 340 patterns.
    CHECK_FALSE(hits("\\bPI\\b", "PITCH"));
    CHECK(hits("\\bPI\\b", "2 * PI"));
}

TEST_CASE("strmatch: alternation and groups") {
    CHECK(hits("cat|dog", "dog"));
    CHECK_FALSE(hits("cat|dog", "bird"));
    CHECK(group_len("(\\w+)=(\\d+)", "count=42", 1) == 5);
    CHECK(group_len("(\\w+)=(\\d+)", "count=42", 2) == 2);
    CHECK(group_len("(?:\\w+)=(\\d+)", "count=42", 1) == 2);
    CHECK(group_len("(a)(b)(c)(d)(e)(f)", "abcdef", 6) == 1);
    CHECK(group_len("(a)|(b)", "b", 1) == StrMatchNoGroup);
}

TEST_CASE("strmatch: the rewritten counted quantifiers keep their meaning") {
    CHECK(hit_end("^[uU](?:[lL][lL]?)?$", "ull") == 3);
    CHECK_FALSE(hits("^[uU](?:[lL][lL]?)?$", "ulll"));
    CHECK(hits("^[A-Z][A-Z0-9_][A-Z0-9_]+$", "MAX"));
    CHECK_FALSE(hits("^[A-Z][A-Z0-9_][A-Z0-9_]+$", "PI"));
}

TEST_CASE("strmatch: patterns taken from ase-codegen") {
    CHECK(group_len("(\\w+)\\s*=\\s*(\\d+)", "count = 42", 1) == 5);
    CHECK(hits("registry\\.(?:emplace_or_replace|get_or_emplace|emplace)<([\\w:]+)>\\s*\\(",
               "registry.emplace<ase::Foo>("));
    CHECK(hits("\\b(0[xX][0-9a-fA-F]+)(?:[uU](?:[lL][lL]?)?|[lL][lL]?[uU]?)\\b", "0xFFu"));
    CHECK(group_len("(\\d+\\.\\d+)[fF]\\b", "3.5f", 1) == 3);
}

TEST_CASE("strmatch: replacement is global and resolves $N") {
    // These carried static_assert until 2026-08-20. They moved down here unchanged when the
    // replacement started returning std::string — not one case was dropped in the move.
    CHECK(repl_is("cat", "the cat", "dog", "the dog"));
    CHECK(repl_is("a", "aaa", "b", "bbb"));
    CHECK(repl_is("x", "abc", "y", "abc"));
    CHECK(repl_is("(\\w+)=(\\d+)", "count=42", "$2=$1", "42=count"));
    CHECK(repl_is("\\d+", "a1b22c", "[$0]", "a[1]b[22]c"));
    CHECK(repl_is("a", "a", "$$", "$"));
    CHECK(repl_is("(a)|(b)", "b", "<$2>", "<b>"));
    CHECK(repl_is("(a)|(b)", "b", "<$1>", "<>"));
    CHECK(repl_is("(a)", "a", "$9", ""));
    CHECK(repl_is("registry\\.emplace<([\\w:]+)>", "registry.emplace<Foo>", "add<$1>",
                  "add<Foo>"));
    CHECK(repl_is("(\\d+\\.\\d+)[fF]\\b", "x = 3.5f;", "$1", "x = 3.5;"));
}

TEST_CASE("strmatch: replacement grows with the result instead of truncating") {
    // The buffer form this replaced would have needed a size guess here. The string form
    // simply returns what the rewrite produces — 10 matches, 10 characters, no ceiling.
    CHECK(str_match_replace("a", std::string("aaaaaaaaaa"), "bb").size() == 20);
    CHECK(str_match_count("a", "aaaaaaaaaa", 10) == 10);
}

TEST_CASE("strmatch: guards") {
    CHECK_FALSE(hits("a", ""));
    CHECK(hits("a*", ""));
    // A null pattern or subject is refused rather than dereferenced.
    int32_t gb[StrMatchMaxGroups + 1] = {};
    int32_t ge[StrMatchMaxGroups + 1] = {};
    CHECK_FALSE(str_match_search(nullptr, "abc", 3, gb, ge, StrMatchMaxGroups));
    CHECK_FALSE(str_match_search("abc", nullptr, 3, gb, ge, StrMatchMaxGroups));
    CHECK_FALSE(str_match_search("abc", "abc", 3, nullptr, ge, StrMatchMaxGroups));
}
