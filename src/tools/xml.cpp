/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "xml.h"

#include <string.h>

static const int RESIZE_BUFFER = 16;

static void outIndent(std::ostream & s, size_t cnt)
{
  while (cnt > 0)
  {
    s << " ";
    cnt--;
  }
}

static void outString(std::ostream & stream, const std::string & text)
{
  for (unsigned int i = 0; i < text.length(); i++)
    switch (text[i])
    {
      case '>' : stream << "&gt;";   break;
      case '<' : stream << "&lt;";   break;
      case '"' : stream << "&quot;"; break;
      case '&' : stream << "&amp;";  break;
      case '\'': stream << "&apos;"; break;
      default  : stream << text[i];  break;
    }
}

xmlWriter_c::xmlWriter_c(std::ostream & str) : stream(str), state(StBase)
{
  stream << "<?xml version=\"1.0\"?>\n";
}

xmlWriter_c::~xmlWriter_c(void)
{
  if (tagStack.size() > 0)
    throw xmlWriterException_c("Not all tags were closed");
}

void xmlWriter_c::newTag(const std::string & name)
{
  switch (state)
  {
    case StInTag:
      // when we are inside a tag, we need to finish that one first and go to next line
      stream << ">\n";
      break;
    case StBase:
      // in Base State we don't need to do anything
      break;
    case StInContent:
      throw xmlWriterException_c("Try to add a new node into a node with content...");
      break;
  }

  outIndent(stream, tagStack.size());
  tagStack.push_back(name);
  stream << "<" << name;
  state = StInTag;
}

void xmlWriter_c::newAttrib(const std::string & attrib, const std::string & value)
{
  switch (state)
  {
    case StInTag:
      stream << " " << attrib << "=\"";
      outString(stream, value);
      stream << "\"";
      break;
    case StBase:
    case StInContent:
      throw xmlWriterException_c("Try to add attribute but not in node");
      break;
  }
}

void xmlWriter_c::newAttrib(const std::string & attrib, unsigned long value)
{
  switch (state)
  {
    case StInTag:
      stream << " " << attrib << "=\"" << value << "\"";
      break;
    case StBase:
    case StInContent:
      throw xmlWriterException_c("Try to add attribute but not in node");
      break;
  }
}

void xmlWriter_c::newAttrib(const std::string & attrib, signed long value)
{
  switch (state)
  {
    case StInTag:
      stream << " " << attrib << "=\"" << value << "\"";
      break;
    case StBase:
    case StInContent:
      throw xmlWriterException_c("Try to add attribute but not in node");
      break;
  }
}

void xmlWriter_c::endTag(const std::string & name)
{
  if (name != *(tagStack.rbegin()))
    throw xmlWriterException_c("Try to close tag with wrong name");

  switch (state)
  {
    case StInTag:
      stream << "/>\n";
      break;
    case StBase:
      outIndent(stream, tagStack.size()-1);
      stream << "</" << name << ">\n";
      break;
    case StInContent:
      stream << "</" << name << ">\n";
      break;
  }

  tagStack.pop_back();
  state = StBase;
}

void xmlWriter_c::addContent(const std::string & text)
{
  switch (state)
  {
    case StInTag:
      stream << ">";
      break;
    case StBase:
      throw xmlWriterException_c("Try to add content to a node, where it is not allowed");
      break;
    case StInContent:
      break;
  }

  state = StInContent;

  outString(stream, text);
}

void xmlWriter_c::addContent(unsigned long val)
{
  switch (state)
  {
    case StInTag:
      stream << ">";
      break;
    case StBase:
      throw xmlWriterException_c("Try to add content to a node, where it is not allowed");
      break;
    case StInContent:
      break;
  }

  state = StInContent;
  stream << val;
}

void xmlWriter_c::addContent(signed long val)
{
  switch (state)
  {
    case StInTag:
      stream << ">";
      break;
    case StBase:
      throw xmlWriterException_c("Try to add content to a node, where it is not allowed");
      break;
    case StInContent:
      break;
  }

  state = StInContent;
  stream << val;
}

