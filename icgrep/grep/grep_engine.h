
/*
 *  Copyright (c) 2018 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */
#ifndef GREP_ENGINE_H
#define GREP_ENGINE_H
#include <grep_interface.h>
#include <kernels/callback.h>
#include <cc/multiplex_CCs.h>
#include <string>
#include <vector>
#include <sstream>
#include <atomic>
#include <boost/filesystem.hpp>

namespace re { class CC; }
namespace re { class RE; }
namespace llvm { namespace cl { class OptionCategory; } }
namespace kernel { class ProgramBuilder; }
namespace kernel { class StreamSet; }
class BaseDriver;


namespace grep {
    
    
extern unsigned ByteCClimit;

enum class GrepRecordBreakKind {Null, LF, Unicode};

class InternalSearchEngine;

enum GrepSignal : unsigned {BinaryFile};

class GrepCallBackObject : public kernel::SignallingObject {
public:
    GrepCallBackObject() : SignallingObject(), mBinaryFile(false) {}
    virtual ~GrepCallBackObject() {}
    virtual void handle_signal(unsigned signal);
    bool binaryFileSignalled() {return mBinaryFile;}
private:
    bool mBinaryFile;
};

class MatchAccumulator : public GrepCallBackObject {
public:
    MatchAccumulator() {}
    virtual ~MatchAccumulator() {}
    virtual void accumulate_match(const size_t lineNum, char * line_start, char * line_end) = 0;
    virtual void report_matches(char * buffer_base, size_t coordsCount, size_t * coords);
    virtual void finalize_match(char * buffer_end) {}  // default: no op
};

extern "C" void accumulate_match_wrapper(intptr_t accum_addr, const size_t lineNum, char * line_start, char * line_end);

extern "C" void report_matches_wrapper(intptr_t accum_addr, char * buffer_base, size_t coordsCount, size_t * coords);

extern "C" void finalize_match_wrapper(intptr_t accum_addr, char * buffer_end);

class GrepEngine {
    enum class FileStatus {Pending, GrepComplete, PrintComplete};
    friend class InternalSearchEngine;
public:

    enum class EngineKind {QuietMode, MatchOnly, CountOnly, EmitMatches};

    GrepEngine(BaseDriver & driver);

    virtual ~GrepEngine() = 0;

    void setPreferMMap() {mPreferMMap = true;}

    void showFileNames() {mShowFileNames = true;}
    void setStdinLabel(std::string lbl) {mStdinLabel = lbl;}
    void showLineNumbers() {mShowLineNumbers = true;}
    void setInitialTab() {mInitialTab = true;}

    void setMaxCount(int m) {mMaxCount = m;}
    void setGrepStdIn() {mGrepStdIn = true;}
    void setInvertMatches() {mInvertMatches = true;}
    void setCaseInsensitive()  {mCaseInsensitive = true;}

    void suppressFileMessages() {mSuppressFileMessages = true;}
    void setBinaryFilesOption(argv::BinaryFilesMode mode) {mBinaryFilesMode = mode;}
    void setRecordBreak(GrepRecordBreakKind b);
    void initFileResult(const std::vector<boost::filesystem::path> & filenames);
    void initREs(std::vector<re::RE *> & REs);
    virtual void grepCodeGen();
    bool searchAllFiles();
    void * DoGrepThreadMethod();
    virtual void showResult(uint64_t grepResult, const std::string & fileName, std::ostringstream & strm);

protected:
    // Functional components that may be required for grep searches,
    // depending on search pattern, mode flags, external parameters and
    // implementation strategy.
    typedef uint32_t component_t;
    enum class Component : component_t {
        NoComponents = 0,
        MoveMatchesToEOL = 0x01,
        GraphemeClusterBoundary = 0x02
    };
    bool hasComponent(Component compon_set, Component c);
    void setComponent(Component & compon_set, Component c);

    std::pair<kernel::StreamSet *, kernel::StreamSet *> grepPipeline(const std::unique_ptr<kernel::ProgramBuilder> &P,
                                                                     kernel::StreamSet * ByteStream);

