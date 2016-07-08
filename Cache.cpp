/**
* @file   cache.cpp
* @author Matthew Bradley, (C) 2016
* @date   07/06/2016
* @brief  Simple Cache Simulator
*
* @section DESCRIPTION
* Basic cache simulator, first argument is cache config file, second argument is memory trace
* using LRU replacement algorithm and Write-Back and Write-Allocate policy
*
* Cache file is associativity as an integer, line size in bytes, cache size
* in bytes, each on their own line.
*
* Trace file is <AccessType>:<RefSize>:<HexAddress>, AccessType is R or W,
* Ref size is size of reference in bytes, hex address is hex address of mem
* location, addresses are 32b.
* TODO:
* 
**********************************************************************/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <boost/tokenizer.hpp>

class Access;

class Cache;

class Cache
{
public:
	explicit Cache(std::ifstream&);
	virtual ~Cache();

	void operator () (std::ifstream&);

	// return index of oldest item in set, add age increases age of all valid items in set
	size_t getOldest(size_t);
	void addAge(size_t);
	bool Insert(size_t, Access&); // adds entry to first non-valid entry if set t is not full returns true if success, false otherwise
	bool isFull(size_t t) const; // whether set t is full, true if all entries are valid, false if 1 or more is not

	// used to calculate offset, index, and tag values for addresses
	size_t calcOffset(std::string);
	size_t calcIndex(std::string);
	size_t calcTag(std::string);
	
	size_t calcNumSets(size_t, size_t, size_t);

	// accessors for getting number of bits for tag, index, and offset fields
	size_t getIndexBits() const { return indexBits_; }
	size_t getTagBits() const { return tagBits_; }
	size_t getOffsetBits() const { return offsetBits_; }

	//returns true if HIT, false if MISS
	bool Read(Access&);
	bool Write(Access&);

	void Display();
	
private:
	size_t numBlocks_; // number of blocks, numSets_ * setSize_
	size_t numSets_;
	size_t lineSize_;
	size_t setSize_;
	size_t cacheSize_;
	size_t offsetBits_;
	size_t indexBits_;
	size_t tagBits_;
	size_t hits_;
	size_t accesses_;

	// contains cache data
	std::vector<std::vector<Access>> cache_;
	// memory trace commands in sequence
	std::vector<Access> trace_;

	std::vector<bool> hitMiss_; // tracks whether cache call was hit or miss

	// parseHex returns string as a binary string given hex string as input
	std::string parseHex(std::string); // private helper methods
	size_t binStringToInt(std::string);
}; // end class Cache

class Access
{
public:
	Access();
	explicit Access(std::string);
	virtual ~Access();

	// Access mutators	
	void setAddress(const std::string t); // expands given address to 32 bits (8 hex chars) as needed
	void setRefSize(const std::string t) { refSize_ = t; }
	void setOffset(const size_t t) { offset_ = t; }
	void setIndex(const size_t t) { index_ = t; }
	void setTag(const size_t t) { tag_ = t; }
	void addAge() { ++age_; }
	void setValid() { valid_ = 1; }
	void setDirty() { dirty_ = 1; }

	// Access accessors
	std::string getOp() { return op_; }
	std::string getRefSize() const { return refSize_; } 
	std::string getAddress() const { return address_; }
	size_t getOffset() const { return offset_; }
	size_t getIndex() const { return index_; }
	size_t getTag() const { return tag_; }
	size_t getAge() const { return age_; }

	bool Valid() const { return valid_; }
	bool Dirty() const { return dirty_; }

private:
	std::string op_;
	std::string refSize_;
	std::string address_;
	size_t age_;
	size_t offset_;
	size_t index_;
	size_t tag_;
	bool valid_;
	bool dirty_;

}; // end class Access

/**************************
 *  Cache Implementations
 **************************/