std::ostream & xmlWriter_c::addContent(void)
{
  switch (state)
  {
    case StInTag:
      stream << ">";
      break;
    case StBase:
      throw xmlWriterException_c("Try to add content to a node, where it is not allowed");
      break;
    case StInContent:
      break;
  }

  state = StInContent;
  return stream;
}


/////////////////////////
//////// PARSER /////////
/////////////////////////

/* Copyright (c) 2005,2007 Vivek Krishna
 *  Based on kxml2 by Stefan Haustein, Oberhausen, Rhld., Germany
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The  above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE. */

static int parseInt(std::string s, int radix)
{
  int len = s.size ();
  int value = 0;
  if (s.empty ())
    return -1;
  for (int i = 0; i < len; i++) {
    if (radix == 10) {
      if (s[i] <= '9' && s[i] >= '0')
        value = (i == 0) ? (s[i] - '0') : radix * value + (s[i] - '0');

      else
        return value;//return what is parsed till then
    }
    else if (radix == 16) {
      //assumes that the encoding format has arranges the alphabets and numbers in increasing order
      if (s[i] <= '9' && s[i] >= 0)
        value = (i == 0) ? (s[i] - '0') : radix * value + (s[i] - '0');

      else if (s[i] <= 'F' && s[i] >= 'A')
        value =
          (i ==
           0) ? (s[i] - 'A') + 10 : radix * value + (s[i] - 'A') + 10;

      else if (s[i] <= 'f' && s[i] >= 'a')
        value =(i ==0) ? (s[i] - 'a') + 10 : radix * value + (s[i] - 'a') + 10;
    }
  }
  return value;
}



xmlParser_c::xmlParser_c(std::istream & is) :
  unexpected_eof ("Unexpected EOF"),
  illegal_type ("wrong Event Type"),
  nspStack (16),
  elementStack (16),
  attributes (16),
  reader (is)
{
  initBuf ();
  commonInit ();
}


xmlParser_c::xmlParser_c(void) :
  unexpected_eof ("Unexpected EOF"),
  illegal_type ("wrong Event Type"),
  nspStack (16),
  elementStack (16),
  attributes (16),
  reader (std::cin)
{
  initBuf ();
  commonInit ();
}


void xmlParser_c::initBuf(void)
{
  srcBuf = new char[8192];
  srcBuflength = 8192;
  txtBuf = new char[256];
  txtBufSize = 256;
  nspCounts = new int[8];
  nspSize = 8;
}


//does common initializations
void xmlParser_c::commonInit(void)
{
  line = 1;
  column = 0;
  type = START_DOCUMENT;
  name = "";
  Ns = "";
  degenerated = false;
  attributeCount = -1;
  encoding = "";
  version = "";
  standalone = false;
  unresolved = false;
  LEGACY = 999;
  XML_DECL = 998;
  srcPos = 0;
  srcCount = 0;
  peekCount = 0;
  depth = 0;
  relaxed = false;
  skipNextTag=false;
  entityMap["apos"] = "'";
  entityMap["gt"] = ">";
  entityMap["lt"] = "<";
  entityMap["quot"] = "\"";
  entityMap["amp"] = "&";
  for (int i = 0; i < nspSize; i++)
    nspCounts[i] = 0;
}


xmlParser_c::~xmlParser_c(void)
{
  delete [] srcBuf;
  delete [] txtBuf;
  delete [] nspCounts;
}


std::string xmlParser_c::state(int eventType)
{
  switch (eventType)
  {
    case START_DOCUMENT:         return "START_DOCUMENT";
    case END_DOCUMENT:           return "END_DOCUMENT";
    case START_TAG:              return "START_TAG";
    case END_TAG:                return "END_TAG";
    case TEXT:                   return "TEXT";
    case CDSECT:                 return "CDSECT";
    case ENTITY_REF:             return "ENTITY_REF";
    case IGNORABLE_WHITESPACE:   return "IGNORABLE_WHITESPACE";
    case PROCESSING_INSTRUCTION: return "PROCESSING_INSTRUCTION";
    case COMMENT:                return "COMMENT";
    case DOCDECL:                return "DOCDECL";
    default:                     return "Illegal state";
  }
}

void xmlParser_c::exception(std::string desc)
{
  xmlParserException_c e (desc, state (type), line, column);
  throw e;
}