    virtual uint64_t doGrep(const std::string & fileName, std::ostringstream & strm);
    int32_t openFile(const std::string & fileName, std::ostringstream & msgstrm);
    std::string linePrefix(std::string fileName);

protected:

    EngineKind mEngineKind;
    bool mSuppressFileMessages;
    argv::BinaryFilesMode mBinaryFilesMode;
    bool mPreferMMap;
    bool mShowFileNames;
    std::string mStdinLabel;
    bool mShowLineNumbers;
    bool mInitialTab;
    bool mCaseInsensitive;
    bool mInvertMatches;
    int mMaxCount;
    bool mGrepStdIn;
    BaseDriver & mGrepDriver;
    void * mMainMethod;

    std::atomic<unsigned> mNextFileToGrep;
    std::atomic<unsigned> mNextFileToPrint;
    std::vector<boost::filesystem::path> inputPaths;
    std::vector<std::ostringstream> mResultStrs;
    std::vector<FileStatus> mFileStatus;
    bool grepMatchFound;
    GrepRecordBreakKind mGrepRecordBreak;

    std::vector<re:: RE *> mREs;
    std::set<re::Name *> mUnicodeProperties;
    re::CC * mBreakCC;
    std::string mFileSuffix;
    Component mRequiredComponents;
    bool mMoveMatchesToEOL;
    pthread_t mEngineThread;
};


//
// The EmitMatches engine uses an EmitMatchesAccumulator object to concatenate together
// matched lines.

class EmitMatch : public MatchAccumulator {
    friend class EmitMatchesEngine;
public:
    EmitMatch(std::string linePrefix, bool showLineNumbers, bool initialTab, std::ostringstream & strm)
        : mLinePrefix(linePrefix),
        mShowLineNumbers(showLineNumbers),
        mShowByteNumbers(false),
        mInitialTab(initialTab),
        mLineCount(0),
        mTerminated(true),
        mResultStr(strm) {}
    void accumulate_match(const size_t lineNum, char * line_start, char * line_end) override;
    void report_matches(char * buffer_base, size_t coordsCount, size_t * coords) override;
    void finalize_match(char * buffer_end) override;
protected:
    std::string mLinePrefix;
    bool mShowLineNumbers;
    bool mShowByteNumbers;
    bool mInitialTab;
    size_t mLineCount;
    bool mTerminated;
    std::ostringstream & mResultStr;
};

class EmitMatchesEngine final : public GrepEngine {
public:
    EmitMatchesEngine(BaseDriver & driver);
    void grepCodeGen() override;
private:
    uint64_t doGrep(const std::string & fileName, std::ostringstream & strm) override;
};

class CountOnlyEngine final : public GrepEngine {
public:
    CountOnlyEngine(BaseDriver & driver);
private:
    void showResult(uint64_t grepResult, const std::string & fileName, std::ostringstream & strm) override;
};

class MatchOnlyEngine final : public GrepEngine {
public:
    MatchOnlyEngine(BaseDriver & driver, bool showFilesWithoutMatch, bool useNullSeparators);
private:
    void showResult(uint64_t grepResult, const std::string & fileName, std::ostringstream & strm) override;
    unsigned mRequiredCount;
};

class QuietModeEngine final : public GrepEngine {
public:
    QuietModeEngine(BaseDriver & driver);
};



class InternalSearchEngine {
public:
    InternalSearchEngine(BaseDriver & driver);

    InternalSearchEngine(const std::unique_ptr<grep::GrepEngine> & engine);

    ~InternalSearchEngine();

    void setRecordBreak(GrepRecordBreakKind b) {mGrepRecordBreak = b;}
    void setCaseInsensitive()  {mCaseInsensitive = true;}

    void grepCodeGen(re::RE * matchingRE, re::RE * invertedRE);

    void doGrep(const char * search_buffer, size_t bufferLength, MatchAccumulator & accum);

private:
    GrepRecordBreakKind mGrepRecordBreak;
    bool mCaseInsensitive;
    bool mSaveSegmentPipelineParallel;
    BaseDriver & mGrepDriver;
    void * mMainMethod;
};

}

#endif