Cache::Cache(std::ifstream& ifs) : numSets_(0), lineSize_(0), setSize_(0), cacheSize_(0), hits_(0), accesses_(0), cache_(), trace_()
{
	ifs >> setSize_;
	ifs >> lineSize_;
	ifs >> cacheSize_;

	numSets_ = calcNumSets(cacheSize_, setSize_, lineSize_);
	numBlocks_ = numSets_ * setSize_;
	cache_.resize(numSets_);

	indexBits_ = (size_t)std::log2(cacheSize_);
	offsetBits_ = (size_t)std::log2(lineSize_);
	tagBits_ = 32 - indexBits_ + (size_t)std::log2(setSize_); // compensates for associativity by increasing number of bits for tag
	
	// resizes the cache sets to the appropriate size
	for (size_t i = 0; i < cache_.size(); ++i)
	{
		cache_[i].resize(setSize_);
	}

	ifs.close();
}

Cache::~Cache()
{}

void Cache::operator () (std::ifstream& ifs)
{
	boost::char_separator<char> delim(":");
	typedef boost::tokenizer< boost::char_separator< char > > tokenizer;

	std::vector<std::string> trace;

	do
	{
		std::string line;
		std::ws(ifs);
		std::getline(ifs, line);
		trace.push_back(line);
	} while (ifs.eof() == 0);

	for (size_t i = 0; i < trace.size(); ++i)
	{
		std::string s = trace[i];
		std::string op;

		tokenizer tokens(s, delim);
		tokenizer::iterator iter = tokens.begin();

		if (iter != tokens.end())
		{
			if (*iter == "R")
			{
				op = "Read";
			}
			else if (*iter == "W")
			{
				op = "Write";
			}
			Access a(op);
			
			++iter;
			
			a.setRefSize(*iter);
			
			++iter;
		
			a.setAddress(*iter);

			a.setIndex(calcIndex(a.getAddress()));
			a.setOffset(calcOffset(a.getAddress()));
			a.setTag(calcTag(a.getAddress()));


			trace_.push_back(a);
		}
	}

	for (size_t i = 0; i < trace_.size(); ++i)
	{
		std::string op = trace_[i].getOp();

		if (op == "Read")
		{
			if (Read(trace_[i]))
			{
				++hits_;
				++accesses_;
			}
			else // miss
			{
				++accesses_;
			}
		}
		else if (op == "Write")
		{
			if (Write(trace_[i]))
			{
				++hits_;
				++accesses_;
			}
			else // miss
			{
				++accesses_;
			}
		}
	}
	ifs.close();
}

size_t Cache::calcIndex(std::string s)
{
	std::string b = parseHex(s);

	size_t indexStart = (32 - offsetBits_) - indexBits_;
	
	std::string indexStr = b.substr(indexStart, indexBits_); 
	
	size_t blockNum = binStringToInt(indexStr); 

	return blockNum % numSets_;
}

size_t Cache::calcTag(std::string s)
{
	std::string b = parseHex(s);

	std::string tagStr = b.substr(0, tagBits_);
	
	return binStringToInt(tagStr);
}

size_t Cache::calcOffset(std::string s)
{
	//std::cout << "Calculating Offset!\n"; // debug
	std::string b = parseHex(s);
	size_t offsetStart = 32 - offsetBits_;
	std::string offsetStr = b.substr(offsetStart, offsetBits_);

	return binStringToInt(offsetStr);
}

// number of sets = cache size / set size * line size
size_t Cache::calcNumSets(size_t c, size_t s, size_t l)
{
	return (c / (s * l));
}

// converts binary string to size_t to use for indexes by iterating through string in reverse order and calculating powers of two
size_t Cache::binStringToInt(std::string s)
{
	int index = s.length() - 1; // index of max string element is length - 1

	size_t num = 0;

	index = s.length() - 1; // debug
	for (size_t i = 0; index >= 0; --index, ++i) // i used to determine power
	{
		if (s[index] == '1')
		{
			num += (size_t)pow(2, i);
		}
	}
	return num;
}

