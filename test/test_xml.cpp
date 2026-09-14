#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "tools/xml.h"

#include <sstream>
#include <string>

namespace {

/* run a writer over a scratch stream and hand back what it produced */
std::string written(void (*build)(xmlWriter_c &)) {
  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    build(xml);
  }
  return out.str();
}

bool contains(const std::string & haystack, const std::string & needle) {
  return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("xml writer: an empty tag uses the short form", "[xml][writer]") {
  std::string doc = written([](xmlWriter_c & xml) {
    xml.newTag("puzzle");
    xml.endTag("puzzle");
  });

  /* no content was written, so the writer should emit the self-closing form
     rather than an open/close pair */
  REQUIRE(contains(doc, "<puzzle"));
  REQUIRE_FALSE(contains(doc, "</puzzle>"));
}

TEST_CASE("xml writer: a tag with content uses the long form", "[xml][writer]") {
  std::string doc = written([](xmlWriter_c & xml) {
    xml.newTag("comment");
    xml.addContent(std::string("hello"));
    xml.endTag("comment");
  });

  REQUIRE(contains(doc, "<comment>"));
  REQUIRE(contains(doc, "hello"));
  REQUIRE(contains(doc, "</comment>"));
}

TEST_CASE("xml writer: tags nest in the order they were opened", "[xml][writer]") {
  std::string doc = written([](xmlWriter_c & xml) {
    xml.newTag("outer");
    xml.newTag("inner");
    xml.addContent(std::string("x"));
    xml.endTag("inner");
    xml.endTag("outer");
  });

  const size_t outerOpen = doc.find("<outer");
  const size_t innerOpen = doc.find("<inner");
  const size_t innerClose = doc.find("</inner>");
  const size_t outerClose = doc.find("</outer>");

  REQUIRE(outerOpen != std::string::npos);
  REQUIRE(innerOpen != std::string::npos);
  REQUIRE(innerClose != std::string::npos);
  REQUIRE(outerClose != std::string::npos);

  /* strictly nested, not interleaved */
  REQUIRE(outerOpen < innerOpen);
  REQUIRE(innerOpen < innerClose);
  REQUIRE(innerClose < outerClose);
}

TEST_CASE("xml writer: attributes carry their values", "[xml][writer]") {
  std::string doc = written([](xmlWriter_c & xml) {
    xml.newTag("shape");
    xml.newAttrib("name", std::string("piece one"));
    xml.newAttrib("count", (unsigned int)7);
    xml.newAttrib("offset", (signed int)-3);
    xml.endTag("shape");
  });

  REQUIRE(contains(doc, "name=\"piece one\""));
  REQUIRE(contains(doc, "count=\"7\""));
  REQUIRE(contains(doc, "offset=\"-3\""));
}

TEST_CASE("xml writer: all five special characters are escaped in content", "[xml][writer][escape]") {
  /* src/tools/xml.cpp:43-47 escapes exactly these five and nothing else */
  std::string doc = written([](xmlWriter_c & xml) {
    xml.newTag("t");
    xml.addContent(std::string("<&>\"'"));
    xml.endTag("t");
  });

  REQUIRE(contains(doc, "&lt;"));
  REQUIRE(contains(doc, "&amp;"));
  REQUIRE(contains(doc, "&gt;"));
  REQUIRE(contains(doc, "&quot;"));
  REQUIRE(contains(doc, "&apos;"));

  /* the exact content between <t> and </t> must be the escaped run and
     nothing else -- this is the assertion that actually does the work
     above only checks that each entity appears *somewhere* in the whole
     document, which a writer that escaped unrelated content elsewhere
     could also satisfy */
  const size_t body = doc.find("<t>");
  REQUIRE(body != std::string::npos);
  const size_t close = doc.find("</t>", body);
  REQUIRE(close != std::string::npos);
  const std::string tail = doc.substr(body + 3, close - (body + 3));
  REQUIRE(tail == "&lt;&amp;&gt;&quot;&apos;");
}

TEST_CASE("xml writer: special characters are escaped in attribute values too", "[xml][writer][escape]") {
  std::string doc = written([](xmlWriter_c & xml) {
    xml.newTag("t");
    xml.newAttrib("v", std::string("a<b&c\"d"));
    xml.endTag("t");
  });

  REQUIRE(contains(doc, "&lt;"));
  REQUIRE(contains(doc, "&amp;"));
  REQUIRE(contains(doc, "&quot;"));

  /* an unescaped quote would terminate the attribute early and corrupt the
     document, so this is the one that really matters */
  REQUIRE_FALSE(contains(doc, "v=\"a<b&c\"d\""));
}

TEST_CASE("xml writer: leaving a tag open throws on destruction", "[xml][writer]") {
  /* ~xmlWriter_c is noexcept(false) and throws when the tag stack is not
     empty. The lambda gives the destructor a well-defined run point with no
     other exception in flight -- throwing during unwinding would terminate
     the process rather than fail this case. */
  REQUIRE_THROWS_AS(([]{
    std::ostringstream out;
    xmlWriter_c xml(out);
    xml.newTag("never closed");
  }()), xmlWriterException_c);
}

TEST_CASE("xml writer: closing a tag with the wrong name throws", "[xml][writer]") {
  /* the wrong-name throw in endTag() raises xmlWriterException_c("Try to
     close tag with wrong name") when the name passed to endTag() does not
     match the top of the tag stack, and does so *before* popping that
     entry.

     ~xmlWriter_c is noexcept(false) and itself throws if the tag stack is
     still non-empty when it runs (see the case above). If the mismatched
     endTag's exception were simply left to propagate, `xml` would be
     destroyed during that unwind with "outer" still on the stack, the
     destructor would throw a second exception while the first is still in
     flight, and the process would std::terminate instead of failing this
     assertion. So the mismatched call is caught here, "outer" is closed
     properly to empty the tag stack, and only then is the original
     exception rethrown -- giving the destructor a well-defined, single-
     exception run point, the same discipline the lambda above uses. */
  REQUIRE_THROWS_AS(([]{
    std::ostringstream out;
    xmlWriter_c xml(out);
    xml.newTag("outer");
    try {
      xml.endTag("wrong");
    }
    catch (...) {
      xml.endTag("outer");
      throw;
    }
  }()), xmlWriterException_c);
}

TEST_CASE("xml writer: closing a tag when none is open throws", "[xml][writer]") {
  /* xml.cpp's endTag() used to do `*(tagStack.rbegin())` with no check that
     tagStack was non-empty -- calling endTag() with no open tag dereferenced
     the reverse-iterator of an empty vector, which is undefined behaviour,
     not a clean error. It now checks tagStack.empty() first and throws
     xmlWriterException_c, matching the style of the mismatched-name throw
     immediately below it.

     Because tagStack is empty for the whole test -- not a single tag was
     ever opened -- ~xmlWriter_c has nothing to complain about: its own
     throw is conditioned on `tagStack.size() > 0` (see the constructor
     above), so it stays quiet here and only endTag's exception is ever in
     flight. That is true independent of *how* xml is destroyed, so unlike
     the two cases above this one needs no lambda to give the destructor a
     safe, single-exception run point -- but the lambda is used anyway, for
     the same reason and the same discipline: it keeps that fact obvious
     from the shape of the test rather than leaving it to be worked out from
     the implementation. */
  REQUIRE_THROWS_AS(([]{
    std::ostringstream out;
    xmlWriter_c xml(out);
    xml.endTag("puzzle");
  }()), xmlWriterException_c);
}

TEST_CASE("xml parser: walks a document tag by tag", "[xml][parser]") {
  std::istringstream in("<a><b>text</b></a>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "a");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "b");
}

TEST_CASE("xml parser: reads attribute values by name", "[xml][parser]") {
  std::istringstream in("<shape name=\"piece one\" count=\"7\"/>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "shape");
  REQUIRE(pars.getAttributeValue("name") == "piece one");
  REQUIRE(pars.getAttributeValue("count") == "7");
}

TEST_CASE("xml parser: an absent attribute is not reported as present", "[xml][parser]") {
  std::istringstream in("<shape name=\"x\"/>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getAttributeValue("name") == "x");

  /* an absent attribute reads back as an empty string, not the value of a
     different attribute and not some sentinel */
  REQUIRE(pars.getAttributeValue("missing") == "");
}

TEST_CASE("xml parser: entities in content are resolved", "[xml][parser]") {
  std::istringstream in("<t>a&lt;b&amp;c&gt;d&quot;e&apos;f</t>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "t");

  REQUIRE(pars.next() == xmlParser_c::TEXT);

  /* the five standard entities must come back as their characters */
  const std::string text = pars.getText();
  REQUIRE(text == "a<b&c>d\"e'f");
}

TEST_CASE("xml parser: an empty element tag is recognised as such", "[xml][parser]") {
  std::istringstream in("<a><short/></a>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "a");
  /* a regular open tag must not also read as empty-element, otherwise an
     unconditional `return true` would pass this case unnoticed */
  REQUIRE_FALSE(pars.isEmptyElementTag());

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "short");
  REQUIRE(pars.isEmptyElementTag());
}

TEST_CASE("xml parser: require accepts the expected tag and rejects others", "[xml][parser]") {
  {
    std::istringstream in("<puzzle/>");
    xmlParser_c pars(in);
    pars.nextTag();
    /* the matching case must not throw */
    REQUIRE_NOTHROW(pars.require(xmlParser_c::START_TAG, "puzzle"));
  }
  {
    std::istringstream in("<puzzle/>");
    xmlParser_c pars(in);
    pars.nextTag();
    /* the wrong name must be rejected */
    REQUIRE_THROWS_AS(pars.require(xmlParser_c::START_TAG, "problem"), xmlParserException_c);
  }
}

TEST_CASE("xml parser: skipSubTree steps over a whole nested element", "[xml][parser]") {
  std::istringstream in("<root><skipme><deep><deeper/></deep></skipme><after/></root>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "root");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "skipme");

  pars.skipSubTree();

  /* everything inside skipme is gone; the next tag is its sibling */
  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "after");
}

TEST_CASE("xml parser: a custom entity replacement is applied", "[xml][parser]") {
  /* An entity reference that is the *entire* content of an element (no
     other text alongside it, e.g. "<t>&mine;</t>") is exercised on its
     own below. Mixing the entity with an ordinary character here sidesteps
     that case and still exercises defineEntityReplacementText on its own
     terms. */
  std::istringstream in("<t>x&mine;</t>");
  xmlParser_c pars(in);
  pars.defineEntityReplacementText("mine", "replaced");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.next() == xmlParser_c::TEXT);
  REQUIRE(pars.getText() == "xreplaced");
}

TEST_CASE("xml parser: an entity that is an element's sole content resolves as text", "[xml][parser]") {
  /* MECHANISM: next() (xml.cpp) merges "ignorable" events (entity refs,
     comments, whitespace) with whatever follows, taking the minimum
     event-type code across the run via `while (minType > ENTITY_REF ...)`.
     When an entity reference is followed immediately by the closing tag
     and nothing else, that minimum now comes out as TEXT, and the
     resolved entity text buffered internally is surfaced by getText().

     This matters beyond the parser: xmlWriter_c::addContent escapes '<'
     and '&', so a puzzle comment made up entirely of XML-special
     characters -- addContent("<&") writes exactly
     <comment>&lt;&amp;</comment> -- round-trips through the escaper as
     an entity-only element body. puzzle_c::load() (puzzle.cpp:329-336)
     only assigns `comment` when `state == xmlParser_c::TEXT`, so before
     this fix such a comment was silently dropped on reload with no error
     raised anywhere. See test_roundtrip.cpp for the end-to-end case.

     FIX: xml.cpp's loop condition is `minType > ENTITY_REF // ignorable`,
     matching upstream kXML2 -- the comment was carried over from the
     original verbatim but the constant it named was not (it had drifted
     to CDSECT). */
  std::istringstream in("<t>&mine;</t>");
  xmlParser_c pars(in);
  pars.defineEntityReplacementText("mine", "replaced");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.next() == xmlParser_c::TEXT);
  REQUIRE(pars.getText() == "replaced");
}

TEST_CASE("xml parser: prevTag makes the following nextTag re-read the current tag", "[xml][parser]") {
  /* prevTag()'s contract, established by reading xml.cpp: it sets a
     one-shot flag (skipNextTag) that makes the *next* nextTag() call
     return the parser's already-current (type, name) again without
     consuming any input, instead of advancing. It is a one-tag replay,
     not a real rewind -- normal advancement resumes exactly where it had
     actually left off once the replay is consumed. */
  std::istringstream in("<a><b></b><c></c></a>");
  xmlParser_c pars(in);

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "a");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "b");

  pars.prevTag();

  /* the very next nextTag() replays the same start tag, unchanged */
  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "b");

  /* after the replay, advancement resumes normally: "b"'s own close tag
     is still pending and comes next, then "c" */
  REQUIRE(pars.nextTag() == xmlParser_c::END_TAG);
  REQUIRE(pars.getName() == "b");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "c");
}