/**
 * common base for next and nextToken. Clears the state, except from
 * txtPos and whitespace. Does not set the type variable
 */
void xmlParser_c::nextImpl(void)
{
  if (type == END_TAG)
    depth--;
  while (true)
  {
    attributeCount = -1;
    if (degenerated)

    {
      degenerated = false;
      type = END_TAG;
      return;
    }
    prefix = "";
    name = "";
    Ns = "";
    text = "";
    type = peekType ();
    switch (type)
    {
      case ENTITY_REF:
        pushEntity ();
        return;
      case START_TAG:
        parseStartTag (false);
        return;
      case END_TAG:
        parseEndTag ();
        return;
      case END_DOCUMENT:
        return;
      case TEXT:
        pushText ('<', !token);
        if (depth == 0)
        {
          if (isWspace)
            type = IGNORABLE_WHITESPACE;
        }
        return;
      default:
        type = parseLegacy (token);
        if (type != XML_DECL)
          return;
    }
  }
}

int xmlParser_c::parseLegacy (bool bpush)
{
  std::string req = "";
  int term;
  int result;
  int prev = 0;
  read ();                                      // <
  int c = read ();
  if (c == '?')
  {
    if ((peekbuf (0) == 'x' || peekbuf (0) == 'X')
        && (peekbuf (1) == 'm' || peekbuf (1) == 'M'))
    {
      if (bpush)
      {
        push (peekbuf (0));
        push (peekbuf (1));
      }
      read();
      read();

      if ((peekbuf (0) == 'l' || peekbuf (0) == 'L')
          && peekbuf (1) <= ' ')
      {
        if (line != 1 || column > 4)
          exception ("PI must not start with xml");
        parseStartTag (true);
        if (attributeCount < 1 || "version" != attributes[2])
          exception ("version expected");
        version = attributes[3];
        int pos = 1;
        if (pos < attributeCount && "encoding" == attributes[2 + 4])
        {
          encoding = attributes[3 + 4];
          pos++;
        }

        if (pos < attributeCount
            && "standalone" == attributes[4 * pos + 2])
        {
          std::string st = attributes[3 + 4 * pos];
          if ("yes" == st)
            standalone = true;

          else if ("no" == st)
            standalone = false;

          else
            exception ("illegal standalone value: " + st);
          pos++;
        }

        if (pos != attributeCount)
          exception ("illegal xmldecl");
        isWspace = true;
        txtPos = 0;
        return XML_DECL;
      }
    }
    term = '?';
    result = PROCESSING_INSTRUCTION;
  }
  else if (c == '!')
  {
    if (peekbuf (0) == '-')
    {
      result = COMMENT;
      req = "--";
      term = '-';
    }
    else if (peekbuf (0) == '[')
    {
      result = CDSECT;
      req = "[CDATA[";
      term = ']';
      bpush = true;
    }
    else
    {
      result = DOCDECL;
      req = "DOCTYPE";
      term = -1;
    }
  }
  else
  {
    exception ("illegal: <" + c);
    return -1;
  }

  for (unsigned int i = 0; i < req.length (); i++)
    read (req.at (i));
  if (result == DOCDECL)
    parseDoctype (bpush);
  else
  {
    while (true)
    {
      c = read();
      if (c == -1)
        exception (unexpected_eof);
      if (bpush)
        push(c);
      if ((term == '?' || c == term)
          && peekbuf (0) == term && peekbuf (1) == '>')
        break;
      prev = c;
    }

    if (term == '-' && prev == '-' && !relaxed)
      exception ("illegal comment delimiter: --->");
    read();
    read();
    if (bpush && term != '?')
      txtPos--;
  }
  return result;
}


/** precondition: &lt! consumed */
void xmlParser_c::parseDoctype(bool bpush)
{
  int nesting = 1;
  bool quoted = false;

  // read();
  while (true)
  {
    int i = read ();
    switch (i)
    {
      case -1:
        exception (unexpected_eof);
      case '\'':
        quoted = !quoted;
        break;
      case '<':
        if (!quoted)
          nesting++;
        break;
      case '>':
        if (!quoted)
        {
          if ((--nesting) == 0)
            return;
        }
        break;
    }
    if (bpush)
      push (i);
  }
}