//converts HexString to binary string
std::string Cache::parseHex(const std::string s)
{
	std::string out;

	for (size_t i = 0; i < s.length(); ++i)
	{
		std::string test;

		test += s[i];

		if (test.compare("0") == 0)
		{
			out += "0000";
		}
		else if (test.compare("1") == 0)
		{
			out += "0001";
		}
		else if (test.compare("2") == 0)
		{
			out += "0010";
		}
		else if (test.compare("3") == 0)
		{
			out += "0011";
		}
		else if (test.compare("4") == 0)
		{
			out += "0100";
		}
		else if (test.compare("5") == 0)
		{
			out += "0101";
		}
		else if (test.compare("6") == 0)
		{
			out += "0110";
		}
		else if (test.compare("7") == 0)
		{
			out += "0111";
		}
		else if (test.compare("8") == 0)
		{
			out += "1000";
		}
		else if (test.compare("9") == 0)
		{
			out += "1001";
		}
		else if (test.compare("a") == 0)
		{
			out += "1010";
		}
		else if (test.compare("b") == 0)
		{
			out += "1011";
		}
		else if (test.compare("c") == 0)
		{
			out += "1100";
		}
		else if (test.compare("d") == 0)
		{
			out += "1101";
		}
		else if (test.compare("e") == 0)
		{
			out += "1110";
		}
		else if (test.compare("f") == 0)
		{
			out += "1111";
		}
	} // end for

	return out;
} // end ParseHex

size_t Cache::getOldest(size_t t)
{
	size_t age = 0;
	size_t oldest;

	for (size_t i = 0; i < cache_[t].size(); ++i)
	{
		if (cache_[t][i].Valid() && (cache_[t][i].getAge() > age))
		{
			age = cache_[t][i].getAge();
			oldest = i;
		}
	}
	
	return oldest;
}
void Cache::addAge(size_t t)
{
		for (size_t i = 0; i < cache_[t].size(); ++i)
		{
			if (cache_[t][i].Valid())
			{
				cache_[t][i].addAge();
			}
		}
}

bool Cache::isFull(size_t t) const
{
	for (size_t i = 0; i < cache_[t].size(); ++i)
	{
		if (!cache_[t][i].Valid())
		{
			return 0;
		}
	}

	return 1;
}

bool Cache::Insert(size_t t, Access& a)
{
	for (size_t i = 0; i < cache_[t].size(); ++i)
	{
		if (!cache_[t][i].Valid())
		{
			cache_[t][i] = a;
			return 1;
		}
	}
	return 0;
}

bool Cache::Read(Access& r)
{
	size_t t = r.getIndex();
	bool hit = 0;

	addAge(t); // make the elements in t older. 

	for (size_t i = 0; i < cache_[t].size(); ++i)
	{
		if (cache_[t][i].Valid() && (cache_[t][i].getTag() == r.getTag())) // hit
		{
			hit = 1;
			hitMiss_.push_back(1);
			return 1;
		}
	}

	if (!hit) // miss
	{
		r.setValid();
		hitMiss_.push_back(0);

		if (isFull(t)) // if block is full, replace oldest
		{
	     	size_t j = getOldest(t);
			cache_[t][j] = r;
		}
		else // else just add r in to the block
		{
			Insert(t, r);
		}
	}

	return 0;
}
bool Cache::Write(Access& w)
{

	size_t t = w.getIndex();
	bool hit = 0;

	addAge(t); // make the elements in t older. 

	for (size_t i = 0; i < cache_[t].size(); ++i)
	{
		if (cache_[t][i].Valid() && (cache_[t][i].getTag() == w.getTag())) // hit
		{
			cache_[t][i].setDirty();
			hit = 1;
			hitMiss_.push_back(1);
			return 1;
		}
	}

	if (!hit) // miss
	{
		w.setValid();
		hitMiss_.push_back(0);
		
		if (isFull(t)) // if block is full, replace oldest
		{
			size_t j = getOldest(t);
			cache_[t][j] = w;
		}
		else // else just add w in to the block
		{
			Insert(t, w);
		}
	}
	

	return 0;
}

