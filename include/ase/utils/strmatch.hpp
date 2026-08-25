#pragma once

/**
 * @file        strmatch.hpp
 * @brief       Pattern matching with capture groups — the ASE stand-in for std::regex
 * @description std::regex is validator-forbidden project-wide, and the rule text names
 *              ase::utils as the destination. Until 2026-08-20 that destination did not
 *              exist, which left 665 findings in ase-codegen with nowhere to go. This is
 *              the destination.
 *
 *              WHAT IT SUPPORTS, AND WHY EXACTLY THIS SET. Measured on 2026-08-20 across
 *              all 340 real regex literals in core/ase-codegen (not on the 665 findings —
 *              those are call sites, a pattern is used at several). Share of patterns
 *              using each feature:
 *
 *                quantifiers * + ?        295   86.8 %   supported
 *                char classes \w \d \s    259   76.2 %   supported
 *                capturing groups ( )     212   62.4 %   supported, with ONE limit — read it
 *                word boundary \b         140   41.2 %   supported
 *                bracket classes [...]    138   40.6 %   supported
 *                anchors ^ $              101   29.7 %   supported
 *                non-capturing (?:         59   17.4 %   supported
 *                alternation |             38   11.2 %   supported
 *                lazy quantifiers *? +?    18    5.3 %   supported
 *                dot .                      3    0.9 %   supported
 *                counted {n,m}              3    0.9 %   NOT supported, see below
 *                lookahead (?= (?!          3    0.9 %   NOT supported, see below
 *                backreference \1 in pattern 1  0.3 %   NOT supported, see below
 *                lookbehind (?<             0    0.0 %   NOT supported
 *
 *              THE FOUR OMISSIONS ARE MEASURED, NOT CONVENIENT. Each was opened and read:
 *
 *              {n,m} — the three uses are \b(0[xX][0-9a-fA-F]+)(?:[uU][lL]{0,2}|[lL]{1,2}[uU]?)\b
 *                and its sibling, plus \b([A-Z][A-Z0-9_]{2,})\b. All three bounds are small
 *                enough to write exactly: X{0,2} is (?:XX?)?, X{1,2} is XX?, X{2,} is XX+.
 *                Writing them as X* or X+ instead WOULD lose meaning — X* accepts "ulllll",
 *                which is not a C++ suffix. The exact forms accept neither.
 *              lookahead — three uses, and one of them does nothing at all: \bPI\b(?!TCH)
 *                cannot match inside PITCH anyway, because there is no word boundary between
 *                I and T. The other two are =(?!=), "an equals sign that is not ==", which is
 *                a one-character test on the following position, not a lookahead.
 *              backreference in pattern — exactly one:
 *                (\w+)\.begin\s*\(\s*\)\s*(==|!=)\s*\1\.end\s*\(\s*\) — "the same identifier
 *                on both sides". The caller can match without the \1 and compare group 1
 *                against group 3 itself, which is a string comparison, not backtracking.
 *              lookbehind — zero uses.
 *
 *              Adding any of the four would mean carrying machinery for at most three call
 *              sites, and the last one would turn a bounded matcher into one that can
 *              backtrack across group boundaries.
 *
 *              THE ONE LIMIT ON GROUPS — THE REST OF A PATTERN DOES NOT REACH INTO A GROUP.
 *              match_atom matches a group with match_alt against the group's own pattern only
 *              and returns exactly ONE advance. If the rest of the pattern then fails, the
 *              group is never retried at a different end. A real engine would give characters
 *              back across that boundary; this one does not.
 *
 *              MEASURED 2026-08-20 against "registry.view<PositionComponent>", positive
 *              control built into the probe:
 *                  view<([^>]+)>       hit,  group length 17   ← the control: correct
 *                  view<([\s\S]+?)>    NO HIT
 *                  view<[\s\S]+?>      hit                     ← same laziness, no group
 *              The third line is what makes this a group finding and not a lazy-quantifier
 *              one: drop the parentheses and the identical quantifier works.
 *
 *              SO: a group must END ITSELF. A negated class ([^>]+ before '>', [^)]* before
 *              ')') or \w+ before a non-word character cannot swallow its own terminator, and
 *              those are what the 212 call sites use — not by luck, that is how patterns in
 *              this tree are written. A group that needs the FOLLOWING character as its signal
 *              is the failing shape, and it fails SILENTLY: no error, no exception, just "no
 *              match". In the transpiler that reads as "this rewrite did not apply", and the
 *              generated client is wrong with every gate green.
 *
 *              WHERE THE EDGE IS NOT — and this was got wrong once, on the day it was written,
 *              which is why it is spelled out. Give-back INSIDE a group works normally:
 *              ([A-Z]\w*(?:Config|Data)) matches "PlayerConfigData", because \w* and the
 *              alternation after it sit in the SAME inner sequence, and that sequence
 *              backtracks like any other. The limit is only give-back across the closing
 *              paren, demanded by the pattern that FOLLOWS the group.
 *
 *              Two shapes to recognise before writing one:
 *                  ([\s\S]+?)>        needs the '>' to know where to stop — write ([^>]+)>
 *                  ([A-Z]\w*)Data     \w* swallows "Data" and nothing hands it back, so the
 *                                     literal after the group never lands
 *              The static_asserts in tests/strmatch_test.cpp pin both, each next to a positive
 *              control, so a later rewrite that adds continuation passing announces itself by
 *              failing them rather than by changing behaviour quietly.
 *
 *              THE SAME CAUSE IN ITS SECOND SHAPE — ALTERNATION IS FIRST-MATCH-WINS.
 *              match_alt returns the first alternative whose own pattern matches. If the rest
 *              of the pattern then fails, no other alternative is tried, because that retry
 *              would also have to cross the group edge.
 *
 *              CONSEQUENCE, AND IT IS THE ONE THAT ACTUALLY BIT: when one alternative is a
 *              PREFIX of another, the longer one must come FIRST.
 *                  (float|int|int8_t)     loses every int8_t - "int" matches, then the rest
 *                                         of the pattern meets the '8' and the whole thing fails
 *                  (float|int8_t|int)     correct, and still matches plain "int"
 *              MEASURED 2026-08-20 in four cells, the short name as control:
 *                  int-first    on "constexpr int8_t"  MISS   ·  on "constexpr int"  HIT
 *                  longest-first on "constexpr int8_t"  HIT   ·  on "constexpr int"  HIT
 *
 *              Five patterns in ase-codegen carried the shadowing order and worked, because
 *              std::regex backtracks. Rewriting them to str_match narrowed them SILENTLY -
 *              every int8_t/int16_t/int32_t/int64_t constant and every registry.get_or_emplace
 *              call simply stopped being seen. Nothing failed; the output was just missing
 *              entries. Whoever rewrites a pattern from std::regex to here checks its
 *              alternations for prefixes FIRST, before anything else.
 *
 *              THE ALGORITHM IS RECURSIVE BACKTRACKING over the pattern itself; there is no
 *              compile step and no allocation. Alternation and groups are what force this —
 *              with them a matcher stops being a loop. Recursion depth is capped
 *              (StrMatchMaxDepth) and every failure path returns, so a pathological pattern
 *              costs time, never the stack.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    structure/datatype/textual
 * @created     2026-08-20
 * @modified    2026-08-20
 * @version     1.0.0
 */