/* precondition: &lt;/ consumed */
void xmlParser_c::parseEndTag(void)
{
  read();                                      // '<'
  read();                                      // '/'
  name = readName();
  skip();
  read('>');
  int sp = (depth - 1) << 2;
  if (!relaxed)
  {
    if (depth == 0)
      exception ("element stack empty");
    if (name != elementStack[sp + 3])
      exception ("expected: " + elementStack[sp + 3]);
  }
  else if (depth == 0 || name != elementStack[sp + 3])
    return;
  Ns = elementStack[sp];
  prefix = elementStack[sp + 1];
  name = elementStack[sp + 2];
}


int xmlParser_c::peekType(void)
{
  switch (peekbuf (0))
  {
    case -1:
      return END_DOCUMENT;
    case '&':
      return ENTITY_REF;
    case '<':
      switch (peekbuf (1))
      {
        case '/':
          return END_TAG;
        case '?':
        case '!':
          return LEGACY;
        default:
          return START_TAG;
      }
    default:
      return TEXT;
  }
}


std::string xmlParser_c::get(int pos)
{
  std::string
    tmp (txtBuf);
  return tmp.substr (pos, txtPos - pos);
}


void xmlParser_c::push(int c)
{
  isWspace &= c <= ' ';
  if (txtPos >= txtBufSize - 1)
  {
    char *bigger = new char[txtBufSize = txtPos * 4 / 3 + 4];
    memcpy (bigger, txtBuf, txtPos);
    delete[] txtBuf;
    txtBuf = bigger;
  }
  txtBuf[txtPos++] = (char) c;
  txtBuf[txtPos] = 0;
}


/** Sets name and attributes */
void xmlParser_c::parseStartTag(bool xmldecl)
{
  if (!xmldecl)
    read();
  name = readName ();
  attributeCount = 0;

  while (true)
  {
    skip ();
    int c = peekbuf (0);
    if (xmldecl)
    {
      if (c == '?')
      {
        read ();
        read ('>');
        return;
      }
    }
    else
    {
      if (c == '/')
      {
        degenerated = true;
        read ();
        skip ();
        read ('>');
        break;
      }
      if (c == '>' && !xmldecl)
      {
        read ();
        break;
      }
    }
    if (c == -1)
      exception (unexpected_eof);
    std::string attrName = readName ();
    if (attrName.empty ())
      exception ("attr name expected");
    skip ();
    read ('=');
    skip ();
    int delimiter = read ();
    if (delimiter != '\'' && delimiter != '"')
    {
      if (!relaxed)
        exception ("<"
            + name + ">: invalid delimiter: " + (char) delimiter);
      delimiter = ' ';
    }
    unsigned int i = (attributeCount++) << 2;

    //attributes = ensureCapacity(attributes, i + 4);
    if (attributes.size () <= i + 4)
      attributes.resize (i + 4 + RESIZE_BUFFER);
    attributes[i++] = "";
    attributes[i++] = "";
    attributes[i++] = attrName;
    int p = txtPos;
    pushText (delimiter, true);
    attributes[i] = get (p);
    txtPos = p;
    if (delimiter != ' ')
      read ();                              // skip endquote
  }
  unsigned  int sp = depth++ << 2;

  //elementStack = ensureCapacity(elementStack, sp + 4,elementStackSize);
  if (elementStack.size () <= sp + 4)
    elementStack.resize (sp + 4 + RESIZE_BUFFER);
  elementStack[sp + 3] = name;

  /*    vivek ,avoided the increment array logic..fix later*/
  if (depth >= nspSize)
  {
    int *bigger = new int[nspSize + 4];
    int i = 0;
    for (i = 0; i < nspSize; i++)
      bigger[i] = nspCounts[i];
    for (i = nspSize; i < nspSize + 4; i++)
      bigger[i] = 0;
    delete [] nspCounts;
    nspCounts = bigger;
    nspSize += 4;
  }
  nspCounts[depth] = nspCounts[depth - 1];
  for (int i = attributeCount - 1; i > 0; i--)
  {
    for (int j = 0; j < i; j++)
    {
      if (getAttributeName (i) == getAttributeName (j))
        exception ("Duplicate Attribute: " + getAttributeName (i));
    }
  }
  elementStack[sp] = Ns;
  elementStack[sp + 1] = prefix;
  elementStack[sp + 2] = name;
}