void Cache::Display()
{
	std::cout << "Total Cache Size:  " << cacheSize_ << "B\n";
	std::cout << "Line Size:  " << lineSize_ << "B\n";
	std::cout << "Set Size:  " << setSize_ << '\n';
	std::cout << "Number of Sets:  " << numSets_ << "\n\n";
	std::cout << std::setw(8) << std::left << "RefNum";
	std::cout << std::setw(10) << std::left << "  R/W";
	std::cout << std::setw(13) << std::left << "Address";
	std::cout << std::setw(6) << std::left << "Tag";
	std::cout << std::setw(8) << std::left << "Index";
	std::cout << std::setw(10) << std::left << "Offset";
	std::cout << std::setw(8) << std::left << "H/M";
	std::cout << std::setfill('*') << std::setw(64) << "\n" << std::setfill(' ');
	std::cout << "\n";

	for (size_t counter = 0; counter < trace_.size(); ++counter)
	{
		std::cout << "   " << std::setw(5) << std::left << std::dec << counter;


		std::cout << std::setw(8) << trace_[counter].getOp();

		std::cout << "  " << trace_[counter].getAddress();
		std::cout << std::setfill(' ') << std::setw(7) << std::internal << std::hex << trace_[counter].getTag();
		std::cout << std::setfill(' ') << std::setw(8) << std::internal << trace_[counter].getIndex();
		std::cout << std::setfill(' ') << std::setw(8) << std::internal << std::dec << trace_[counter].getOffset();

		if (hitMiss_[counter])
			std::cout << std::setw(10) << "Hit";
		else
			std::cout << std::setw(10) << "Miss";
		 
		std::cout << "\n";
	}

	double hitRate = (double)hits_ / accesses_;
	double missRate = (double)(accesses_ - hits_) / accesses_;

	std::cout << "\n";
	std::cout << "    Simulation Summary\n";
	std::cout << "**************************\n";
	std::cout << "Total Hits:\t" << hits_ << "\n";
	std::cout << "Total Misses:\t" << (accesses_ - hits_) << "\n";
	std::cout << "Hit Rate:\t" << std::setprecision(5) << hitRate << "\n";
	std::cout << "Miss Rate:\t" << std::setprecision(5) << missRate << "\n";

}

/**************************************
* Access Implementations
***************************************/
Access::Access() : op_(""), refSize_(""), address_(""), age_(0), offset_(0), index_(0), tag_(0), valid_(0), dirty_(0)
{}
Access::Access(std::string s) : op_(s), refSize_(""), address_(""), age_(0), offset_(0), index_(0), tag_(0), valid_(1), dirty_(0)
{}
Access::~Access()
{}

// expands given address to 32 bits (8 hex chars) as needed
void Access::setAddress(const std::string t)
{
	size_t bits = t.length();
	bits = 8 - bits; // 8 because address is in hex.

	std::string s;

	for (size_t i = 0; i < bits; ++i)
	{
		s += "0";
	}

	s += t;

	address_ = s;
}

/****************
 *  Main
 *****************/
int main(int argc, char * argv[])
{
	std::ifstream ifs1, ifs2;

	if (argc != 3)
	{
		std::cerr << "Error: specify files: <cache config> <mem trace>\n";
		exit(1);
	}

	ifs1.open(argv[1]);
	ifs2.open(argv[2]);

	if (ifs1.fail() || ifs2.fail())
	{
		std::cout << "Error opening files!\n";
		exit(1);
	}

	Cache cache(ifs1);
	cache(ifs2);
	cache.Display();

	return 0;
}
