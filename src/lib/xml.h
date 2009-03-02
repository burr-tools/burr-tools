/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#ifndef __XML_H__
#define __XML_H__

#include <vector>
#include <string>
#include <iostream>

#include <map>

/** /file
 * This file contains the XML load and save stuff.
 *
 * The load functions are base on xmlpat and the save stuff is self written
 * I used to use xml2 with xmlwrapp, but that got way to bulky and slow
 */

class xmlException {

  public:

    xmlException(const std::string & /*txt*/) {}

};

class xmlWriter_c
{
  private:

    /** all data is written to this stream
     */
    std::ostream & stream;

    std::vector<std::string> tagStack;

    enum {
      StBase,
      StInTag,
      StInContent,
    } state;

  public:

    /** open an XML writer, wirte header */
    xmlWriter_c(std::ostream & str);

    ~xmlWriter_c(void);

    void newTag(const std::string & name);

    /**
     * add an attribute to the current tag
     */
    void newAttrib(const std::string & attrib, const std::string & value);
    void newAttrib(const std::string & attrib, unsigned long value);
    void newAttrib(const std::string & attrib, signed long value);
    void newAttrib(const std::string & attrib, unsigned int value) { newAttrib(attrib, (unsigned long)value); }
    void newAttrib(const std::string & attrib, signed int value)  { newAttrib(attrib, (signed long)value); }

    /**
     * ends the current tag.
     * when no content has been written to this tag, the short form is used
     * otherwise a normal and tag is written
     */
    void endTag(const std::string & name);

    /** adds text to the content of the current tag */
    void addContent(const std::string & text);
    void addContent(unsigned long val);
    void addContent(signed long val);
    void addContent(unsigned int val) { addContent((unsigned long)val); }
    void addContent(signed int val) { addContent((signed long)val); }
    std::ostream & addContent(void);
};

// this parser is taken from wsdlpull.sf.net, but heavily modified

class xmlParserException_c
{
  public:

    xmlParserException_c(std::string desc, std::string STATE, int l, int c) : state (STATE), line(l), col(c)
    {
      description = "Xml Parser Exception : " ;
      description += desc;
    }

    xmlParserException_c(std::string desc) : state("unknown"), line(0), col(0)
    {
      description = "Xml Parser Exception : " ;
      description += desc;
    }

    ~xmlParserException_c() throw() {};

    std::string description, state;

    int line, col;
};

/** A simple, pull based XML parser.  */
class xmlParser_c
{
  public:

    xmlParser_c(std::istream & is);
    xmlParser_c(void);

    ~xmlParser_c(void);

    std::string getInputEncoding(void);

    void defineEntityReplacementText(std::string entity, std::string value);

    int getDepth(void);

    std::string getPositionDescription(void);

    bool isWhitespace(void);

    std::string getText(void);

    const char *getTextCharacters(int *poslen);

    std::string getName(void)
    {
      return name;
    }

    std::string getPrefix(void)
    {
      return prefix;
    }

    bool isEmptyElementTag(void);

    int getAttributeCount(void)
    {
      return attributeCount;
    }

    std::string getAttributeName(int index);
    std::string getAttributePrefix(int index);
    std::string getAttributeValue(int index);
    std::string getAttributeValue(std::string name);

    int getEventType(void)
    {
      return type;
    }

    //parsing methods
    int next(void);
    int nextToken(void);
    int nextTag(void);
    void prevTag(void);

    //----------------------------------------------------------------------
    // utility methods to make XML parsing easier ...
    void require(int type, std::string name);
    std::string nextText(void);
    void skipSubTree(void);

    //enum for event types
    enum
    {
      START_DOCUMENT,
      END_DOCUMENT,
      START_TAG,
      END_TAG,
      TEXT,
      CDSECT,
      ENTITY_REF,
      IGNORABLE_WHITESPACE,
      PROCESSING_INSTRUCTION,
      COMMENT,
      DOCDECL,
      UNKNOWN
    };
    // throws XmlPullParserException with the current line and col
    void exception (std::string desc);

  private:

    void commonInit (void);
    void initBuf (void);

    //returns the state name as  a std::string
    std::string state (int eventType);
    std::string *ensureCapacity (std::string * arr, int required);

    /**
     * common base for next and nextToken. Clears the state, except from
     * txtPos and whitespace. Does not set the type variable */
    void nextImpl ();                             //throws IOException, XmlPullParserException
    int parseLegacy (bool push);

    //throws IOException, XmlPullParserException

    /** precondition: &lt! consumed */
    void parseDoctype (bool push);

    /* precondition: &lt;/ consumed */
    void parseEndTag ();
    int peekType ();
    std::string get (int pos);
    void push (int c);

    /** Sets name and attributes */
    void parseStartTag (bool xmldecl);

    /** result: isWspace; if the setName parameter is set,
      the name of the entity is stored in "name" */

    //vivek
    void pushEntity ();

    /** types:
      '<': parse to any token (for nextToken ())
      '"': parse to quote
      ' ': parse to whitespace or '>'
      */
    void pushText (int delimiter, bool resolveEntities);
    void read (char c);
    int read ();

    /** Does never read more than needed */
    int peekbuf (int pos);
    std::string readName ();
    void skip ();
    std::string unexpected_eof;
    std::string illegal_type;
    int LEGACY;
    int XML_DECL;

    // general
    std::string version;
    bool standalone;

    //   private bool reportNspAttr;
    bool processNsp;
    bool relaxed;
    std::map < std::string, std::string > entityMap;
    int depth;
    std::vector < std::string > nspStack;
    std::vector < std::string > elementStack;
    int *nspCounts;
    int nspSize;


    std::string encoding;
    char *srcBuf;
    int srcPos;
    int srcCount;
    int srcBuflength;

    //    private bool eof;
    int line;
    int column;

    // txtbuffer
    char *txtBuf;
    int txtPos;
    int txtBufSize;

    // Event-related
    int type;
    std::string text;
    bool isWspace,skipNextTag;
    std::string Ns;
    std::string prefix;
    std::string name;
    bool degenerated;
    int attributeCount;
    std::vector < std::string > attributes;
    // source
    std::istream & reader;

    /**
     * A separate peek buffer seems simpler than managing
     * wrap around in the first level read buffer */
    int peek[2];
    int peekCount;
    bool wasCR;
    bool unresolved;
    bool token;
};


#endif