/** result: isWspace; if the setName parameter is set,
the name of the entity is stored in "name" */
void xmlParser_c::pushEntity(void)
{
  read();                                      // &
  int pos = txtPos;
  while (true)
  {
    int c = read ();
    if (c == ';')
      break;
    if (relaxed && (c == '<' || c == '&' || c <= ' '))

    {
      if (c != -1)
        push (c);
      return;
    }
    if (c == -1)
      exception (unexpected_eof);
    push (c);
  }
  std::string code = get (pos);
  txtPos = pos;
  if (token && type == ENTITY_REF)
    name = code;
  if (code[0] == '#')
  {
    int c = (code[1] == 'x' ? parseInt (code.substr(2), 16)
        : parseInt (code.substr(1), 10));
    push (c);
    return;
  }
  std::string result = (std::string) entityMap[code];
  unresolved = result == "";
  if (unresolved)
  {
    if (!token)
      exception ("unresolved: &" + code + ";");
  }
  else
  {
    for (unsigned int i = 0; i < result.length (); i++)
      push (result.at (i));
  }
}


/** types:
'<': parse to any token (for nextToken ())
'"': parse to quote
' ': parse to whitespace or '>'
*/
void xmlParser_c::pushText(int delimiter, bool resolveEntities)
{
  int next = peekbuf (0);
  while (next != -1 && next != delimiter)       // covers eof, '<', '"'
  {
    if (delimiter == ' ')
      if (next <= ' ' || next == '>')
        break;
    if (next == '&')
    {
      if (!resolveEntities)
        break;
      pushEntity ();
    }
    else if (next == '\n' && type == START_TAG)
    {
      read ();
      push (' ');
    }
    else
      push (read ());
    next = peekbuf (0);
  }
}


void xmlParser_c::read(char c)
{
  int a = read();
  std::string sa (1, (char) a), sc (1, c);
  if (a != c)
    exception ("expected: '" + sc + "' actual: '" + sa + "'");
}


int xmlParser_c::read(void)
{
  int result;
  if (peekCount == 0)
    result = peekbuf (0);
  else
  {
    result = peek[0];
    peek[0] = peek[1];
  }
  peekCount--;
  column++;
  if (result == '\n')
  {
    line++;
    column = 1;
  }
  return result;
}


/** Does never read more than needed */
int xmlParser_c::peekbuf(int pos)
{
  while (pos >= peekCount)
  {
    int nw;
    if (srcBuflength <= 1)
      nw = reader.get ();
    else if (srcPos < srcCount)
      nw = srcBuf[srcPos++];
    else
    {
      srcCount = reader.read (srcBuf, srcBuflength).gcount ();
      if (srcCount <= 0)
        nw = -1;

      else
        nw = srcBuf[0];
      srcPos = 1;
    }
    if (nw == '\r')
    {
      wasCR = true;
      peek[peekCount++] = '\n';
    }
    else
    {
      if (nw == '\n')
      {
        if (!wasCR)
          peek[peekCount++] = '\n';
      }
      else
        peek[peekCount++] = nw;
      wasCR = false;
    }
  }
  return peek[pos];
}


std::string xmlParser_c::readName(void)
{
  int  pos = txtPos;
  int  c = peekbuf (0);
  if ((c < 'a' || c > 'z')
      && (c < 'A' || c > 'Z') && c != '_' && c != ':' && c < 0x0c0)
    exception ("name expected");

  do
  {
    push (read ());
    c = peekbuf (0);
  } while ((c >= 'a' && c <= 'z')
      || (c >= 'A' && c <= 'Z')
      || (c >= '0' && c <= '9')
      || c == '_' || c == '-' || c == ':' || c == '.' || c >= 0x0b7);

  std::string result = get(pos);
  txtPos = pos;
  return result;
}


void xmlParser_c::skip(void)
{
  while (true)
  {
    int c = peekbuf (0);
    if (c > ' ' || c == -1)
      break;
    read ();
  }
}


//--------------- public part starts here... ---------------