#include <cstdint>
#include <string>  // only for the replacement result; the matcher itself touches no std type

namespace ase::utils {

// Highest capture group the tree actually uses: $6 appears once, $5 once, $4 three times.
constexpr uint32_t StrMatchMaxGroups = 6;
// Longest measured pattern is 292 characters; this is the buffer callers should size to.
constexpr uint32_t StrMatchMaxPattern = 320;
// Recursion ceiling. Deepest measured group nesting is 2; 64 leaves room for quantifier
// backtracking on long inputs without ever reaching the real stack limit.
constexpr uint32_t StrMatchMaxDepth = 64;
// Returned for a group that did not participate in the match.
constexpr int32_t StrMatchNoGroup = -1;

namespace strmatch_detail {

/** True if c is [A-Za-z0-9_] — the \w class, and the character set \b is defined against. */
constexpr bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_digit_char(char c) {
    return c >= '0' && c <= '9';
}

constexpr bool is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/** Match one escaped class or literal: \w \d \s \W \D \S \b handled by the caller. */
constexpr bool escape_matches(char esc, char c) {
    if (esc == 'w') return is_word_char(c);
    if (esc == 'W') return !is_word_char(c);
    if (esc == 'd') return is_digit_char(c);
    if (esc == 'D') return !is_digit_char(c);
    if (esc == 's') return is_space_char(c);
    if (esc == 'S') return !is_space_char(c);
    if (esc == 'n') return c == '\n';
    if (esc == 't') return c == '\t';
    if (esc == 'r') return c == '\r';
    return esc == c;  // \. \( \\ \* … — the escape stands for itself
}

/**
 * Find the end of the bracket class that starts at pat[p] == '['.
 * Returns the index just past the closing ']'. A ']' as the first member is literal,
 * which is why the scan starts after an optional '^' and an optional leading ']'.
 */
constexpr uint32_t class_end(const char* pat, uint32_t pat_len, uint32_t p) {
    uint32_t i = p + 1;
    if (i < pat_len && pat[i] == '^') ++i;
    if (i < pat_len && pat[i] == ']') ++i;
    while (i < pat_len && pat[i] != ']') {
        if (pat[i] == '\\' && i + 1 < pat_len) ++i;
        ++i;
    }
    return (i < pat_len) ? i + 1 : pat_len;
}

/** Does character c match the bracket class spanning [begin, end) (end is past ']')? */
constexpr bool class_matches(const char* pat, uint32_t begin, uint32_t end, char c) {
    uint32_t i = begin + 1;
    bool negated = false;
    if (i < end && pat[i] == '^') {
        negated = true;
        ++i;
    }
    const uint32_t last = (end > begin) ? end - 1 : begin;  // index of the ']'
    bool hit = false;
    while (i < last) {
        if (pat[i] == '\\' && i + 1 < last) {
            if (escape_matches(pat[i + 1], c)) hit = true;
            i += 2;
            continue;
        }
        // Range a-z, but only when the '-' is not the last member.
        if (i + 2 < last && pat[i + 1] == '-') {
            if (c >= pat[i] && c <= pat[i + 2]) hit = true;
            i += 3;
            continue;
        }
        if (pat[i] == c) hit = true;
        ++i;
    }
    return negated ? !hit : hit;
}

/**
 * Find the end of the group that opens at pat[p] == '('.
 * Returns the index just past the matching ')'. Nested groups and escaped parentheses
 * are counted correctly; a '(' inside a bracket class is not a group.
 */
constexpr uint32_t group_end(const char* pat, uint32_t pat_len, uint32_t p) {
    uint32_t depth = 0;
    uint32_t i = p;
    while (i < pat_len) {
        if (pat[i] == '\\' && i + 1 < pat_len) {
            i += 2;
            continue;
        }
        if (pat[i] == '[') {
            i = class_end(pat, pat_len, i);
            continue;
        }
        if (pat[i] == '(') ++depth;
        if (pat[i] == ')') {
            --depth;
            if (depth == 0) return i + 1;
        }
        ++i;
    }
    return pat_len;
}

/**
 * End of the atom starting at p — the unit a quantifier would apply to.
 * A group counts as one atom, a bracket class as one, an escape as two characters.
 */
constexpr uint32_t atom_end(const char* pat, uint32_t pat_len, uint32_t p) {
    if (p >= pat_len) return pat_len;
    if (pat[p] == '(') return group_end(pat, pat_len, p);
    if (pat[p] == '[') return class_end(pat, pat_len, p);
    if (pat[p] == '\\' && p + 1 < pat_len) return p + 2;
    return p + 1;
}

/**
 * Split [begin, end) at top-level '|' characters. Returns the index of the first
 * top-level '|', or `end` when there is none. Alternatives inside groups or classes
 * do not count.
 */
constexpr uint32_t alt_split(const char* pat, uint32_t begin, uint32_t end) {
    uint32_t i = begin;
    while (i < end) {
        if (pat[i] == '\\' && i + 1 < end) {
            i += 2;
            continue;
        }
        if (pat[i] == '[') {
            i = class_end(pat, end, i);
            continue;
        }
        if (pat[i] == '(') {
            i = group_end(pat, end, i);
            continue;
        }
        if (pat[i] == '|') return i;
        ++i;
    }
    return end;
}

/**
 * Which capture group does the '(' at position p belong to?
 *
 * Counted from the pattern, not from the run: it is the number of capturing parentheses that
 * open at or before p. This is the only correct source. MEASURED 2026-08-20: the first version
 * used a running counter in MatchCtx that was reset whenever an alternative failed — so in
 * "(a)|(b)" the group (b) was numbered 1 instead of 2 as soon as (a) had been tried and
 * rejected. Four static_asserts caught it. A group's number is a property of where it stands
 * in the pattern; nothing that happens while matching can change it.
 */
constexpr uint32_t group_number(const char* pat, uint32_t pat_len, uint32_t p) {
    uint32_t n = 0;
    uint32_t i = 0;
    while (i <= p && i < pat_len) {
        if (pat[i] == '\\') {
            i += 2;
            continue;
        }
        if (pat[i] == '[') {
            i = class_end(pat, pat_len, i);
            continue;
        }
        if (pat[i] == '(' && !(i + 2 < pat_len && pat[i + 1] == '?' && pat[i + 2] == ':')) {
            ++n;
        }
        ++i;
    }
    return n;
}

/** Carries everything the recursion needs, so the parameter lists stay readable. */
struct MatchCtx {
    const char* pat = nullptr;
    uint32_t pat_len = 0;
    const char* txt = nullptr;
    uint32_t txt_len = 0;
    int32_t* gbeg = nullptr;
    int32_t* gend = nullptr;
    uint32_t max_groups = 0;
};

// Forward declaration: sequence and alternation call each other through groups.
constexpr int32_t match_alt(MatchCtx& ctx, uint32_t pbeg, uint32_t pend, uint32_t t,
                         uint32_t depth);

/**
 * Match a single non-quantified atom at pattern position p against text position t.
 * Returns the text position after the atom, or StrMatchNoGroup on failure.
 * Zero-width atoms (^ $ \b) return t unchanged on success.
 */
constexpr int32_t match_atom(MatchCtx& ctx, uint32_t p, uint32_t aend, uint32_t t,
                          uint32_t depth) {
    const char* pat = ctx.pat;

    if (pat[p] == '^') {
        return (t == 0) ? static_cast<int32_t>(t) : StrMatchNoGroup;
    }
    if (pat[p] == '$') {
        return (t == ctx.txt_len) ? static_cast<int32_t>(t) : StrMatchNoGroup;
    }
    if (pat[p] == '\\' && p + 1 < ctx.pat_len && pat[p + 1] == 'b') {
        const bool left = (t > 0) && is_word_char(ctx.txt[t - 1]);
        const bool right = (t < ctx.txt_len) && is_word_char(ctx.txt[t]);
        return (left != right) ? static_cast<int32_t>(t) : StrMatchNoGroup;
    }
    if (pat[p] == '\\' && p + 1 < ctx.pat_len && pat[p + 1] == 'B') {
        const bool left = (t > 0) && is_word_char(ctx.txt[t - 1]);
        const bool right = (t < ctx.txt_len) && is_word_char(ctx.txt[t]);
        return (left == right) ? static_cast<int32_t>(t) : StrMatchNoGroup;
    }

    if (pat[p] == '(') {
        // Non-capturing when it opens with "(?:", capturing otherwise.
        const bool capturing = !(p + 2 < ctx.pat_len && pat[p + 1] == '?' && pat[p + 2] == ':');
        const uint32_t inner_begin = capturing ? p + 1 : p + 3;
        const uint32_t inner_end = (aend > p) ? aend - 1 : p;  // strip the ')'

        const uint32_t slot = capturing ? group_number(ctx.pat, ctx.pat_len, p) : 0;

        const int32_t r = match_alt(ctx, inner_begin, inner_end, t, depth + 1);
        if (r == StrMatchNoGroup) {
            return StrMatchNoGroup;
        }
        if (capturing && slot <= ctx.max_groups) {
            ctx.gbeg[slot] = static_cast<int32_t>(t);
            ctx.gend[slot] = r;
        }
        return r;
    }

    // Everything below consumes exactly one character.
    if (t >= ctx.txt_len) return StrMatchNoGroup;
    const char c = ctx.txt[t];

    if (pat[p] == '[') {
        return class_matches(pat, p, aend, c) ? static_cast<int32_t>(t + 1) : StrMatchNoGroup;
    }
    if (pat[p] == '.') {
        return static_cast<int32_t>(t + 1);
    }
    if (pat[p] == '\\' && p + 1 < ctx.pat_len) {
        return escape_matches(pat[p + 1], c) ? static_cast<int32_t>(t + 1) : StrMatchNoGroup;
    }
    return (pat[p] == c) ? static_cast<int32_t>(t + 1) : StrMatchNoGroup;
}

/**
 * Match the sequence [p, pend) against the text from t.
 * Returns the end position in the text, or StrMatchNoGroup.
 *
 * The quantifier handling is where the backtracking lives: a greedy quantifier tries the
 * longest run first and gives characters back one at a time until the REST of the pattern
 * also matches; a lazy one starts short and grows. Both must re-run the rest after every
 * step, which is why this function calls itself with the remaining pattern.
 */
constexpr int32_t match_seq(MatchCtx& ctx, uint32_t p, uint32_t pend, uint32_t t,
                         uint32_t depth) {
    if (depth > StrMatchMaxDepth) return StrMatchNoGroup;
    if (p >= pend) return static_cast<int32_t>(t);

    const uint32_t aend = atom_end(ctx.pat, pend, p);

    // Quantifier directly after the atom?
    char quant = '\0';
    bool lazy = false;
    uint32_t rest = aend;
    if (aend < pend && (ctx.pat[aend] == '*' || ctx.pat[aend] == '+' || ctx.pat[aend] == '?')) {
        quant = ctx.pat[aend];
        rest = aend + 1;
        if (rest < pend && ctx.pat[rest] == '?') {
            lazy = true;
            ++rest;
        }
    }

    if (quant == '\0') {
        const int32_t after = match_atom(ctx, p, aend, t, depth);
        if (after == StrMatchNoGroup) return StrMatchNoGroup;
        return match_seq(ctx, rest, pend, static_cast<uint32_t>(after), depth + 1);
    }

    const uint32_t min_count = (quant == '+') ? 1 : 0;
    const uint32_t max_count = (quant == '?') ? 1 : 0xFFFFFFFFu;

    if (lazy) {
        // Shortest first: try the rest after min_count repetitions, then take one more.
        uint32_t count = 0;
        uint32_t pos = t;
        for (;;) {
            if (count >= min_count) {
                const int32_t r = match_seq(ctx, rest, pend, pos, depth + 1);
                if (r != StrMatchNoGroup) return r;
            }
            if (count >= max_count) return StrMatchNoGroup;
            const int32_t adv = match_atom(ctx, p, aend, pos, depth);
            if (adv == StrMatchNoGroup) return StrMatchNoGroup;
            if (static_cast<uint32_t>(adv) == pos) return StrMatchNoGroup;  // zero-width guard
            pos = static_cast<uint32_t>(adv);
            ++count;
        }
    }

    // Greedy: first count how far the atom reaches, then walk back one repetition at a time.
    //
    // The obvious implementation remembers every stop position in an array and indexes back
    // into it. It is not used here: that array would need one entry per repetition, it lives
    // on the stack, and this function recurses — a 320-entry array at depth 64 is 80 KB of
    // stack for a matcher that is supposed to be free of allocation. Re-walking costs a second
    // pass over a run that has already been shown to match, which for the measured patterns
    // (\w+ over identifiers, \s* over indentation) is a handful of characters.
    uint32_t reach = 0;
    uint32_t pos = t;
    while (reach < max_count) {
        const int32_t adv = match_atom(ctx, p, aend, pos, depth);
        if (adv == StrMatchNoGroup) break;
        if (static_cast<uint32_t>(adv) == pos) break;  // zero-width guard
        pos = static_cast<uint32_t>(adv);
        ++reach;
    }

    // The atom did not reach the required minimum, so no number of give-backs will help.
    // MEASURED 2026-08-20: without this line `a+` behaved like `a*` — the loop below tried
    // take = 0 BEFORE checking take against min_count, and an empty run reported success.
    // Three static_asserts caught it, among them !hits("a+", "bbb").
    if (reach < min_count) return StrMatchNoGroup;

    uint32_t take = reach;
    for (;;) {
        // Re-walk to exactly `take` repetitions. This also re-establishes the capture groups
        // of the last repetition, which is what a caller reading $1 after a quantified group
        // expects to see.
        uint32_t at = t;
        bool ok = true;
        for (uint32_t k = 0; k < take; ++k) {
            const int32_t adv = match_atom(ctx, p, aend, at, depth);
            if (adv == StrMatchNoGroup) {
                ok = false;
                break;
            }
            at = static_cast<uint32_t>(adv);
        }
        if (ok) {
            const int32_t r = match_seq(ctx, rest, pend, at, depth + 1);
            if (r != StrMatchNoGroup) return r;
        }
        if (take <= min_count) return StrMatchNoGroup;
        --take;
    }
}

/** Try each top-level alternative of [pbeg, pend) in turn. */
constexpr int32_t match_alt(MatchCtx& ctx, uint32_t pbeg, uint32_t pend, uint32_t t,
                         uint32_t depth) {
    if (depth > StrMatchMaxDepth) return StrMatchNoGroup;
    uint32_t begin = pbeg;
    for (;;) {
        const uint32_t bar = alt_split(ctx.pat, begin, pend);
        const int32_t r = match_seq(ctx, begin, bar, t, depth + 1);
        if (r != StrMatchNoGroup) return r;
        if (bar >= pend) return StrMatchNoGroup;
        begin = bar + 1;
    }
}

}  // namespace strmatch_detail

/**
 * Search `text` for the first position where `pattern` matches.
 *
 * @param pattern     null-terminated pattern, at most StrMatchMaxPattern characters
 * @param text        subject, NOT required to be null-terminated
 * @param text_len    length of the subject
 * @param group_begin out: start offsets, at least max_groups+1 entries; [0] is the whole match
 * @param group_end   out: end offsets (one past the last character), same size
 * @param max_groups  how many capture groups the caller has room for (<= StrMatchMaxGroups)
 * @return true when a match was found. Groups that did not participate are StrMatchNoGroup.
 *
 * Both output arrays are filled with StrMatchNoGroup before the search, so a caller may
 * read every slot after a true return without checking which groups the pattern has.
 */
constexpr bool str_match_search(const char* pattern, const char* text, uint32_t text_len,
                             int32_t* group_begin, int32_t* group_end, uint32_t max_groups) {
    if (pattern == nullptr || text == nullptr) return false;
    if (group_begin == nullptr || group_end == nullptr) return false;
    if (max_groups > StrMatchMaxGroups) max_groups = StrMatchMaxGroups;

    uint32_t pat_len = 0;
    while (pattern[pat_len] != '\0' && pat_len < StrMatchMaxPattern) ++pat_len;

    for (uint32_t i = 0; i <= max_groups; ++i) {
        group_begin[i] = StrMatchNoGroup;
        group_end[i] = StrMatchNoGroup;
    }

    for (uint32_t start = 0; start <= text_len; ++start) {
        strmatch_detail::MatchCtx ctx;
        ctx.pat = pattern;
        ctx.pat_len = pat_len;
        ctx.txt = text;
        ctx.txt_len = text_len;
        ctx.gbeg = group_begin;
        ctx.gend = group_end;
        ctx.max_groups = max_groups;

        const int32_t r = strmatch_detail::match_alt(ctx, 0, pat_len, start, 0);
        if (r != StrMatchNoGroup) {
            group_begin[0] = static_cast<int32_t>(start);
            group_end[0] = r;
            return true;
        }
        // A failed attempt may have written groups; clear them before the next start.
        for (uint32_t i = 1; i <= max_groups; ++i) {
            group_begin[i] = StrMatchNoGroup;
            group_end[i] = StrMatchNoGroup;
        }
    }
    return false;
}

/**
 * True when `pattern` occurs anywhere in the subject — the "does this contain" question,
 * without the capture groups.
 *
 * MEASURED 2026-08-20: the tree holds 47 std::regex_search calls, and 26 of them use the result
 * as a plain bool (in an if, in a return, behind a !). Written against str_match_search those 26
 * would each have to declare two group arrays that nobody reads — 52 buffers as scaffolding.
 * That is why this exists; it is not a convenience.
 */
constexpr bool str_match_contains(const char* pattern, const char* text, uint32_t text_len) {
    int32_t gb[StrMatchMaxGroups + 1] = {};
    int32_t ge[StrMatchMaxGroups + 1] = {};
    return str_match_search(pattern, text, text_len, gb, ge, 0);
}

/**
 * Find the next match at or after `pos`, and advance `pos` past it.
 *
 * This is the iteration primitive. The caller keeps its own loop:
 *
 *     uint32_t pos = 0;
 *     int32_t gb[StrMatchMaxGroups + 1], ge[StrMatchMaxGroups + 1];
 *     while (str_match_next(pattern, text, len, pos, gb, ge, StrMatchMaxGroups)) {
 *         // gb[n] / ge[n] are offsets into `text`, not into the remaining tail
 *     }
 *
 * WHY THIS EXISTS RATHER THAN TEN HAND-WRITTEN LOOPS. Ten callers advancing a position by hand
 * are ten chances to get the ZERO-WIDTH case wrong, and a position that fails to advance is not
 * a wrong result — it is a transpiler that hangs, with no error and no line. The advance lives
 * here, once, and it is the same one str_match_replace uses internally.
 *
 * The offsets are ABSOLUTE. str_match_search reports relative to the pointer it was handed, and
 * every caller of a raw search-from-offset has to add `pos` back by hand; that addition happens
 * here instead.
 *
 * @return true when a match was found; false when the subject is exhausted.
 */
constexpr bool str_match_next(const char* pattern, const char* text, uint32_t text_len,
                              uint32_t& pos, int32_t* group_begin, int32_t* group_end,
                              uint32_t max_groups) {
    if (pattern == nullptr || text == nullptr || pos > text_len) return false;
    if (group_begin == nullptr || group_end == nullptr) return false;
    if (max_groups > StrMatchMaxGroups) max_groups = StrMatchMaxGroups;

    if (!str_match_search(pattern, text + pos, text_len - pos, group_begin, group_end,
                          max_groups)) {
        pos = text_len + 1;  // exhausted: a further call returns false immediately
        return false;
    }

    const uint32_t mbeg = pos + static_cast<uint32_t>(group_begin[0]);
    const uint32_t mend = pos + static_cast<uint32_t>(group_end[0]);

    // Relative to absolute, for every participating group.
    for (uint32_t i = 0; i <= max_groups; ++i) {
        if (group_begin[i] != StrMatchNoGroup) {
            group_begin[i] += static_cast<int32_t>(pos);
            group_end[i] += static_cast<int32_t>(pos);
        }
    }

    // THE ADVANCE, and the whole reason this function exists: an empty match leaves the position
    // where it was, so stepping past it by one character is what terminates the loop.
    pos = (mend == mbeg) ? mbeg + 1 : mend;
    return true;
}

/**
 * True when `pattern` matches the ENTIRE subject, not just a part of it.
 * Convenience over str_match_search for the anchored case; the tree uses it wherever a
 * pattern is written with both ^ and $.
 */
constexpr bool str_match_full(const char* pattern, const char* text, uint32_t text_len) {
    int32_t gb[StrMatchMaxGroups + 1] = {};
    int32_t ge[StrMatchMaxGroups + 1] = {};
    if (!str_match_search(pattern, text, text_len, gb, ge, StrMatchMaxGroups)) return false;
    return gb[0] == 0 && ge[0] == static_cast<int32_t>(text_len);
}

/**
 * Replace EVERY match of `pattern` in `text` with `replacement`, and return the result.
 *
 * `replacement` may refer to capture groups as $1 … $6, and to the whole match as $0. A
 * literal dollar sign is written as $$. A reference to a group the pattern does not have, or
 * one that did not participate in this match, expands to nothing — the same behaviour the
 * existing call sites already rely on.
 *
 * WHY THIS RETURNS A STRING INSTEAD OF FILLING A BUFFER. The first version wrote into a
 * caller-supplied char[] with a size limit, matching strops.hpp. That was the wrong shape,
 * and the measurement says so: of the 270 regex_replace call sites in ase-codegen, 269 read
 *
 *     x = std::regex_replace(x, some_regex, "…");
 *
 * — a std::string assigned back to itself. The single exception passes the result straight
 * into another call. A buffer form would have forced 269 call sites to invent a size for a
 * system body of unknown length, and a size guessed 269 times is 269 chances to guess low.
 * ase-utils already returns std::string where the shape calls for it: fs.hpp does it in
 * parent_of, filename_of and relative_to.
 *
 * Replacing every occurrence rather than the first is deliberate too: std::regex_replace
 * defaults to global replacement, and all 270 measured call sites use that default.
 */
// NOT constexpr, and that is a compiler fact rather than a choice: std::string is not a literal
// type here (clang 22 with libstdc++ 16.1.1 rejects it as a constexpr return type), so the
// replacement cases below are checked by doctest at runtime while the matcher itself and
// str_match_count stay constexpr and are proved by static_assert while translating.
inline std::string str_match_replace(const char* pattern, const std::string& text,
                                     const char* replacement) {
    std::string out;
    if (pattern == nullptr || replacement == nullptr) return text;

    const uint32_t text_len = static_cast<uint32_t>(text.size());
    const char* raw = text.data();
    uint32_t pos = 0;

    while (pos <= text_len) {
        int32_t gb[StrMatchMaxGroups + 1] = {};
        int32_t ge[StrMatchMaxGroups + 1] = {};
        if (!str_match_search(pattern, raw + pos, text_len - pos, gb, ge, StrMatchMaxGroups)) {
            break;
        }

        const uint32_t mbeg = pos + static_cast<uint32_t>(gb[0]);
        const uint32_t mend = pos + static_cast<uint32_t>(ge[0]);

        // Everything between the previous match and this one is carried over unchanged.
        for (uint32_t i = pos; i < mbeg; ++i) out.push_back(raw[i]);

        // Expand the replacement, resolving $N against this match's groups.
        for (uint32_t r = 0; replacement[r] != '\0'; ++r) {
            if (replacement[r] != '$') {
                out.push_back(replacement[r]);
                continue;
            }
            const char nxt = replacement[r + 1];
            if (nxt == '$') {
                out.push_back('$');
                ++r;
                continue;
            }
            if (nxt < '0' || nxt > '9') {
                out.push_back('$');  // a dollar that names nothing stays a dollar
                continue;
            }
            const uint32_t slot = static_cast<uint32_t>(nxt - '0');
            ++r;
            if (slot > StrMatchMaxGroups) continue;
            if (gb[slot] == StrMatchNoGroup) continue;  // group did not participate
            const uint32_t gbeg = pos + static_cast<uint32_t>(gb[slot]);
            const uint32_t gend = pos + static_cast<uint32_t>(ge[slot]);
            for (uint32_t i = gbeg; i < gend; ++i) out.push_back(raw[i]);
        }

        // A zero-width match would spin here forever: step one character and carry it over.
        if (mend == mbeg) {
            if (mbeg < text_len) out.push_back(raw[mbeg]);
            pos = mbeg + 1;
        } else {
            pos = mend;
        }
    }

    // Whatever follows the last match.
    for (uint32_t i = pos; i < text_len; ++i) out.push_back(raw[i]);
    return out;
}

/**
 * How many times `pattern` matches in `text`. Counts the same occurrences the replacement
 * above would rewrite, including the empty match a pattern like a* finds at every position.
 *
 * Takes pointer and length rather than a std::string on purpose: that keeps it constexpr, so
 * the counting cases are settled while translating. The replacement cannot follow, because its
 * return type is what stops it.
 */
constexpr uint32_t str_match_count(const char* pattern, const char* raw, uint32_t text_len) {
    if (pattern == nullptr || raw == nullptr) return 0;
    uint32_t pos = 0;
    uint32_t count = 0;
    while (pos <= text_len) {
        int32_t gb[StrMatchMaxGroups + 1] = {};
        int32_t ge[StrMatchMaxGroups + 1] = {};
        if (!str_match_search(pattern, raw + pos, text_len - pos, gb, ge, StrMatchMaxGroups)) {
            break;
        }
        const uint32_t mbeg = pos + static_cast<uint32_t>(gb[0]);
        const uint32_t mend = pos + static_cast<uint32_t>(ge[0]);
        ++count;
        pos = (mend == mbeg) ? mbeg + 1 : mend;
    }
    return count;
}

}  // namespace ase::utils
