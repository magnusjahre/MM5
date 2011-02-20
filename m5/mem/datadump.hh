/*
 * datadump.hh
 *
 *  Created on: Feb 20, 2011
 *      Author: jahre
 */

#ifndef DATADUMP_HH_
#define DATADUMP_HH_

#include <string>
#include <map>

#include "requesttrace.hh"

class DataDump{

private:
	const char* filename;
	std::map<std::string, RequestTraceEntry> data;

public:
	DataDump(const char* filename);

	void addElement(std::string key, RequestTraceEntry value);

	void dump();

	static std::string buildKey(std::string name, int id);
};

#endif /* DATADUMP_HH_ */
