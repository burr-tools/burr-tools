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
  /* xml.cpp:131-132 throws xmlWriterException_c("Try to close tag with
     wrong name") when the name passed to endTag() does not match the top
     of the tag stack, and does so *before* popping that entry.

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
     other text alongside it, e.g. "<t>&mine;</t>") does not surface here
     as TEXT -- see the (BUG) case below for why, and for the data-loss
     consequence that has in production. Mixing the entity with an
     ordinary character sidesteps that and still exercises
     defineEntityReplacementText on its own terms. */
  std::istringstream in("<t>x&mine;</t>");
  xmlParser_c pars(in);
  pars.defineEntityReplacementText("mine", "replaced");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.next() == xmlParser_c::TEXT);
  REQUIRE(pars.getText() == "xreplaced");
}

TEST_CASE("xml parser: an entity that is an element's sole content silently discards it (BUG)", "[xml][parser]") {
  /* MECHANISM: next() (xml.cpp) merges "ignorable" events (entity refs,
     comments, whitespace) with whatever follows, taking the minimum
     event-type code across the run via `while (minType > CDSECT ...)`.
     When an entity reference is followed immediately by the closing tag
     and nothing else, that minimum comes out as END_TAG rather than TEXT,
     even though the resolved entity text was buffered internally.
     getText() then reports "" because it checks (type < TEXT).

     CONSEQUENCE: this is not a cosmetic quirk, it is silent data loss.
     xmlWriter_c::addContent escapes '<' and '&', so a puzzle comment that
     is made up entirely of XML-special characters -- addContent("<&")
     writes exactly <comment>&lt;&amp;</comment> -- round-trips through
     the escaper as an entity-only element body. puzzle_c::load()
     (puzzle.cpp:329-336) only assigns `comment` when
     `state == xmlParser_c::TEXT`; when next() instead reports END_TAG for
     that body, the branch is skipped and the comment is silently dropped
     on reload, with no error raised anywhere.

     FIX: xml.cpp:1087's loop condition, `minType > CDSECT // ignorable`,
     should be `minType > ENTITY_REF // ignorable`, matching upstream
     kXML2 -- the comment was carried over from the original verbatim but
     the constant it names was not. Applying exactly that one-token change
     was verified to fix this: <t>&lt;&amp;</t> then yields
     next() == TEXT and getText() == "<&". Left unapplied here per this
     PR's test-only scope; production code is unchanged. */
  std::istringstream in("<t>&mine;</t>");
  xmlParser_c pars(in);
  pars.defineEntityReplacementText("mine", "replaced");

  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  REQUIRE(pars.next() == xmlParser_c::END_TAG);
  REQUIRE(pars.getText() == "");
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
  /* Traced through pushText() (xml.cpp): with no closing '"' anywhere in
     the remaining input, the attribute scan swallows everything up to and
     including the trailing "></a>" as if it were all still the attribute's
     text, then tries to consume one more character for the (nonexistent)
     closing quote and finds EOF. The exception this throws is therefore
     "Unexpected EOF" raised while still inside the <a> start tag, not a
     targeted "attribute never closed" diagnostic -- but it does still
     throw.
     This rejection is INCIDENTAL, not principled: BurrTools throws here
     only because the input stream ran out while the scan for the closing
     '"' was still hunting. The same class of input -- an attribute value
     containing an unescaped delimiter-like byte -- parses cleanly with no
     exception at all once the stream keeps going past that point instead
     of ending; see "an unquoted '<' inside an attribute value is silently
     accepted" below for a case where that happens and swallows a whole
     child element. This case therefore pins "this particular malformed
     document is rejected", not "unterminated attributes are rejected" in
     general. */
  std::istringstream in("<a name=\"unterminated></a>");
  xmlParser_c pars(in);

  REQUIRE_THROWS_AS(([&]{
    while (pars.next() != xmlParser_c::END_DOCUMENT) { }
  }()), xmlParserException_c);
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

TEST_CASE("xml parser: an unquoted '<' inside an attribute value is silently accepted", "[xml][parser][malformed]") {
  /* SUSPECTED DEFECT, recorded as observed behaviour -- not asserted to be
     correct. This one has earned the label: unlike the case above, this
     input is NOT well-formed XML.
     XML 1.0 3.1's AttValue production is:
       AttValue ::= '"' ([^<&"] | Reference)* '"'
     A literal '<' inside a double-quoted attribute value is explicitly
     excluded ([^<&"]) -- this is a well-formedness constraint, so a
     conforming parser MUST reject it. pushText() (src/tools/xml.cpp:844)
     scans for the closing delimiter with:
       while (next != -1 && next != delimiter)
     and has no check for '<' in that loop at all, so BurrTools happily
     accepts a literal '<' inside an attribute value as ordinary text.
     Measured against this repository's own parser (not assumed):
       input  <a name="x<y"/>
       output START <a> empty=1 attrs=1 [name=x<y]; END </a>; clean
              END_DOCUMENT, no exception
     Escalating the same gap shows real data loss, not just leniency: if
     the swallowed text itself contains a full child element's markup, it
     is absorbed into the attribute value as inert text instead of being
     parsed as structure --
       input  <a name="v><b/>" z="2"/>
       output START <a> empty=1 attrs=2 [name=v><b/>] [z=2]; END </a>;
              clean END_DOCUMENT, no exception
     The <b/> element is gone -- not rejected, not visible as a child
     element, just bytes inside "name"'s value. For a `.xmpuzzle` file,
     the same mechanism means a stray '<' introduced by truncation or
     corruption inside an attribute value would not be rejected; it could
     silently absorb following markup -- up to and including whole
     elements -- into an attribute string instead of raising a parse
     error. Production code (xml.cpp:844) is intentionally left unchanged;
     this case only pins the observed behaviour. */
  {
    std::istringstream in("<a name=\"x<y\"/>");
    xmlParser_c pars(in);

    REQUIRE(pars.next() == xmlParser_c::START_TAG);
    REQUIRE(pars.getName() == "a");
    REQUIRE(pars.isEmptyElementTag());
    REQUIRE(pars.getAttributeCount() == 1);
    REQUIRE(pars.getAttributeValue("name") == "x<y");

    REQUIRE_NOTHROW(([&]{
      while (pars.next() != xmlParser_c::END_DOCUMENT) { }
    }()));
  }
  {
    /* the escalation: an entire child element's markup absorbed whole */
    std::istringstream in("<a name=\"v><b/>\" z=\"2\"/>");
    xmlParser_c pars(in);

    REQUIRE(pars.next() == xmlParser_c::START_TAG);
    REQUIRE(pars.getName() == "a");
    REQUIRE(pars.isEmptyElementTag());
    REQUIRE(pars.getAttributeCount() == 2);
    REQUIRE(pars.getAttributeValue("name") == "v><b/>");
    REQUIRE(pars.getAttributeValue("z") == "2");

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
       than merely a non-empty string: xml.cpp:630 raises
       "expected: " + elementStack[...] for a mismatched close tag, which
       here names the still-open "a" */
    REQUIRE_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring("expected: a"));
  }
}
