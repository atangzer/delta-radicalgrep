#include <kernel/core/idisa_target.h>
#include <kernel/core/kernel_builder.h>
#include <kernel/core/streamset.h>
#include <kernel/pipeline/pipeline_builder.h>
#include <kernel/pipeline/driver/cpudriver.h>
#include <kernel/streamutils/deletion.h>
#include <kernel/streamutils/pdep_kernel.h>
#include <kernel/streamutils/stream_select.h>
#include <kernel/streamutils/stream_shift.h>
#include <kernel/unicode/UCD_property_kernel.h>
#include <kernel/basis/s2p_kernel.h>
#include <kernel/basis/p2s_kernel.h>
#include <kernel/io/source_kernel.h>
#include <kernel/io/stdout_kernel.h>
#include <kernel/scan/scanmatchgen.h>
#include <boost/filesystem.hpp>
#include <re/cc/cc_compiler.h>
#include <re/cc/cc_compiler_target.h>
#include <re/adt/adt.h>
#include <re/parse/parser.h>
#include <re/unicode/re_name_resolve.h>
#include <re/cc/cc_kernel.h>
#include <re/ucd/ucd_compiler.hpp>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/ErrorHandling.h>
#include <pablo/pablo_kernel.h>
#include <pablo/builder.hpp>
#include <pablo/pe_zeroes.h>
#include <pablo/pe_ones.h>
#include <pablo/bixnum/bixnum.h>
#include <toolchain/pablo_toolchain.h>
#include <grep/grep_kernel.h>
#include <toolchain/toolchain.h>
#include <fileselect/file_select.h>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <map>
#include <cstdio>


//#include <re/adt/re_re.h>
//#include <re/adt/re_name.h>
//unused in adapted version of code


namespace fs = boost::filesystem;

using namespace llvm;
using namespace codegen;
using namespace kernel;
using namespace pablo;


//  Given a Unicode character class (set of Unicode characters), ucount
//  counts the number of occurrences of characters in that class within
//  given input files.

static cl::OptionCategory ucFlags("Command Flags", "ucount options");

//modified: taken out cl::opt

//  Multiple input files are allowed on the command line; counts are produced
//  for each file.
static cl::list<std::string> inputFiles(cl::Positional, cl::desc("<input file ...>"), cl::CommaSeparated, cl::cat(ucFlags));//modified cl::OneOrMore to CommaSeparated

std::vector<fs::path> allFiles;


//
//  The Hexify Kernel is the logic that produces hexadecimal output
//  from a source bit stream set called spreadBasis and the insertMask
//  used to spread out the bits.
//

//  Given a Unicode character class (set of Unicode characters), ucount
//  counts the number of occurrences of characters in that class within
//  given input files.

static cl::OptionCategory ucFlags("Command Flags", "ucount options");

//modified: taken out cl::opt

//  Multiple input files are allowed on the command line; counts are produced
//  for each file.
static cl::list<std::string> inputFiles(cl::Positional, cl::desc("<input file ...>"), cl::CommaSeparated, cl::cat(ucFlags));//modified cl::OneOrMore to CommaSeparated

std::vector<fs::path> allFiles;


//
//  The Hexify Kernel is the logic that produces hexadecimal output
//  from a source bit stream set called spreadBasis and the insertMask
//  used to spread out the bits.
//
class CSV_parsing : public PabloKernel {
public:
    CSV_parsing(BuilderRef kb,  StreamSet * dquote, StreamSet * delimiter, StreamSet * cr,StreamSet * FieldStrats, StreamSet * FieldFollows, StreamSet * RecordStarts, StreamSet * RecordFollows, StreamSet * basis, StreamSet * target, Scalar * FieldNumber, Scalar * RecordNumber)   //modified
        : PabloKernel(kb, "CSV_parsing",
                      {Binding{"dquote",dquote,FixedRate(),LookAhead(1)}},
                      {Binding{"delimiter",dilimiter, FixedRate(), LookAhead(1)}},
                      {Binding{"cr",cr,FixedRate(),LookAhead(1)}}
                      {Binding{"FieldStrats", FieldStarts},//don't know what fixedrate() is used for
                      {Binding{"FieldFollows",FieldFollows}},
                      {Binding{"RecordStarts",RecordStarts}, Binding{"RecordFollows",RecordFollows},Binding{"basis",basis},Binding{"target",target}},///modified 
                      {},
                      {},
                      {Binding{"FieldNumber",FieldNumber},Binding{"RecordNumber", RecordNumber}}) {}
protected:
    void generatePabloMethod() override;
};


/* from WIKI~
In this step, CSV syntax marks are removed from the input, to leave the raw CSV data only.

1.Double quotes surrounding fields are deleted.In general, double quotes will be added back during the expansion step (because they are in the expansion templates).
2.The quote_escape characters are deleted, but the escaped_quote characters are kept.
3.Commas are deleted.
4.CR before LF should be deleted to uniformly standardize on LF as line separator.
*/
/* And in this program, we actually delete
  1. double quotes surrounding fields which can be detected by start_dquote [or] end_dquote 
  2. quote_escape and keep escaped_quote
  3. realDelimiter
  4. cr(carriage return)   
*/
void CSV_parsing::generatePabloMethod(){
    pablo::PabloBuilder pb(getEntryScope());
    std::vector<PabloAST *> basis = getInputStreamSet("basis");
    //delimiter is used to delimit fields, such as commas
    PabloAST * delimiter = getInputStreamSet("delimiter")[0];
    PabloAST * dquote = getInputStreamSet("dquote")[0];
    PabloAST * cr = getInputStreamSet("cr")[0];
    PabloAST * target = getInputStreamSet("target")[0];
    
    PabloAST * dquote_odd = pb.createEveryNth(dquote, pb.getInteger(2));
    PabloAST * dquote_even = pb.createXor(dquote, dquote_odd);
    PabloAST * quote_escape = pb.createAnd(dquote_even, pb.createLookahead(dquote, 1));
    PabloAST * escaped_quote = pb.createAdvance(quote_escape, 1);

    PabloAST * start_dquote = pb.createXor(dquote_odd, escaped_quote);
    PabloAST * end_dquote = pb.createXor(dquote_even, quote_escape);
    //dquote_surrounding_fields
    PabloAST * dquote_sf = pb.createOr(start_dquote,end_quote);

    PabloAST * literal_delimiter = pb.createAnd(delimiter,pb.createIntrinsicCall(pablo::Intrinsic::InclusiveSpan, {start_dquote, end_dquote}));
    PabloAST * RealDelimiter = pb.createXor(delimiter,literal_delimiter);

    PabloAST * literal_target = pb.createAnd(target,pb.createIntrinsicCall(pablo::Intrinsic::InclusiveSpan, {start_dquote, end_dquote}));
    PabloAST * CSV_newlines = pb.createXor(target,literal_target);
    PabloAST * newline_count=pb.createCount(CSV_newlines);

    PabloAST * ToBeDeletion = pb.creatOr3(dquote_sf,quote_eacape,realDelimiter);
    ToBeDeletion = pb.createOr(ToBeDelition,cr);

}

