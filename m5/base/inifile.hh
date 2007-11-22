/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

#ifndef __INIFILE_HH__
#define __INIFILE_HH__

#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "base/hashmap.hh"

/**
 * @file
 * Declaration of IniFile object.
 * @todo Change comments to match documentation style.
 */

///
/// This class represents the contents of a ".ini" file.
///
/// It's basically a two level lookup table: a set of named sections,
/// where each section is a set of key/value pairs.  Section names,
/// keys, and values are all uninterpreted strings.
///
class IniFile
{
  protected:

    ///
    /// A single key/value pair.
    ///
    class Entry
    {
	std::string	value;		///< The entry value.
	mutable bool	referenced;	///< Has this entry been used?

      public:
	/// Constructor.
	Entry(const std::string &v)
	    : value(v), referenced(false)
	{
	}

	/// Has this entry been used?
	bool isReferenced() { return referenced; }

	/// Fetch the value.
	const std::string &getValue() const;

	/// Set the value.
	void setValue(const std::string &v) { value = v; }

	/// Append the given string to the value.  A space is inserted
	/// between the existing value and the new value.  Since this
	/// operation is typically used with values that are
	/// space-separated lists of tokens, this keeps the tokens
	/// separate.
	void appendValue(const std::string &v) { value += " "; value += v; }
    };

    ///
    /// A section.
    ///
    class Section
    {
	/// EntryTable type.  Map of strings to Entry object pointers.
	typedef m5::hash_map<std::string, Entry *> EntryTable;

	EntryTable	table;		///< Table of entries.
	mutable bool	referenced;	///< Has this section been used?

      public:
	/// Constructor.
	Section()
	    : table(), referenced(false)
	{
	}

	/// Has this section been used?
	bool isReferenced() { return referenced; }

	/// Add an entry to the table.  If an entry with the same name
	/// already exists, the 'append' parameter is checked If true,
	/// the new value will be appended to the existing entry.  If
	/// false, the new value will replace the existing entry.
	void addEntry(const std::string &entryName, const std::string &value,
		      bool append);

	/// Add an entry to the table given a string assigment.
	/// Assignment should be of the form "param=value" or
	/// "param+=value" (for append).  This funciton parses the
	/// assignment statment and calls addEntry().
	/// @retval True for success, false if parse error.
	bool add(const std::string &assignment);

	/// Find the entry with the given name.
	/// @retval Pointer to the entry object, or NULL if none.
	Entry *findEntry(const std::string &entryName) const;

	/// Print the unreferenced entries in this section to cerr.
	/// Messages can be suppressed using "unref_section_ok" and
	/// "unref_entries_ok".
	/// @param sectionName Name of this section, for use in output message.
	/// @retval True if any entries were printed.
	bool printUnreferenced(const std::string &sectionName);

	/// Print the contents of this section to cout (for debugging).
	void dump(const std::string &sectionName);
    };

    /// SectionTable type.  Map of strings to Section object pointers.
    typedef m5::hash_map<std::string, Section *> SectionTable;

  protected:
    /// Hash of section names to Section object pointers.
    SectionTable table;

    /// Look up section with the given name, creating a new section if
    /// not found.
    /// @retval Pointer to section object.
    Section *addSection(const std::string &sectionName);

    /// Look up section with the given name.
    /// @retval Pointer to section object, or NULL if not found.
    Section *findSection(const std::string &sectionName) const;

  public:
    /// Constructor.
    IniFile();

    /// Destructor.
    ~IniFile();

    /// Load parameter settings from given istream.  This is a helper
    /// function for load(string) and loadCPP(), which open a file
    /// and then pass it here.
    /// @retval True if successful, false if errors were encountered.
    bool load(std::istream &f);

    /// Load the specified file, passing it through the C preprocessor.
    /// Parameter settings found in the file will be merged with any
    /// already defined in this object.
    /// @param file The path of the file to load.
    /// @param cppFlags Vector of extra flags to pass to cpp.
    /// @retval True if successful, false if errors were encountered.
    bool loadCPP(const std::string &file, std::vector<char *> &cppFlags);

    /// Load the specified file.
    /// Parameter settings found in the file will be merged with any
    /// already defined in this object.
    /// @param file The path of the file to load.
    /// @retval True if successful, false if errors were encountered.
    bool load(const std::string &file);

    /// Take string of the form "<section>:<parameter>=<value>" or
    /// "<section>:<parameter>+=<value>" and add to database.
    /// @retval True if successful, false if parse error.
    bool add(const std::string &s);

    /// Find value corresponding to given section and entry names.
    /// Value is returned by reference in 'value' param.
    /// @retval True if found, false if not.
    bool find(const std::string &section, const std::string &entry,
	      std::string &value) const;

    /// Determine whether the named section exists in the .ini file.
    /// Note that the 'Section' class is (intentionally) not public,
    /// so all clients can do is get a bool that says whether there
    /// are any values in that section or not.
    /// @return True if the section exists.
    bool sectionExists(const std::string &section) const;

    /// Print unreferenced entries in object.  Iteratively calls
    /// printUnreferend() on all the constituent sections.
    bool printUnreferenced();

    /// Dump contents to cout.  For debugging.
    void dump();
};

#endif // __INIFILE_HH__