/* xmlParser_c has a `relaxed` member (src/tools/xml.h) that, if true, would
   let several of the cases below through instead of throwing: it guards the
   mismatched-end-tag check, the empty-element-stack check, the invalid
   attribute-delimiter check and part of entity-reference parsing (xml.cpp).
   Read together with commonInit() (xml.cpp), which sets `relaxed = false`
   and is never followed anywhere in this codebase by code that sets it
   true, this means the "relaxed" mode is unreachable dead code as the
   parser is actually wired up here. `relaxed` is also a private member
   with no public setter (xml.h), so no consumer of this class could turn
   it on even if it wanted to -- it is not merely unset, it is unsettable.
   The first four cases below throw, matching the strict (non-relaxed)
   behaviour; the last two do not throw at all, for reasons unrelated to
   `relaxed` -- see their own comments. */

TEST_CASE("xml parser: mismatched closing tag is rejected", "[xml][parser][malformed]") {
  std::istringstream in("<a></b>");
  xmlParser_c pars(in);

  REQUIRE_THROWS_AS(([&]{
    while (pars.next() != xmlParser_c::END_DOCUMENT) { }
  }()), xmlParserException_c);
}

TEST_CASE("xml parser: an unclosed tag is rejected", "[xml][parser][malformed]") {
  std::istringstream in("<a><b></a>");
  xmlParser_c pars(in);

  REQUIRE_THROWS_AS(([&]{
    while (pars.next() != xmlParser_c::END_DOCUMENT) { }
  }()), xmlParserException_c);
}

