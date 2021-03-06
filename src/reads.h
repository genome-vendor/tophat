#ifndef READS_H
#define READS_H
/*
 *  reads.h
 *  TopHat
 *
 *  Created by Cole Trapnell on 9/2/08.
 *  Copyright 2008 Cole Trapnell. All rights reserved.
 *
 */

#include <string>
#include <sstream>
#include <seqan/sequence.h>
#include "common.h"
#include <queue>

using std::string;

static const int max_read_bp = 256;

// Note: qualities are not currently used by TopHat
struct Read
{
	Read() 
	{
		seq.reserve(max_read_bp);
		qual.reserve(max_read_bp);
	}
	
	string name;
	string seq;
	string alt_name;
	string qual;
	
	bool lengths_equal() { return seq.length() == qual.length(); }
	void clear() 
	{ 
		name.clear(); 
		seq.clear(); 
		qual.clear(); 
		alt_name.clear();
	}
};

void reverse_complement(string& seq);
string convert_color_to_bp(const string& color);
seqan::String<char> convert_color_to_bp(char base, const seqan::String<char>& color);

string convert_bp_to_color(const string& bp, bool remove_primer = false);
seqan::String<char> convert_bp_to_color(const seqan::String<char>& bp, bool remove_primer = false);

/*
  This is a dynamic programming to decode a colorspace read, which is from BWA paper.

  Heng Li and Richard Durbin
  Fast and accurate short read alignment with Burrows-Wheeler transform
 */
void BWA_decode(const string& color, const string& qual, const string& ref, string& decode);

  
template <class Type>
string DnaString_to_string(const Type& dnaString)
{
  std::string result;
  std::stringstream ss(std::stringstream::in | std::stringstream::out);
  ss << dnaString >> result;
  return result;
}

class ReadTable;

class FLineReader { //simple text line reader class, buffering last line read
  int len;
  int allocated;
  char* buf;
  bool isEOF;
  FILE* file;
  bool is_pipe;
  bool pushed; //pushed back
  int lcount; //counting all lines read by the object

 public:
  // daehwan - this is not a good place to store the last read ...
  Read last_read;
  bool pushed_read;
  
public:
  char* chars() { return buf; }
  char* line() { return buf; }
  int readcount() { return lcount; } //number of lines read
  int length() { return len; } //length of the last line read
  bool isEof() {return isEOF; }
  char* nextLine();
  FILE* fhandle() { return file; }
  void pushBack() { if (lcount>0) pushed=true; } // "undo" the last getLine request
           // so the next call will in fact return the same line
  void pushBack_read() { if(!last_read.name.empty()) pushed_read=true;}
  FLineReader(FILE* stream=NULL) {
    len=0;
    isEOF=false;
    is_pipe=false;
    allocated=512;
    buf=(char*)malloc(allocated);
    lcount=0;
    buf[0]=0;
    file=stream;
    pushed=false;
    pushed_read=false;
    }

  FLineReader(FZPipe& fzpipe) {
    len=0;
    isEOF=false;
    allocated=512;
    buf=(char*)malloc(allocated);
    lcount=0;
    buf[0]=0;
    file=fzpipe.file;
    is_pipe=!fzpipe.pipecmd.empty();
    pushed=false;
    pushed_read=false;
    }
  void close() {
    if (file==NULL) return;
    if (is_pipe) pclose(file);
            else fclose(file);
    }
  ~FLineReader() {
    free(buf); //does not call close() -- we might reuse the file handle
    }
};

bool get_read_from_stream(uint64_t insert_id,
			  FLineReader& fr,
			  ReadFormat reads_format,
			  bool strip_slash,
			  Read& read,
			  FILE* um_out=NULL, //unmapped reads output
			  bool um_write_found=false);

void skip_lines(FLineReader& fr);
bool next_fasta_record(FLineReader& fr, string& defline, string& seq, ReadFormat reads_format);
bool next_fastq_record(FLineReader& fr, const string& seq, string& alt_name, string& qual, ReadFormat reads_format);
bool next_fastx_read(FLineReader& fr, Read& read, ReadFormat reads_format=FASTQ,
                        FLineReader* frq=NULL);


class ReadStream {
  protected:
    struct ReadOrdering
    {
      bool operator()(std::pair<uint64_t, Read>& lhs, std::pair<uint64_t, Read>& rhs)
      {
        return (lhs.first > rhs.first);
      }
    };
    FZPipe fstream;
    std::priority_queue< std::pair<uint64_t, Read>,
         std::vector<std::pair<uint64_t, Read> >,
         ReadOrdering > read_pq;
    uint64_t last_id; //keep track of last requested ID, for consistency check
    bool r_eof;
    bool next_read(Read& read, ReadFormat read_format); //get top read from the queue

  public:
    ReadStream():fstream(), read_pq(), last_id(0), r_eof(false) {   }

    ReadStream(string& fname):fstream(fname, false),
       read_pq(), last_id(0), r_eof(false) {   }

    void init(string& fname) {
        fstream.openRead(fname, false);
        }
    const char* filename() {
        return fstream.filename.c_str();
        }
    //read_ids must ALWAYS be requested in increasing order
    bool getRead(uint64_t read_id, Read& read,
        ReadFormat read_format=FASTQ,
        bool strip_slash=false,
        FILE* um_out=NULL, //unmapped reads output
        bool um_write_found=false);

    void rewind() {
      fstream.rewind();
      clear();
      }
    FILE* file() {
      return fstream.file;
      }
    void clear() {
      /* while (read_pq.size()) {
        const std::pair<uint64_t, Read>& t = read_pq.top();
        //free(t.second);
        read_pq.pop();
        } */
      read_pq=std::priority_queue< std::pair<uint64_t, Read>,
          std::vector<std::pair<uint64_t, Read> >,
          ReadOrdering > ();
      }
    void close() {
      clear();
      fstream.close();
      }
    ~ReadStream() {
      close();
      }
};
#endif
