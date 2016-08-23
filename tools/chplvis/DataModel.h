/*
 * Copyright 2016 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DATAMODEL_H
#define DATAMODEL_H

#include "Event.h"
#include <list>
#include <vector>
#include <map>
#include "StringCache.h"

// This class builds a list of events 
//   Start, Stop, Pause, and Tag events are grouped together 
//   Other events are placed in a time sorted group between
//   the grouped events.

// This is the class that reads the files as generated by runtime/src/chpl-visual-debug.c
// in the Chapel runtime.
//
// Initially the data files are in ascii.  To turn these into binary data files,
// both this class and runtime/src/chpl-visual-debug.c need to be modified to
// write (chpl-visual-debug.c) and read (this class) in binary.

// Support Structs used by DataModel

// Used to track communication,  each tag has a 2D array of commData, one for each direction
struct commData {
  long numComms;
  long numGets;
  long numPuts;
  long numForks;
  long commSize;
  
  commData() :  numComms(0), numGets(0), numPuts(0), numForks(0), commSize(0) {};
};

// Used to track tasks,  each tag/locale has a map of taskData
struct taskData {
  E_task *taskRec;
  E_begin_task *beginRec;
  E_end_task *endRec;
  int endTagNo;
  double taskClock;
  std::list<Event *>commList;
  commData commSum;

  taskData() : taskRec(NULL), beginRec(NULL), endRec(NULL), endTagNo(-2) {};
};

// Used to track a locale,  each tag has an array of locales
struct localeData {
  double userCpu;
  double sysCpu;
  double Cpu;
  double refUserCpu;
  double refSysCpu;
  double clockTime;
  double refTime;
  double maxTaskClock;
  long    numTasks;
  long    runConc;
  long    maxConc;

  std::map<long,taskData> tasks;

  localeData() : userCpu(0), sysCpu(0), Cpu(0), refUserCpu(0), refSysCpu(0),
                 clockTime(0), refTime(0), maxTaskClock(0), // minTaskClock(1E10),
                 numTasks(0), runConc(0), maxConc(0) {};
};

// File names, rel2Home says the names starts with $CHPL_HOME
typedef struct filename {
  char * name;
  bool rel2Home;
} filename;

// Function names
typedef struct funcname {
  char *name;
  int  fileNo;
  int  lineNo;
  std::list<Event *> func_events;
  int  noOnTasks;
  int  noTasks;
  int  noGets;
  int  noPuts;
} funcname;


// Primary data structure built by reading the data files dumped by using VisualDebug.chpl

class DataModel {

 public:

  // Forward declaration
  struct tagData;

  // Task Event (begin, end) timeline for task concurrency,
  //     one linear structure per locale
  //     long => task number or tag number
  enum Tl_Kind { Tl_Tag, Tl_Begin, Tl_End };
  typedef std::pair < Tl_Kind, long > timelineEntry;
  std::list < timelineEntry > *taskTimeline;

 private:

  int      mainTID;
  taskData mainTask;

  StringCache strDB;
  filename *fileTbl;
  int fileTblSize;

  funcname *funcTbl;
  int funcTblSize;
  
  std::vector<const char *> tagNames;

  typedef std::list<Event*>::iterator evItr;

  int numLocales;
  int numTags;
  
  // Includes entries for -2 (TagAll), and -1 (TagStart->0),  size is numTags+2
  tagData **tagList;    // 1D array of pointers to tagData

  // Unique named tags ... if names repeat
  bool uniqueTags;
  std::map<const char *, int> name2tag;
  tagData **utagList;

  std::list < Event* > theEvents;
  std::list < Event* >::iterator curEvent;
  
  // Utility routines
  
  int LoadFile (const char *filename, int index, double seq);
  
  void newList ();
  
 public:
  
  // Tag array has two extra entries, one for "All" and
  // one for Start (startVdebug() call) to the first tag.
  // Tags are numbered 0 to numTags-1
  
  static const int TagALL = -2, TagStart = -1;
  
  // Tags and manipulation of them
  
  struct tagData { // a class with public elements the default
    friend class DataModel;
    const long numLocales;
    const char *name;
    localeData *locales;
    commData **comms;
    
    // Local Maxes and Minimums
    double maxCpu;
    double maxClock;
    long   maxTasks;
    long   maxConc;
    long   maxComms;
    long   maxSize;
    
    tagData(long numLoc) : numLocales(numLoc), name(""),  maxCpu(0), maxClock(0),
                           maxTasks(0),  maxConc(0), maxComms(0), maxSize(0) {
      locales = new localeData[numLocales];
      comms = new  commData * [numLocales];
      for (int i = 0; i < numLocales; i++ )
        comms[i] = new commData[numLocales];
    }
    
    ~tagData() {
      delete [] locales;
      locales = NULL;
      for (int i = 0; i < numLocales; i++) {
        delete [] comms[i];
        comms[i] = NULL;
      }
      delete [] comms;
      comms = NULL;
    }

    private:
      evItr firstTag;
  };
  // End of struct tagData

  // DataModel methods

  bool hasUniqueTags () { return uniqueTags; }
  
  int NumTags () { return numTags; }

  int NumUTags () { return name2tag.size(); }
  
  tagData * getTagData(int tagno) {
    if (tagno < TagALL || tagno >= numTags) 
      return NULL;
    return tagList[tagno-TagALL];
  }

  tagData * getUTagData(int tagno) {
    if (tagno == TagALL)
      return tagList[0];
    if (tagno == TagStart)
      return tagList[1];
    if (uniqueTags || tagno >= (int)name2tag.size() || tagno < 0)
      return NULL;
    return utagList[tagno];
  }

  taskData * getTaskData (long locale, long taskId, long tagNo = TagALL);

  double start_clock() {
    return (*theEvents.begin())->clock_time();
  }

  const char *fileName (long fileNo) {
    return (fileNo >= 0 && fileNo < fileTblSize) ?
      fileTbl[fileNo].name : "<unknown>";
  }

  bool fileIsRel2Home (long fileNo) {
    return (fileNo >= 0 && fileNo < fileTblSize) ?
      fileTbl[fileNo].rel2Home : false ; 
  }
  
  // Constructor for DataModel
  
  DataModel() {
    numLocales = -1;
    numTags = 0;
    tagList = NULL;
    taskTimeline = NULL;
    curEvent = theEvents.begin();
    uniqueTags = true;
    utagList = NULL;
    tagNames.resize(64);
  }
  
  // Destructor for DataModel
  ~DataModel() {
    if (tagList != NULL) {
      for (int ix = 0; ix < numTags + 2; ix++)
        delete tagList[ix];
      delete [] tagList;
    }
    if (taskTimeline != NULL)
      delete [] taskTimeline;
    if (utagList != NULL) {
      for (unsigned int ix = 0; ix < name2tag.size(); ix++)
        delete utagList[ix];
      delete [] utagList;
    }
  }
  
  //  LoadData loads data from a collection of files
  //  filename of the form  basename-n, where n can
  //  be a multi-digit number
  //  Returns 1 if successful, 0 if not
  
  int LoadData (const char *filename, bool fromArgv);
  
  //  Number of locales found in loading files
  
  int NumLocales () { return numLocales; }
  
};

#endif