TEST_CASE("xml parser: a truncated document is rejected", "[xml][parser][malformed]") {
  std::istringstream in("<a><b");
  xmlParser_c pars(in);

  REQUIRE_THROWS_AS(([&]{
    while (pars.next() != xmlParser_c::END_DOCUMENT) { }
  }()), xmlParserException_c);
}

TEST_CASE("xml parser: an unterminated attribute value is rejected", "[xml][parser][malformed]") {
  /* With no closing '"' anywhere in the remaining input, pushText()
     (xml.cpp) scans into the trailing "></a>" while still hunting for the
     attribute's closing quote. That text contains a literal '<' (from the
     "</a>" close tag), which is itself a well-formedness violation inside
     an attribute value (XML 1.0 3.1: AttValue ::= '"' ([^<&"] | Reference)*
     '"') -- see "a literal '<' inside an attribute value is rejected"
     below. So this case is now rejected for a targeted reason: the
     embedded '<', not merely running out of input. */
  {
    std::istringstream in("<a name=\"unterminated></a>");
    xmlParser_c pars(in);

    try {
      pars.next();
      FAIL("expected the parser to reject the embedded '<'");
    }
    catch (const xmlParserException_c & e) {
      REQUIRE_THAT(std::string(e.what()),
          Catch::Matchers::ContainsSubstring("illegal character '<' in attribute value"));
    }
  }
  {
    /* the embedded '<' guard now catches the case above before the scan
       ever runs out of input, so nothing in the suite exercised pushText's
       run-to-EOF path any more. Restore that coverage with an attribute
       value that has no closing quote AND no '<' anywhere in what remains
       of the stream, so the scan can only stop by hitting end of input. */
    std::istringstream in("<a name=\"unterminated");
    xmlParser_c pars(in);

    try {
      pars.next();
      FAIL("expected the parser to reject the unterminated attribute value");
    }
    catch (const xmlParserException_c & e) {
      REQUIRE_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring("EOF"));
    }
  }
}

