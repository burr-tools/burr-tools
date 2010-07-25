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
#ifndef __XML_H__
#define __XML_H__

#include <vector>
#include <string>
#include <iostream>

#include <map>
#include <exception>

/** \file
 * This file contains the XML load and save stuff.
 *
 * The load functions are base on xmlpat and the save stuff is self written
 * I used to use xml2 with xmlwrapp, but that got way to bulky and slow
 */

/** this exception is thrown by xmlWriter_c, when it is used in a wrong way */
class xmlWriterException_c : public std::exception
{
  private:

    std::string text;

  public:

    xmlWriterException_c(const std::string & txt) : text(txt) {}
    ~xmlWriterException_c() throw() {}

    const char * what(void) const throw() { return text.c_str(); }

};

/** a simple class for xml file generation */
class xmlWriter_c
{
  private:

    /** all data is written to this stream
     */
    std::ostream & stream;

    /** the stack of currently open tags */
    std::vector<std::string> tagStack;

    /** the internal state of the writer */
    enum {
      StBase,        ///< right after start
      StInTag,       ///< inside an open tag
      StInContent,   ///< inside a text
    } state;

  public:

    /** open an XML writer, wirte header */
    xmlWriter_c(std::ostream & str);

    /** all tags must be closed by now, otherwise an exception is thrown */
    ~xmlWriter_c(void);

    /** start a new tag */
    void newTag(const std::string & name);

    /** \name add an attribute to the current tag.
     *
     * You may only call this function right after newTag, otherwise an exception is thrown.
     */
    //@{
    void newAttrib(const std::string & attrib, const std::string & value);
    void newAttrib(const std::string & attrib, unsigned long value);
    void newAttrib(const std::string & attrib, signed long value);
    void newAttrib(const std::string & attrib, unsigned int value) { newAttrib(attrib, (unsigned long)value); }
    void newAttrib(const std::string & attrib, signed int value)  { newAttrib(attrib, (signed long)value); }
    //@}

    /**
     * ends the current tag and pops it off the tag stack
     * when no content has been written to this tag, the short form is used
     * otherwise a normal and tag is written
     */
    void endTag(const std::string & name);

    /** \name adds text to the content of the current tag */
    //@{
    void addContent(const std::string & text);
    void addContent(unsigned long val);
    void addContent(signed long val);
    void addContent(unsigned int val) { addContent((unsigned long)val); }
    void addContent(signed int val) { addContent((signed long)val); }
    /** get an outstream to output content
     * You may use this stream to write out content if all the other
     * functions are too unhandy.
     * Be careful to not use xml special characters (like <, >, &, and so on) or
     * confert them properly.
     * Also don't use this stream once you called any other function from this class
     * You need to get it again using this function.
     */
    std::ostream & addContent(void);
    //@}
};

// this parser is taken from wsdlpull.sf.net, but heavily modified

/** this class is thriown by the xml parser */
class xmlParserException_c : public std::exception
{
  public:

    xmlParserException_c(std::string desc, std::string state, int line, int col);

    xmlParserException_c(std::string desc);

    ~xmlParserException_c() throw() {};

    const char * what(void) const throw() { return description.c_str(); }

  private:

    std::string description;
};

/** A simple, pull based XML parser.
 *
 * As this parser is taken from an other project I don't understand it completely
 * I only comment the functions that I used...
 *
 * */
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

    /** return the text of the current text node.
     * Only call when you are actually in a text node.
     */
    std::string getText(void);

    const char *getTextCharacters(int *poslen);

    /** get the name of the current tag */
    std::string getName(void)
    {
      return name;
    }

    std::string getPrefix(void)
    {
      return prefix;
    }

    bool isEmptyElementTag(void);

    /** get the number of attributes in the current tag */
    int getAttributeCount(void)
    {
      return attributeCount;
    }

    /** get the name of one of the attributes */
    std::string getAttributeName(int index);
    std::string getAttributePrefix(int index);
    /** get the value of one of the attributes */
    std::string getAttributeValue(int index);

    /** get the value of the given attribut.
     * If the attribut doesn't exist an empty string is returned
     */
    std::string getAttributeValue(std::string name);

    int getEventType(void)
    {
      return type;
    }

    /** go to the next xml event, and return the event type */
    int next(void);
    int nextToken(void);
    int nextTag(void);
    void prevTag(void);

    /** check, if the parser is in a certain state.
     *
     * type is one of the even type enum, name is used when a tag name
     * is required, e.g
     * \verbatim require(xmlParser_c::START_TAG, "puzzle") \endverbatim
     * to enforce that we are at an open puzzle tag
     */
    void require(int type, std::string name);

    std::string nextText(void);

    /** skip the complete tree until the close of the current tag.
     * The stream must be at the beginning of a tag, otherwise an exception
     * will be thrown
     */
    void skipSubTree(void);

    /** possible xml events */
    enum
    {
      START_DOCUMENT,
      END_DOCUMENT,
      START_TAG,               ///< an open tag (can be both tag types)
      END_TAG,                 ///< the end of a node (either closing tag, or short tag)
      TEXT,                    ///< a text node
      CDSECT,
      ENTITY_REF,
      IGNORABLE_WHITESPACE,
      PROCESSING_INSTRUCTION,
      COMMENT,
      DOCDECL,
      UNKNOWN
    };

    /** throws XmlPullParserException with the current line and col.
     * You may use this, if you have encounteted a faulty XML, e.g missing attribute or what not.
     */
    void exception(std::string desc);

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