std::string xmlParser_c::getInputEncoding(void)
{
  return encoding;
}


void xmlParser_c::defineEntityReplacementText(std::string entity, std::string value)
{
  if (entityMap.empty())
    exception ("entity replacement text must be defined after setInput!");
  entityMap[entity] = value;
}


int xmlParser_c::getDepth(void)
{
  return depth;
}

bool xmlParser_c::isWhitespace(void)
{
  if (type != TEXT && type != IGNORABLE_WHITESPACE && type != CDSECT)
    exception (illegal_type);
  return isWspace;
}


std::string xmlParser_c::getText(void)
{
  return type < TEXT || (type == ENTITY_REF && unresolved) ? "" : get (0);
}


const char * xmlParser_c::getTextCharacters(int *poslen)
{
  if (type >= TEXT)
  {
    if (type == ENTITY_REF)
    {
      poslen[0] = 0;
      poslen[1] = name.length ();
      return name.c_str ();                 //return name.toCharArray();
    }
    poslen[0] = 0;
    poslen[1] = txtPos;
    return txtBuf;
  }
  poslen[0] = -1;
  poslen[1] = -1;
  return 0;
}


bool xmlParser_c::isEmptyElementTag(void)
{
  if (type != START_TAG)
    exception (illegal_type);
  return degenerated;
}


std::string xmlParser_c::getAttributeName(int index)
{
  if (index >= attributeCount)
    exception ("IndexOutOfBoundsException()");
  return attributes[(index << 2) + 2];
}


std::string xmlParser_c::getAttributePrefix(int index)
{
  if (index >= attributeCount)
    exception ("IndexOutOfBoundsException()");
  return attributes[(index << 2) + 1];
}


std::string xmlParser_c::getAttributeValue(int index)
{
  if (index >= attributeCount)
    exception ("IndexOutOfBoundsException()");
  return attributes[(index << 2) + 3];
}


std::string xmlParser_c::getAttributeValue(std::string nam)
{
  for (int i = (attributeCount << 2) - 4; i >= 0; i -= 4)
  {
    if (attributes[i + 2] == nam)
      return attributes[i + 3];
  }
  return "";
}


int xmlParser_c::next(void)
{
  txtPos = 0;
  isWspace = true;
  int minType = 9999;
  token = false;

  do
  {
    nextImpl();
    if (type < minType)
      minType = type;
  } while (minType > CDSECT                       // ignorable
      || (minType >= TEXT && peekType () >= TEXT));

  type = minType;

  if (type > TEXT)
    type = TEXT;

  return type;
}


int xmlParser_c::nextToken(void)
{
  isWspace = true;
  txtPos = 0;
  token = true;
  nextImpl ();
  return type;
}

void xmlParser_c::prevTag(void)
{
  skipNextTag=true;
}

//----------------------------------------------------------------------
// utility methods to make XML parsing easier ...
int xmlParser_c::nextTag(void)
{
  if(skipNextTag)
  {
    skipNextTag = false;
    return type;
  }
  next();

  if (type == TEXT && isWspace)
    next();

  if (type != END_TAG && type != START_TAG && type != END_DOCUMENT)
    exception ("unexpected type");

  return type;
}


void xmlParser_c::require (int Type, std::string nam)
{
  if (Type != type || (!nam.empty () && nam != getName ()))
    exception ("expected: " + state(Type) + nam);
}

std::string xmlParser_c::nextText(void)
{
  if (type != START_TAG)
    exception ("precondition: START_TAG");

  next();
  std::string result;

  if (type == TEXT)
  {
    result = getText();
    next();
  }
  else
    result = "";

  if (type != END_TAG)
    exception ("END_TAG expected");

  return result;
}


/**
 * Skip sub tree that is currently parser positioned on.
 * <br>NOTE: parser must be on START_TAG and when funtion returns
 * parser will be positioned on corresponding END_TAG.
 */
void xmlParser_c::skipSubTree()
{
  require(START_TAG, "");
  int level = 1;
  while (level > 0)
  {
    int eventType = next();
    if (eventType == END_TAG)
    {
      --level;
    }
    else if (eventType == START_TAG)
    {
      ++level;
    }
  }
}