TEST_CASE("xml parser: an unquoted '=' inside an attribute value is ordinary text", "[xml][parser][malformed]") {
  /* NOT a defect -- this is well-formed XML, and a conforming parser is
     required to parse it exactly as BurrTools does here.
     XML 1.0 3.1 defines:
       EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'
       Attribute    ::= Name Eq AttValue
       AttValue     ::= '"' ([^<&"] | Reference)* '"'
     Input: <a name="value1 name2="/>
     The value1 name2= run contains no '<', '&' or '"', so every character
     of it is a legal AttValue character; the '"' right before '/>' is
     *the* closing quote of "name"'s value, not an accidental resync point.
     There is exactly one attribute, "name", whose value is the literal
     text "value1 name2=". There is no "name2" attribute because the
     document never declares one -- "name2=" is just text that happens to
     look like the start of another attribute to a human reader. Cross-
     checked against expat, which parses this identically (one attribute,
     same value, clean end).
     The real, useful observation is the CONSEQUENCE, not a parser flaw:
     because AttValue syntax has no way to escape a bare '=' or space, a
     quote character lost anywhere earlier in a document (truncation,
     hand-editing, a bug elsewhere) can turn what was meant to be two
     attributes into one differently-meaning attribute, and XML's grammar
     gives no conforming parser -- BurrTools or otherwise -- any way to
     detect that from syntax alone. The document stays well-formed; only
     its meaning changed. */
  std::istringstream in("<a name=\"value1 name2=\"/>");
  xmlParser_c pars(in);

  REQUIRE(pars.next() == xmlParser_c::START_TAG);
  REQUIRE(pars.getName() == "a");
  REQUIRE(pars.isEmptyElementTag());
  REQUIRE(pars.getAttributeCount() == 1);
  REQUIRE(pars.getAttributeName(0) == "name");
  REQUIRE(pars.getAttributeValue(0) == "value1 name2=");
  /* no "name2" attribute was declared, so none is reported */
  REQUIRE(pars.getAttributeValue("name2") == "");

  REQUIRE_NOTHROW(([&]{
    while (pars.next() != xmlParser_c::END_DOCUMENT) { }
  }()));
}

TEST_CASE("xml parser: a literal '<' inside an attribute value is rejected", "[xml][parser][malformed]") {
  /* XML 1.0 3.1's AttValue production is:
       AttValue ::= '"' ([^<&"] | Reference)* '"'
     A literal '<' inside a double-quoted attribute value is explicitly
     excluded ([^<&"]) -- this is a well-formedness constraint, so a
     conforming parser MUST reject it. Measured against expat (via Python's
     xml.parsers.expat), both inputs below are rejected as not well-formed:
       input  <a name="x<y"/>                  expat: not well-formed (col 10)
       input  <a name="v><b/>" z="2"/>          expat: not well-formed (col 11)
     Before this fix, pushText() (src/tools/xml.cpp) scanned for the closing
     delimiter with `while (next != -1 && next != delimiter)` and had no
     check for '<' in that loop at all, so BurrTools accepted a literal '<'
     inside an attribute value as ordinary text. The second input shows why
     that was more than leniency: the swallowed text contained a whole
     child element's markup (<b/>), which was absorbed into the "name"
     attribute's value as inert text instead of being parsed as structure --
     the child element vanished rather than being rejected or reported. For
     a `.xmpuzzle` file, the same mechanism meant a stray '<' introduced by
     truncation or corruption inside an attribute value would not be
     rejected; it could silently absorb following markup -- up to and
     including whole elements -- into an attribute string instead of
     raising a parse error. pushText() now rejects a literal '<' whenever
     it is scanning an attribute value (delimiter is the quote character,
     not '<' itself, which is reserved for element content where '<'
     legitimately ends the run). */
  {
    std::istringstream in("<a name=\"x<y\"/>");
    xmlParser_c pars(in);

    try {
      pars.next();
      FAIL("expected the parser to reject the '<' in the attribute value");
    }
    catch (const xmlParserException_c & e) {
      REQUIRE_THAT(std::string(e.what()),
          Catch::Matchers::ContainsSubstring("illegal character '<' in attribute value"));
      /* expat (0-based, pointing AT the offending byte) reports column 10
         for this input; BurrTools' column counts characters already
         CONSUMED (xml.cpp: column++ happens inside read(), after the
         character is read), so it points at the character BEFORE the '<'
         -- these are different conventions that happen to land on the same
         number here, not a claim that BurrTools' positions match expat's
         in general. Pinned as a regression check on this specific input. */
      REQUIRE_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring("at position: 1; 10"));
    }
  }
  {
    /* the escalation: an entire child element's markup would otherwise be
       absorbed whole into the "name" attribute's value */
    std::istringstream in("<a name=\"v><b/>\" z=\"2\"/>");
    xmlParser_c pars(in);

    try {
      pars.next();
      FAIL("expected the parser to reject the '<' in the attribute value");
    }
    catch (const xmlParserException_c & e) {
      REQUIRE_THAT(std::string(e.what()),
          Catch::Matchers::ContainsSubstring("illegal character '<' in attribute value"));
      /* see the position comment on the case above -- expat reports
         column 11 for this input, and BurrTools' consumed-characters count
         lands on the same number by convention, not by cross-validation */
      REQUIRE_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring("at position: 1; 11"));
    }
  }
}

TEST_CASE("xml parser: an escaped '<' inside an attribute value still resolves", "[xml][parser]") {
  /* The guard added for the rejection case above sits one line above the
     '&' handling inside the same pushText() loop (xml.cpp), so it would be
     easy for a future edit to move the check earlier, or apply it to the
     entity-resolved buffer instead of the raw input stream, and start
     rejecting every attribute value that legitimately contains an escaped
     '<'. Nothing else in the suite pins that acceptance path: "xml parser:
     entities in content are resolved" covers entities in element CONTENT,
     "xml writer: all five special characters are escaped in content"
     covers only the WRITER's escaping, and test_roundtrip.cpp's "puzzle: a
     comment made up only of XML-special characters survives a save and
     reload" covers a special-character COMMENT -- none of those exercise
     entity resolution inside an ATTRIBUTE VALUE. Both
     the named form (&lt;) and the numeric form (&#60;) take the same
     pushText()/pushEntity() path, so both are pinned here; this documents
     existing correct behaviour and changes nothing. */
  {
    std::istringstream in("<a name=\"x&lt;y\"/>");
    xmlParser_c pars(in);

    REQUIRE(pars.next() == xmlParser_c::START_TAG);
    REQUIRE(pars.getAttributeValue("name") == "x<y");

    REQUIRE_NOTHROW(([&]{
      while (pars.next() != xmlParser_c::END_DOCUMENT) { }
    }()));
  }
  {
    std::istringstream in("<a name=\"x&#60;y\"/>");
    xmlParser_c pars(in);

    REQUIRE(pars.next() == xmlParser_c::START_TAG);
    REQUIRE(pars.getAttributeValue("name") == "x<y");

    REQUIRE_NOTHROW(([&]{
      while (pars.next() != xmlParser_c::END_DOCUMENT) { }
    }()));
  }
}

TEST_CASE("xml parser: the exception carries a description", "[xml][parser][malformed]") {
  std::istringstream in("<a></b>");
  xmlParser_c pars(in);

  try {
    while (pars.next() != xmlParser_c::END_DOCUMENT) { }
    FAIL("expected the parser to reject mismatched tags");
  }
  catch (const xmlParserException_c & e) {
    /* an empty message (or a single space) would make a real parse failure
       undiagnosable, so check for the actual diagnostic content rather
       than merely a non-empty string: xmlParser_c::parseEndTag() raises
       "expected: " + elementStack[...] for a mismatched close tag, which
       here names the still-open "a" */
    REQUIRE_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring("expected: a"));
  }
}
